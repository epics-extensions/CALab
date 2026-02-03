#include "globals.h"
#include "TimeoutUniqueLock.h"
#include <cstdlib>
#include <string>
#include <cstring>
#include <unordered_map>
#include <cctype>

// Cross-platform helper to set process environment variables
static inline void SetEnvCompat(const char* name, const char* value) {
#if defined(_WIN32) || defined(_WIN64)
	// Set for C runtime and Windows process environment
	_putenv_s(name, value);
	SetEnvironmentVariableA(name, value);
#else
	// Fallback that avoids relying on setenv being declared on all libstdc++ variants
	static std::unordered_map<std::string, std::string> s_envStore;
	std::string pair = std::string(name) + "=" + value;
	// store persistent buffer to satisfy putenv lifetime requirements
	s_envStore[name] = std::move(pair);
	::putenv(s_envStore[name].data());
#endif
}

namespace {
static void CaLabExceptionHandler([[maybe_unused]] struct exception_handler_args args) {
	// Intentionally suppress CA exception output (user opts in via env var).
}

static void InstallCaExceptionHandlerIfRequested() {
	const char* suppress = std::getenv("CALAB_CA_SUPPRESS_EXCEPTIONS");
	if (!suppress || !*suppress) {
		return;
	}
#if defined(_WIN32) || defined(_WIN64)
	ca_add_exception_event(CaLabExceptionHandler, nullptr);
#else
	if (ca_add_exception_event) {
		ca_add_exception_event(CaLabExceptionHandler, nullptr);
	}
#endif
}
} // namespace

Globals::Globals() {
	// Check for environment variable to enable a specific polling mode for CaLab
	if (getenv("CALAB_POLLING")) {
		bCaLabPolling = true;
	}
	else {
		bCaLabPolling = false;
	}
	// Set up a debug file if the CALAB_NODBG environment variable is defined
	const char* tmp = getenv("CALAB_NODBG");
	if (tmp) {
		if (strlen(tmp) > 3) {
			pCaLabDbgFile = fopen(tmp, "w");
			if (!pCaLabDbgFile) {
				perror("Error opening file");
				CaLabDbgPrintf("Error: Could not open file %s", tmp);
			}
		}
	}
	// Configure faster Channel Access search/connection timings before CA context init.
	// Allow overrides via CALAB_CA_CONN_TMO / CALAB_CA_MAX_SEARCH_PERIOD; otherwise set safe defaults.
	const char* caConnTmo = std::getenv("EPICS_CA_CONN_TMO");
	const char* caMaxSearch = std::getenv("EPICS_CA_MAX_SEARCH_PERIOD");

	if (!caConnTmo || !*caConnTmo) {
		// Default: reduce initial search timeout from ~30s to 3s.
		SetEnvCompat("EPICS_CA_CONN_TMO", "3");
	}

	if (!caMaxSearch || !*caMaxSearch) {
		// Default: cap exponential backoff to 60s (instead of 300s).
		SetEnvCompat("EPICS_CA_MAX_SEARCH_PERIOD", "60");
	}
	pendingCallbacks.store(0);
	stopped.store(false);
	// Initialize the EPICS Channel Access context with preemptive callbacks enabled.
	if (ca_context_create(ca_enable_preemptive_callback) == ECA_NORMAL) {
		pcac = ca_current_context();
		ca_attach_context(pcac);
		InstallCaExceptionHandlerIfRequested();
		// Start a background thread to poll for Channel Access events.
		pollThread_ = std::thread([this]() {
			int status = ca_attach_context(this->pcac);
			if (status != ECA_NORMAL) {
				CaLabDbgPrintf("Polling thread: Error when attaching the CA context: %s",
					ca_message_safe(status));
				return;
			}
			while (!stopped.load()) {
				// Process pending CA events and flush I/O buffers.
				ca_poll();
				ca_flush_io();
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			ca_detach_context();
			});
	}
	else {
		CaLabDbgPrintf("Globals: Error during initialization of the CA context.");
		pcac = nullptr;
	}
}

Globals::~Globals() {
	// Signal the polling thread and other loops to terminate.
	stopped.store(true);
	// Stop registered background workers first to minimize callbacks during teardown.
	stopBackgroundWorkers();
	if (pollThread_.joinable()) {
		pollThread_.join();
	}
	// A fixed delay to allow pending CA operations to complete. This is a fallback.
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	if (pCaLabDbgFile) {
		fclose(pCaLabDbgFile);
		pCaLabDbgFile = nullptr;
	}
	pvRegistry.clear();
}

void Globals::addPendingConnection(PVItem* item) {
	std::lock_guard<std::mutex> lock(pendingConnectionsMutex);
	pendingConnections.insert(item);
}

void Globals::removePendingConnection(PVItem* item) {
	std::lock_guard<std::mutex> lock(pendingConnectionsMutex);
	pendingConnections.erase(item);
}

void Globals::clearPendingConnections() {
	std::lock_guard<std::mutex> lock(pendingConnectionsMutex);
	pendingConnections.clear();
}

// Retrieves a snapshot of the current pending connections.
std::vector<PVItem*> Globals::getPendingConnections() const {
	std::lock_guard<std::mutex> lock(pendingConnectionsMutex);
	return std::vector<PVItem*>(pendingConnections.begin(), pendingConnections.end());
}

void Globals::waitForNotification(std::chrono::milliseconds timeout) {
	std::unique_lock<std::mutex> lock(notificationMutex);
	notificationCv.wait_for(lock, timeout, [this]() {
		if (stopped.load()) {
			return true;
		}
		std::lock_guard<std::mutex> pendingLock(pendingConnectionsMutex);
		return !pendingConnections.empty();
	});
}


void Globals::notify() {
	std::lock_guard<std::mutex> lock(notificationMutex);
	notificationCv.notify_all();
}

void Globals::registerArrayToInstance(void* arrayPtr, InstanceDataPtr* instance) {
	TimeoutUniqueLock<std::shared_timed_mutex> lock(pvRegistryLock, "~Globals", std::chrono::seconds(1));
	if (lock.isLocked()) {
		arrayToInstanceMap[arrayPtr] = instance;
	}
	else {
		DbgPrintf("Error: Failed to acquire lock in registerArrayToInstance.");
	}
}

InstanceDataPtr* Globals::getInstanceForArray(void* arrayPtr) {
	TimeoutSharedLock<std::shared_timed_mutex> lock(pvRegistryLock, "getInstanceForArray",
		std::chrono::milliseconds(200));
	if (!lock.isLocked()) {
		CaLabDbgPrintf("Error: Failed to acquire lock in getInstanceForArray.");
		return nullptr;
	}
	auto it = arrayToInstanceMap.find(arrayPtr);
	return (it != arrayToInstanceMap.end()) ? it->second : nullptr;
}

void Globals::registerBackgroundWorker(const std::string& name, std::function<void()> stopFn) {
	std::lock_guard<std::mutex> lk(workersMutex_);
	// Replace existing entry with same name.
	for (auto& p : backgroundWorkers_) {
		if (p.first == name) { p.second = std::move(stopFn); return; }
	}
	backgroundWorkers_.emplace_back(name, std::move(stopFn));
}

void Globals::stopBackgroundWorkers() {
	std::vector<std::function<void()>> toStop;
	{
		std::lock_guard<std::mutex> lk(workersMutex_);
		for (auto& p : backgroundWorkers_) {
			if (p.second) toStop.emplace_back(p.second);
		}
		backgroundWorkers_.clear();
	}
	// Stop outside the lock to prevent potential deadlocks if a stop function
	// tries to interact with the Globals instance again.
	for (auto& fn : toStop) {
		fn();
	}
}

// Checks if a field name is one of the standard fields common to all EPICS records.
bool Globals::recordFieldIsCommonField(const std::string& fieldName) {
	// Standard fields that exist in all EPICS records
	static const std::unordered_set<std::string> commonFields = {
		"NAME", "DESC", "ASG", "SCAN", "PINI", "PHAS", "EVNT", "TSE", "TSEL", "DTYP",
		"DISV", "DISA", "SDIS", "MLOK", "MLIS", "DISP", "PROC", "STAT", "SEVR", "NSTA",
		"NSEV", "ACKS", "ACKT", "DISS", "LCNT", "PACT", "PUTF", "RPRO", "PRIO", "TPRO",
		"BKPT", "UDF", "UDFS", "TIME", "FLNK", "RTYP", "VAL"
	};
	return (commonFields.find(fieldName) != commonFields.end());
}

/**
 * @brief Checks whether a field name exists for a specific EPICS record type.
 *
 * This function validates if the specified field is valid for the given EPICS record type
 * by checking against a comprehensive, hardcoded list of fields derived from EPICS base
 * and common modules. It includes a cache for performance.
 *
 * @param recordTypeStr The EPICS record type (e.g., "ai", "stringin", "calc").
 * @param fieldName The field name to be checked.
 * @return bool True if the field exists for the record type, otherwise false.
 */
bool Globals::recordFieldExists(const std::string& recordTypeStr, const std::string& fieldName) {
	// Normalize inputs: strip embedded NULs/padding, trim whitespace, and fix case.
	auto sanitize = [](std::string s) {
		// Cut at first NUL if present.
		size_t pos0 = s.find('\0');
		if (pos0 != std::string::npos) s.resize(pos0);
		// Trim whitespace from both ends.
		auto isws = [](unsigned char c) { return std::isspace(c) != 0; };
		size_t start = 0;
		while (start < s.size() && isws(static_cast<unsigned char>(s[start]))) ++start;
		size_t end = s.size();
		while (end > start && isws(static_cast<unsigned char>(s[end - 1]))) --end;
		if (start > 0 || end < s.size()) s = s.substr(start, end - start);
		return s;
		};

	std::string rec = sanitize(recordTypeStr);
	std::string fld = sanitize(fieldName);
	// EPICS record types are conventionally lowercase, fields uppercase.
	for (auto& c : rec) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	for (auto& c : fld) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

	if (recordFieldIsCommonField(fld)) {
		return true;
	}

	// Record-specific fields based on the EPICS headers from epics-base and sscan module.
	static std::unordered_map<std::string, std::unordered_set<std::string>> recordSpecificFields = {
		{"ai", {
			"ADEL", "AOFF", "ASLO", "EGU", "EGUF", "EGUL", "HIGH", "HIHI", "HOPR",
			"HHSV", "HSV", "HYST", "INP", "LINR", "LOLO", "LOPR", "LOW", "LLSV",
			"LSV", "MDEL", "PREC", "RVAL", "SIML", "SIMS", "SIOL", "SMOO", "SVAL"
		}},
		{"aai", {
			"BPTR", "EGU", "FTVL", "HIGH", "HIHI", "HOPR", "HHSV", "HSV", "HYST",
			"INP", "LOLO", "LOPR", "LOW", "LLSV", "LSV", "MDEL", "NELM", "NORD",
			"PREC", "SIML", "SIMS", "SIOL", "SVAL"
		}},
		{"ao", {
			"ADEL", "DOL", "DRVH", "DRVL", "EGU", "EGUF", "EGUL", "HIGH", "HIHI",
			"HOPR", "HHSV", "HSV", "HYST", "INIT", "IVOA", "IVOV", "LBRK", "LINR",
			"LOLO", "LOPR", "LOW", "LLSV", "LSV", "MDEL", "OIF", "OMSL", "ORAW",
			"ORBV", "OUT", "PBRK", "PREC", "RBV", "RVAL", "SIML", "SIMS", "SIOL",
			"SVAL"
		}},
		{"aao", {
			"BPTR", "DOL", "EGU", "FTVL", "HIGH", "HIHI", "HOPR", "HHSV", "HSV",
			"HYST", "IVOA", "IVOV", "LOLO", "LOPR", "LOW", "LLSV", "LSV", "MDEL",
			"NELM", "NORD", "OMSL", "OUT", "PREC", "SIML", "SIMS", "SIOL", "SVAL"
		}},
		{"bi", {
			"COSV", "INP", "MASK", "ONAM", "OSV", "RVAL", "SIML", "SIMS", "SIOL",
			"SVAL", "ZNAM", "ZSV"
		}},
		{"bo", {
			"COSV", "DOL", "HIGH", "IVOA", "IVOV", "MASK", "OMSL", "ONAM", "OSV",
			"OUT", "RBV", "RVAL", "SIML", "SIMS", "SIOL", "SVAL", "ZNAM", "ZSV"
		}},
		{"mbbi", {
			"COSV", "EISV", "EIST", "EIVL", "ELSV", "ELST", "ELVL", "FFSV", "FFST",
			"FFVL", "FRSV", "FRST", "FRVL", "FTSV", "FTST", "FTVL", "FVSV", "FVST",
			"FVVL", "INP", "MASK", "NISV", "NIST", "NIVL", "NOBT", "ONSV", "ONST",
			"ONVL", "RVAL", "SHFT", "SIML", "SIMS", "SIOL", "SVAL", "SVSV", "SVST",
			"SVVL", "SXSV", "SXST", "SXVL", "TESV", "TEST", "TEVL", "THSV", "THST",
			"THVL", "TTSV", "TTST", "TTVL", "TVSV", "TVST", "TVVL", "TWSV", "TWST",
			"TWVL", "ZRSV", "ZRST", "ZRVL"
		}},
		{"mbbo", {
			"COSV", "DOL", "EISV", "EIST", "EIVL", "ELSV", "ELST", "ELVL", "FFSV",
			"FFST", "FFVL", "FRSV", "FRST", "FRVL", "FTSV", "FTST", "FTVL", "FVSV",
			"FVST", "FVVL", "IVOA", "IVOV", "MASK", "NISV", "NIST", "NIVL", "NOBT",
			"OMSL", "ONSV", "ONST", "ONVL", "ORBV", "OUT", "RBV", "RVAL", "SHFT",
			"SVSV", "SVST", "SVVL", "SXSV", "SXST", "SXVL", "TESV", "TEST", "TEVL",
			"THSV", "THST", "THVL", "TTSV", "TTST", "TTVL", "TVSV", "TVST", "TVVL",
			"TWSV", "TWST", "TWVL", "ZRSV", "ZRST", "ZRVL"
		}},
		{"stringin", {
			"INP", "OVAL", "SIML", "SIMS", "SIOL", "SVAL"
		}},
		{"stringout", {
			"DOL", "IVOA", "IVOV", "OMSL", "OUT", "OVAL", "SIML", "SIMS", "SIOL", "SVAL"
		}},
		{"longin", {
			"EGU", "HIGH", "HIHI", "HOPR", "HHSV", "HSV", "HYST", "INP", "LOLO",
			"LOPR", "LOW", "LLSV", "LSV", "MDEL", "SIML", "SIMS", "SIOL", "SVAL"
		}},
		{"longout", {
			"DOL", "DRVH", "DRVL", "EGU", "HIGH", "HIHI", "HOPR", "HHSV", "HSV",
			"IVOA", "IVOV", "LOLO", "LOPR", "LOW", "LLSV", "LSV", "OMSL", "OUT"
		}},
		{"calc", {
			"A", "AA", "ADEL", "B", "BB", "C", "CC", "CALC", "D", "DD", "DLYA",
			"DOPT", "E", "EE", "EGU", "F", "FF", "G", "GG", "H", "HH", "HIGH",
			"HIHI", "HOPR", "HHSV", "HSV", "HYST", "I", "II", "INPA", "INPB",
			"INPC", "INPD", "INPE", "INPF", "INPG", "INPH", "INPI", "INPJ",
			"INPK", "INPL", "J", "JJ", "K", "KK", "L", "LL", "LOLO", "LOPR",
			"LOW", "LLSV", "LSV", "MDEL", "OCAL", "ODLY", "OOPT", "OVAL", "PREC", "WAIT"
		}},
		{"calcout", {
			"CALC", "DLYA", "DOL", "DOPT", "EGU", "HIGH", "HIHI", "HOPR", "HHSV",
			"HSV", "INPA", "INPB", "INPC", "INPD", "INPE", "INPF", "INPG", "INPH",
			"INPI", "INPJ", "INPK", "INPL", "IVOA", "IVOV", "LOLO", "LOPR", "LOW",
			"LLSV", "LSV", "OCAL", "OOPT", "OUT", "OVAL", "PREC", "WAIT"
		}},
		{"waveform", {
			"BPTR", "EGU", "FTVL", "HIGH", "HIHI", "HOPR", "HHSV", "HSV", "HYST",
			"INP", "LOLO", "LOPR", "LOW", "LLSV", "LSV", "NELM", "NORD", "PREC",
			"RARM", "RMOD", "SIML", "SIMS", "SIOL", "SVAL"
		}},
		{"sseq", {
			"ABRT", "BUSY", "DLY1", "DLY2", "DLY3", "DLY4", "DLY5", "DLY6", "DLY7",
			"DLY8", "DLY9", "DLYA", "DO1", "DO2", "DO3", "DO4", "DO5", "DO6", "DO7",
			"DO8", "DO9", "DOA", "DOL1", "DOL2", "DOL3", "DOL4", "DOL5", "DOL6",
			"DOL7", "DOL8", "DOL9", "DOLA", "LNK1", "LNK2", "LNK3", "LNK4", "LNK5",
			"LNK6", "LNK7", "LNK8", "LNK9", "LNKA", "PAUS", "PREC", "SELL", "SELM",
			"STR1", "STR2", "STR3", "STR4", "STR5", "STR6", "STR7", "STR8", "STR9",
			"STRA", "VERS"
		}},
		{"sscan", { // Fields from sscan module, may vary by version
			"ACQM", "ACQS", "ACQT", "AERY", "AQR", "ATIME", "AWCT", "BSPV",
			"BSWAIT", "BUSY", "CMND", "CPT", "CTIME", "DATA", "DDLY", "DIM",
			"D01PV", "D02PV", "D03PV", "D04PV", "D05PV", "D06PV", "D07PV", "D08PV", "D09PV", "D10PV",
			"D11PV", "D12PV", "D13PV", "D14PV", "D15PV", "D16PV", "D17PV", "D18PV", "D19PV", "D20PV",
			"D21PV", "D22PV", "D23PV", "D24PV", "D25PV", "D26PV", "D27PV", "D28PV", "D29PV", "D30PV",
			"D31PV", "D32PV", "D33PV", "D34PV", "D35PV", "D36PV", "D37PV", "D38PV", "D39PV", "D40PV",
			"D41PV", "D42PV", "D43PV", "D44PV", "D45PV", "D46PV", "D47PV", "D48PV", "D49PV", "D50PV",
			"D51PV", "D52PV", "D53PV", "D54PV", "D55PV", "D56PV", "D57PV", "D58PV", "D59PV", "D60PV",
			"D61PV", "D62PV", "D63PV", "D64PV", "D65PV", "D66PV", "D67PV", "D68PV", "D69PV", "D70PV",
			"D71PV", "D72PV", "D73PV", "D74PV", "D75PV", "EXSC", "FFL",
			"FFO", "FNAM", "FNUM", "FPTS", "MPTS", "NDATTR", "NPTS", "P1AR",
			"P1CR", "P1CV", "P1EP", "P1HR", "P1LR", "P1LV", "P1NP", "P1NV",
			"P1PA", "P1PV", "P1RA", "P1SI", "P1SM", "P1SP", "P1WD", "P2AR",
			"P2CR", "P2CV", "P2EP", "P2HR", "P2LR", "P2LV", "P2NP", "P2NV",
			"P2PA", "P2PV", "P2RA", "P2SI", "P2SM", "P2SP", "P2WD", "P3AR",
			"P3CR", "P3CV", "P3EP", "P3HR", "P3LR", "P3LV", "P3NP", "P3NV",
			"P3PA", "P3PV", "P3RA", "P3SI", "P3SM", "P3SP", "P3WD", "P4AR",
			"P4CR", "P4CV", "P4EP", "P4HR", "P4LR", "P4LV", "P4NP", "P4NV",
			"P4PA", "P4PV", "P4RA", "P4SI", "P4SM", "P4SP", "P4WD", "PAME",
			"PASM", "PAUS", "PDLY", "PREC", "PREA", "PTIME", "R1CV", "R1NV",
			"R1PV", "R2CV", "R2NV", "R2PV", "R3CV", "R3NV", "R3PV", "R4CV",
			"R4NV", "R4PV", "R5CV", "R5NV", "R5PV", "SMSG", "STIME", "STRATTR",
			"T1CV", "T1NV", "T1PV", "T2CV", "T2NV", "T2PV", "T3CV", "T3NV",
			"T3PV", "T4CV", "T4NV", "T4PV", "VERS", "WAIT", "WERR"
		}},
		{"busy", {
			"INP", "ONAM", "OUT", "ZNAM"
		}},
		{"dfanout", { // Note: This is an old record, often replaced by fanout
			"INP", "OUTA", "OUTB", "OUTC", "OUTD", "OUTE", "OUTF", "OUTG", "OUTH",
			"SELL", "SELM"
		}},
		{"event", {
			"INP", "OVAL", "SIML", "SIMS", "SIOL", "SVAL"
		}},
		{"fanout", {
			"INP", "LNK0", "LNK1", "LNK2", "LNK3", "LNK4", "LNK5", "LNK6", "LNK7",
			"LNK8", "LNK9", "LNKA", "LNKB", "LNKC", "LNKD", "LNKE", "LNKF", "SELL", "SELM"
		}},
		{"motor", { // Fields from the motor module, can vary significantly by version
			"ACCL", "ADEL", "ALST", "ATHM", "BACC", "BDST", "BVEL", "CARD",
			"CDIR", "CNEN", "DCOF", "DHLM", "DIFF", "DINP", "DIR", "DLLM",
			"DLY", "DMOV", "DOL", "DRBV", "DVAL", "EGU", "ERES", "FOF",
			"FOFF", "FRAC", "HHSV", "HIGH", "HIHI", "HLM", "HLS", "HLSV",
			"HOME", "HOMF", "HOMR", "HOPR", "HSV", "HVEL", "ICOF", "INIT",
			"JAR", "JOGF", "JOGR", "JVEL", "LDVL", "LLM", "LLS", "LLSV",
			"LOCK", "LOLO", "LOPR", "LOW", "LRLV", "LRVL", "LSPG", "LSV",
			"LVAL", "LVIO", "MDEL", "MIP", "MISS", "MLST", "MOVN", "MRES",
			"MSTA", "NTM", "NTMF", "OFF", "OMSL", "OUT", "PCOF", "POST",
			"PP", "PREC", "PREM", "RBV", "RCNT", "RDBD", "RDBL", "RDIF",
			"REP", "RHLS", "RINP", "RLNK", "RLV", "RMOD", "RMP", "RRBV",
			"RRES", "RSTM", "RTRY", "RVAL", "RVEL", "S", "SBAK", "SBAS", "SET",
			"SMAX", "SPDB", "SPMG", "SREV", "STOO", "STOP", "STUP", "SUSE",
			"SYNC", "TDIR", "TWF", "TWR", "TWV", "UEIP", "UREV", "URIP",
			"VBAS", "VELO", "VERS", "VMAX", "VOF"
		}},
		{"sub", {
			"BRSV", "INAM", "INPA", "INPB", "INPC", "INPD", "INPE", "INPF",
			"INPG", "INPH", "INPI", "INPJ", "INPK", "INPL", "LFLG", "LNAM",
			"OUTA", "OUTB", "OUTC", "OUTD", "OUTE", "OUTF", "OUTG", "OUTH",
			"OUTI", "OUTJ", "OUTK", "OUTL", "SNAM", "SUBL", "SUBM"
		}},
		{"subArray", {
			"BPTR", "EGU", "FTVL", "INP", "INDX", "LENG", "MALM", "NELM",
			"NORD", "OUT", "PREC", "SUBL"
		}},
		{"compress", {
			"ALG", "CVT", "HOPR", "HIHI", "HIGH", "HSV", "HHSV", "INP",
			"LOPR", "LOLO", "LOW", "LSV", "LLSV", "NSAM", "NUSE", "OFF",
			"RES", "SQUE"
		}},
		{"pid", { // PID control record from std support
			"CVAL", "D", "DEAD", "DGAP", "DP", "DRVH", "DRVL", "DT", "EGU",
			"ERR", "FBON", "HIGH", "HIHI", "HOPR", "I", "INP", "IVAL", "KD",
			"KI", "KP", "LOLO", "LOPR", "LOW", "MAXI", "MDEL", "MID", "OIF",
			"OMSL", "OUT", "P", "PVAL", "RBV", "STN", "SUM", "TMOD", "TRIG"
		}}
	};

	// Cache for field-record combinations that have already been checked.
	static std::unordered_map<std::string, bool> fieldCache;

	// Create a key for the cache: "recordType.fieldName".
	std::string cacheKey = rec + "." + fld;

	// Check if this combination is already in the cache.
	{
		std::lock_guard<std::shared_timed_mutex> lock(fieldCacheLock);
		auto cacheIt = fieldCache.find(cacheKey);
		if (cacheIt != fieldCache.end()) {
			return cacheIt->second;
		}
	}

	// Check if the field exists in the specific record type's map entry.
	bool exists = false;
	auto recordIt = recordSpecificFields.find(rec);
	if (recordIt != recordSpecificFields.end() &&
		recordIt->second.find(fld) != recordIt->second.end()) {
		exists = true;
	}
	// Update the cache with the result.
	{
		std::lock_guard<std::shared_timed_mutex> lock(fieldCacheLock);
		fieldCache[cacheKey] = exists;
	}
	return exists;
}

void Globals::unregisterArraysForInstance(InstanceDataPtr* instance) {
	TimeoutUniqueLock<std::shared_timed_mutex> lock(pvRegistryLock, "unregisterArraysForInstance", std::chrono::milliseconds(500));
	if (!lock.isLocked()) {
		CaLabDbgPrintf("Error: Failed to acquire lock in unregisterArraysForInstance.");
		return;
	}
	for (auto it = arrayToInstanceMap.begin(); it != arrayToInstanceMap.end(); ) {
		if (it->second == instance) {
			it = arrayToInstanceMap.erase(it);
		}
		else {
			++it;
		}
	}
}
