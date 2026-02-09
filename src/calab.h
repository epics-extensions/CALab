#pragma once

// =============================================================================
// CALab public API header
// -----------------------------------------------------------------------------
// This header exposes the LabVIEW-callable API and all LabVIEW-compatible data
// structures used by the CALab EPICS Channel Access bridge. The types here are
// designed to match LabVIEW handle-based memory semantics and must only be
// allocated/freed via the LabVIEW Memory Manager (DS* functions).
//
 // Notes
// - This file intentionally contains documentation in a consistent Doxygen
//   style to ease maintenance and code navigation.
// - No symbols in this file modify behavior; changes are limited to comments.
// =============================================================================

#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include "epics_compat.h"
#include <extcode.h>
#include "PVItem.h"

#ifndef EXPORT
#define EXPORT __declspec(dllexport)
#endif
#define ERROR_OFFSET		7000

#include "lv_prolog.h"

// LabVIEW Array and Result Types
// -----------------------------------------------------------------------------
// Array handle conventions:
// - All arrays use LabVIEW's handle layout (pointer to a handle whose first
//   element encodes dimensions, followed by inline elements).
// - Memory ownership is with LabVIEW unless explicitly documented otherwise.
// - Use DSNewHandle/DSDisposeHandle and related APIs for allocation/disposal.

/** 1D array of LabVIEW strings (handles), variable length. */
typedef struct { size_t dimSize; LStrHandle elt[1]; } sStringArray;
typedef sStringArray** sStringArrayHdl;
/** 1D array of doubles, variable length. */
typedef struct { size_t dimSize; double elt[1]; } sDoubleArray;
typedef sDoubleArray** sDoubleArrayHdl;
/** 2D array of doubles in row-major order, dimensions in dimSizes[2]. */
typedef struct { uInt32 dimSizes[2]; double elt[1]; } sDoubleArray2D;
typedef sDoubleArray2D** sDoubleArray2DHdl;
// 2D string and integer arrays (row-major: [row * dimSizes[1] + col])
typedef struct { uInt32 dimSizes[2]; LStrHandle elt[1]; } sStringArray2D;
typedef sStringArray2D** sStringArray2DHdl;
typedef struct { uInt32 dimSizes[2]; uint64_t elt[1]; } sLongArray2D;
typedef sLongArray2D** sLongArray2DHdl;
typedef struct { size_t dimSize; uint64_t elt[1]; } sLongArray;
typedef sLongArray** sLongArrayHdl;

/**
 * @struct sError
 * @brief LabVIEW error cluster structure.
 *
 * Matches LabVIEW's {status, code, source} error cluster. All fields are
 * optional for callers, but when populated the memory must be managed using
 * LabVIEW's DS* APIs.
 */
typedef struct {
	LVBoolean status;        // True if an error occurred
	uInt32 code;              // Error or warning code
	LStrHandle source;        // Human-readable message text
} sError;
typedef sError** sErrorHdl;

typedef struct {
	size_t dimSize;
	sError result[1];
} sErrorArray;
typedef sErrorArray** sErrorArrayHdl;

/**
 * @enum DataTypeEnum
 * @brief Data type selector for putValue, mapping LabVIEW to C++ and EPICS CA.
 */
typedef enum {
	DT_STRING = 0,   // LV: string;     C++: char[];    CA: dbr_string_t
	DT_SINGLE = 1,   // LV: SGL;        C++: float;     CA: dbr_float_t
	DT_DOUBLE = 2,   // LV: DBL;        C++: double;    CA: dbr_double_t
	DT_CHAR   = 3,   // LV: I8;         C++: char;      CA: dbr_char_t
	DT_SHORT  = 4,   // LV: I16;        C++: short;     CA: dbr_short_t
	DT_LONG   = 5,   // LV: I32;        C++: long;      CA: dbr_long_t
	DT_QUAD   = 6    // LV: I64;        C++: int64_t;   CA: dbr_long_t (CA limit)
} DataTypeEnum;

/**
 * @struct sResult
 * @brief Result cluster returned per PV by getValue and user events.
 *
 * All string fields are LabVIEW strings (LStrHandle). The value arrays are
 * resizable and their length is reflected in valueArraySize.
 */
typedef struct {
	LStrHandle PVName;                 // PV name
	uInt32 valueArraySize;             // Number of elements currently in the value arrays
	sStringArrayHdl StringValueArray;  // Values as strings (length == valueArraySize)
	sDoubleArrayHdl ValueNumberArray;  // Values as doubles (length == valueArraySize)
	LStrHandle StatusString;           // Status as text (e.g., NO_ALARM)
	int16_t StatusNumber;              // Status as numeric code (CA/DB status)
	LStrHandle SeverityString;         // Severity as text (e.g., MINOR)
	int16_t SeverityNumber;            // Severity as numeric code
	LStrHandle TimeStampString;        // Timestamp in human-readable format
	uInt32 TimeStampNumber;            // Seconds since Unix epoch (converted from EPICS epoch)
	sStringArrayHdl FieldNameArray;    // Optional: field names cached for this PV
	sStringArrayHdl FieldValueArray;   // Optional: field values as strings
	sError ErrorIO;                    // Error cluster for per-PV issues
} sResult;
typedef sResult* sResultPtr;
typedef sResult** sResultHdl;

typedef struct {
	size_t dimSize;
	sResult result[1];
} sResultArray;
typedef sResultArray** sResultArrayHdl;

/**
 * @enum out_filter
 * @brief Bitmask selecting which outputs get populated by getValue.
 */
typedef enum {
	 firstValueAsString   = 1,       // Fill FirstStringValue (per-PV first value as string)
	 firstValueAsNumber   = 2,       // Fill FirstDoubleValue (per-PV first value as double)
	 valueArrayAsNumber   = 4,       // Fill DoubleValueArray (2D numeric array)
	 errorOut             = 8,       // Fill global error outputs when present

	 // sResult population flags (per-PV cluster)
	 pviElements          = 16,      // valueArraySize
	 pviValuesAsString    = 32,      // StringValueArray
	 pviValuesAsNumber    = 64,      // ValueNumberArray
	 pviStatusAsString    = 128,     // StatusString
	 pviStatusAsNumber    = 256,     // StatusNumber
	 pviSeverityAsString  = 512,     // SeverityString
	 pviSeverityAsNumber  = 1024,    // SeverityNumber
	 pviTimestampAsString = 2048,    // TimeStampString
	 pviTimestampAsNumber = 4096,    // TimeStampNumber
	 pviFieldNames        = 8192,    // FieldNameArray
	 pviFieldValues       = 16384,   // FieldValueArray
	 pviError             = 32768,   // ErrorIO

	 // Convenience masks
	 pviAll = pviElements | pviValuesAsString | pviValuesAsNumber |
				 pviStatusAsString | pviStatusAsNumber | pviSeverityAsString |
				 pviSeverityAsNumber | pviTimestampAsString | pviTimestampAsNumber |
				 pviFieldNames | pviFieldValues | pviError,
	 defaultFilter = 0xffff          // Default: populate everything
} out_filter;

// Exported Functions (callable from LabVIEW)
extern "C" {
	/**
	 * @brief Fetches EPICS PV values and fills the LabVIEW output arrays.
	 *
	 * On the first call or when inputs change, this function initializes the PV registry
	 * and per-PV state, subscribes to missing PVs (including optional fields), waits up to
	 * `Timeout` for initial values, and copies the results into the requested LabVIEW arrays
	 * according to the provided `filter` bitmask.
	 *
	 * Thread-safety: Acquires a short-lived exclusive lock for setup and shared locks
	 * during data collection. `CommunicationStatus` is set to 1 on failures such as invalid
	 * handles or lock timeouts, otherwise 0.
	*
	* @param PvNameArray          Input PV names (required).
	* @param FieldNameArray       Optional field selection per PV (may be null).
	* @param PvIndexArray         Opaque per-PV cache maintained across calls.
	* @param Timeout              Max seconds to wait for connections/values.
	* @param ResultArray          Optional: per-PV result cluster array.
	* @param FirstStringValue     Optional: first value per PV as string.
	* @param FirstDoubleValue     Optional: first value per PV as double.
	* @param DoubleValueArray     Optional: 2D numeric array (values per PV).
	* @param CommunicationStatus  Set to 1 on recoverable failure, else 0.
	* @param FirstCall            In/out: signals first invocation from the VI.
	* @param NoMDEL               Reserved for future use.
	* @param IsInitialized        In/out: true when cache/arrays are initialized.
	* @param filter               Output selection bitmask (out_filter).
	 */
	EXPORT void getValue(sStringArrayHdl* PvNameArray, sStringArrayHdl* FieldNameArray, sLongArrayHdl* PvIndexArray, double Timeout, sResultArrayHdl* ResultArray, sStringArrayHdl* FirstStringValue, sDoubleArrayHdl* FirstDoubleValue, sDoubleArray2DHdl* DoubleValueArray, LVBoolean* CommunicationStatus, LVBoolean* FirstCall, LVBoolean* NoMDEL, LVBoolean* IsInitialized, int filter);

    /**
	* @brief Write values to EPICS PVs.
	*
	* Supports writing string or numeric arrays (row-major 2D). When wait4readback
	* is true, the function waits for CA put-callbacks (or initial value callbacks)
	* to confirm completion up to Timeout.
	*
	* @param PvNameArray      PV names to write to (rows).
	* @param PvIndexArray     Optional cache maintained across calls.
	* @param StringValueArray2D  Source values when DataType=DT_STRING.
	* @param DoubleValueArray2D  Source values when DataType=DT_SINGLE/DT_DOUBLE.
	* @param LongValueArray2D    Source values when DataType is an integer type.
	* @param DataType         Type selector (see DataTypeEnum).
	* @param Timeout          Max seconds to wait for connection/confirmation.
	* @param wait4readback    If true, wait for completion callbacks.
	* @param ErrorArray       Per-PV error results (allocated/filled on demand).
	* @param Status           Aggregate success (false when any PV fails).
	* @param FirstCall        In/out first call toggle for initialization.
	*/
	EXPORT void putValue(sStringArrayHdl* PvNameArray, sLongArrayHdl* PvIndexArray, sStringArray2DHdl* StringValueArray2D, sDoubleArray2DHdl* DoubleValueArray2D, sLongArray2DHdl* LongValueArray2D, uInt32 DataType, double Timeout, LVBoolean* wait4readback, sErrorArrayHdl* ErrorArray, LVBoolean* Status, LVBoolean* FirstCall);

	/**
	 * @brief Collect library/runtime diagnostic information and an optional PV snapshot.
	 *
	 * Provides a compact, human-readable 2D table of key/value pairs about the current
	 * CALab instance (e.g., version, thread counts, CA context, connection statistics).
	 * When ResultArray is supplied, a per-PV snapshot is returned similar to getValue's
	 * sResult content (names, first values, alarm/timestamp fields), without performing
	 * new subscriptions. Intended for info panels and debugging VIs.
	 *
	 * Memory/ownership: All output handles are LabVIEW-owned. On the first invocation
	 * (FirstCall == true), the function allocates or resizes buffers as needed and may
	 * cache them for reuse by the calling VI.
	 *
	 * @param InfoStringArray2D  Optional: 2D string array [N x 2] with {Name, Value} rows.
	 *                           Pass null to skip. Allocated/resized as needed.
	 * @param ResultArray        Optional: per-PV result array (subset of pviAll). Pass null to skip.
	 * @param FirstCall          In/out: set true on first VI call to allow internal init/caching.
	 */
	EXPORT void info(sStringArray2DHdl* InfoStringArray2D, sResultArrayHdl* ResultArray, LVBoolean* FirstCall);

	EXPORT void disconnectPVs(sStringArrayHdl* PvNameArray, bool All);

	// VI Lifecycle Callbacks
	EXPORT MgErr reserved(InstanceDataPtr* instanceState);
	EXPORT MgErr unreserved(InstanceDataPtr* instanceState);
	EXPORT MgErr aborted(InstanceDataPtr* instanceState);

	// LabVIEW User Event Callbacks
	EXPORT void addEvent(LVUserEventRef* RefNum, sResult* ResultPtr);
	EXPORT void destroyEvent(LVUserEventRef* RefNum);
	EXPORT uInt32 getCounter();
}

#include "lv_epilog.h"

// =================================================================================
// Internal Implementation Details
// These are helper structures and functions used internally by the DLL.
// =================================================================================

/**
 * @struct PVMetaInfo
 * @brief Per-PV change-tracking metadata backing PvIndexArray entries.
 *
 * Captures the last known change-hash of a PVItem so we can detect when
 * values/metadata have changed without copying payloads.
 */
struct PVMetaInfo {
	PVItem* pvItem;
	std::size_t lastChangeHash;
	std::string functionName;

	PVMetaInfo(PVItem* item)
		: pvItem(item), lastChangeHash(item->getChangeHash()), functionName() {
	}

	bool hasChanged() const {
		return pvItem && pvItem->hasChanged(lastChangeHash);
	}

	void update() {
		if (pvItem) {
			lastChangeHash = pvItem->getChangeHash();
		}
	}
};
struct PvIndexEntry;
namespace {
	std::mutex g_entryRegistryMutex;
	std::unordered_set<PvIndexEntry*> g_entryRegistry;

	void registerEntry(PvIndexEntry* e) {
		if (!e) return;
		std::lock_guard<std::mutex> lk(g_entryRegistryMutex);
		g_entryRegistry.insert(e);
	}

	void unregisterEntry(PvIndexEntry* e) {
		if (!e) return;
		std::lock_guard<std::mutex> lk(g_entryRegistryMutex);
		g_entryRegistry.erase(e);
	}

	static inline bool isLiveEntry(PvIndexEntry* e) {
		if (!e) return false;
		std::lock_guard<std::mutex> lk(g_entryRegistryMutex);
		return g_entryRegistry.find(e) != g_entryRegistry.end();
	}
}
/**
 * @struct PvIndexEntry
 * @brief Entry stored in PvIndexArray: links a PVItem to its PVMetaInfo.
 *
 * Added:
 *  - internal atomic refcount to allow safe concurrent readers
 *  - deletion marker to prevent new acquirers while an entry is being reclaimed
 *
 * Usage:
 *  - Readers should call try_acquire() before using the entry and release()
 *    afterwards.
 *  - Deleter sets deleting=true and waits for refs==0 before deleting.
 */
struct PvIndexEntry {
	PVItem* pvItem;
	PVMetaInfo* metaInfo;

	// Internal lifetime control for safe concurrent access
	std::atomic<uint32_t> refs{ 0 };
	std::atomic<bool> deleting{ false };

	PvIndexEntry(PVItem* item, PVMetaInfo* meta)
		: pvItem(item), metaInfo(meta) {
		registerEntry(this);
	}

	~PvIndexEntry() {
		unregisterEntry(this);
	}

	// Try to acquire a usage reference. Returns false if entry is being deleted.
	bool try_acquire() noexcept {
		uint32_t old = refs.load(std::memory_order_relaxed);
		for (;;) {
			// If deletion in progress, refuse to acquire.
			if (deleting.load(std::memory_order_acquire)) {
				return false;
			}
			if (refs.compare_exchange_weak(old, old + 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
				return true;
			}
			// compare_exchange_weak updates old; loop and retry.
		}
	}

	// Release a previously acquired reference.
	void release() noexcept {
		refs.fetch_sub(1, std::memory_order_acq_rel);
	}

	// Mark entry for deletion so new acquirers fail.
	void mark_deleting() noexcept {
		deleting.store(true, std::memory_order_release);
	}

	// Return current refcount (for waiting in deleter).
	uint32_t current_refs() const noexcept {
		return refs.load(std::memory_order_acquire);
	}
};

/**
 * @struct MyInstanceData
 * @brief Per-VI-instance handle cache for output arrays.
 *
 * Stores handles that belong to the caller VI so repeated calls can reuse
 * allocated arrays, minimizing allocations and copy overhead.
 */
struct MyInstanceData {
	std::mutex arrayMutex;
	std::string firstFunctionName;
	sLongArrayHdl PvIndexArray = nullptr;
	sResultArrayHdl ResultArray = nullptr;
	sStringArrayHdl FirstStringValue = nullptr;
	sDoubleArrayHdl FirstDoubleValue = nullptr;
	sDoubleArray2DHdl DoubleValueArray = nullptr;
	std::atomic<bool> isUnreserving{ false };
	std::atomic<uint32_t> activeCalls{ 0 };

	~MyInstanceData() {
	 // Deallocation is handled explicitly in the `unreserved` callback.
	}
};

typedef MyInstanceData* MyInstanceDataPtr;



// --- Internal Function Prototypes ---

// Debugging Utilities
/**
 * @brief Print a formatted debug line with a timestamp prefix.
 * @param format Printf-style format string.
 * @return Number of characters written.
 */
MgErr CaLabDbgPrintf(const char* format, ...);

/**
 * @brief Print a formatted debug line with a timestamp prefix (debug builds only).
 * @param format Printf-style format string.
 * @return Number of characters written (0 in non-debug builds).
 */
MgErr CaLabDbgPrintfD(const char* format, ...);

// Undocumented but used LV function for debug printing.
TH_REENTRANT EXTERNC MgErr _FUNCC DbgPrintfv(const char* buf, va_list args);

// Memory Management Helpers
/** Set a LabVIEW string handle to the specified text (allocates/resizes as needed). */
void setLVString(LStrHandle& handle, const std::string& text);

/** Fill an sResult instance from a PVItem (values, status, severity, timestamp) */
void fillResultFromPv(PVItem* pvItem, sResult* target);

/** Populate a 2D string array with key/value info pairs (rows x 2). */
MgErr populateInfoStringArray(sStringArray2DHdl* InfoStringArray2D,
	const std::vector<std::pair<std::string, std::string>>& info);

/** Populate a ResultArray with one entry per PV currently in the registry. */
MgErr populateAllResultArray(sResultArrayHdl* ResultArray);

std::vector<std::pair<std::string, std::string>> collectEnvironmentInfo();


/**
 * @brief Dispose a LabVIEW string array handle and all contained string handles.
 * @param array LabVIEW string array handle to free.
 * @return LabVIEW memory manager error code (noErr on success).
 */
MgErr DeleteStringArray(sStringArrayHdl array);

/**
 * @brief Free all handles inside an sResult element and reset its fields.
 * @param currentResult Pointer to the sResult element to clean.
 * @return LabVIEW memory manager error code (noErr on success).
 */
MgErr CleanupResult(sResult* currentResult);

/**
 * @brief Release and/or selectively clean output buffers when sizes change.
 * If changedIndices is empty, performs a full cleanup; otherwise cleans only the specified entries.
 * @param ResultArray Result array handle to clean.
 * @param FirstStringValue First string values array handle.
 * @param FirstDoubleValue First numeric values array handle.
 * @param DoubleValueArray 2D numeric values array handle.
 * @param PvNameArray Input PV name array (used for size checks).
 * @param filter Output selection bitmask.
 * @param changedIndices Optional list of indices to clean selectively.
 */
void cleanupMemory(sResultArrayHdl* ResultArray, sStringArrayHdl* FirstStringValue,
	sDoubleArrayHdl* FirstDoubleValue, sDoubleArray2DHdl* DoubleValueArray,
	sStringArrayHdl* PvNameArray, const std::vector<uInt32>& changedIndices);

/**
 * @brief Allocate/resize all required output arrays for getValue according to the filter.
 * Initializes newly created entries and preserves existing content where possible.
 * @param nameCount Number of PVs.
 * @param maxNumberOfValues Maximum value count per PV.
 * @param filter Output selection bitmask.
 * @param ResultArray Result array handle.
 * @param FirstStringValue First string values array handle.
 * @param FirstDoubleValue First numeric values array handle.
 * @param DoubleValueArray 2D numeric values array handle.
 * @param PvNameArray Input PV name array.
 * @param changedIndices Optional list of indices to (re)initialize.
 * @return LabVIEW memory manager error code (noErr on success).
 */
MgErr prepareOutputArrays(uInt32 nameCount, uInt32 maxNumberOfValues, int filter,
	sResultArrayHdl* ResultArray, sStringArrayHdl* FirstStringValue,
	sDoubleArrayHdl* FirstDoubleValue, sDoubleArray2DHdl* DoubleValueArray,
	sStringArrayHdl* PvNameArray, const std::vector<uInt32>& changedIndices);

// Core PV & Channel Access Logic

/**
 * @brief Initialize PvIndex entries and ensure base PVs exist in the registry.
 * @param PvNameArray LabVIEW array of PV names.
 * @param FieldNameArray Optional LabVIEW array of field names.
 * @param PvIndexArray Output index array.
 * @param CommunicationStatus Set to 1 on failure.
 * @param uninitializedPvNames Output set of base PV names that require connection.
 * @return true on success, false on allocation/lock failures.
 */
bool setupPvIndexAndRegistry(
	sStringArrayHdl* PvNameArray,
	sStringArrayHdl* FieldNameArray,
	sLongArrayHdl* PvIndexArray,
	LVBoolean* CommunicationStatus,
	std::unordered_set<std::string>* uninitializedPvNames);

/**
 * @brief Connect base PVs and their .RTYP channels, then subscribe.
 * Wait up to Timeout for .RTYP values to determine record types.
 * @param basePvNames Set of base PV names to process.
 * @param Timeout Maximum seconds to wait for .RTYP resolution.
 */
void subscribeBasePVs(const std::unordered_set<std::string>& basePvNames, double Timeout, std::chrono::steady_clock::time_point endBy, LVBoolean* NoMDEL);

/**
 * @brief Validate and connect approved field PVs (e.g., base.FIELD) after record type is known.
 * @param basePvNames Set of base PV names whose fields should be processed.
 * @param Timeout Maximum seconds to wait for connections.
 */
void connectPVs(const std::unordered_set<std::string>& basePvNames, double Timeout, std::chrono::steady_clock::time_point endBy);

/**
 * @brief Wait for initial values from field PVs after subscriptions have been established.
 * @param basePvNames Set of base PV names whose fields are being monitored.
 * @param Timeout Maximum seconds to wait before giving up.
 */
void waitForUninitializedPvs(const std::unordered_set<std::string>& basePvNames, double Timeout, std::chrono::steady_clock::time_point endBy);

/**
 * @brief Create a CA channel for a PV and optionally its .RTYP companion.
 * Adds created channels to the pending connection tracker.
 * @param pvItem The PVItem to connect.
 * @param filter Output selection bitmask (reserved for future use).
 * @param connectRtyp If true, also connect the .RTYP companion channel.
 */
void connectPv(PVItem* pvItem, bool connectRtyp = true);

/**
 * @brief Create a CA value/alarm subscription for a given PV.
 * @param pvItem The PVItem (must already have a connected CA channel).
 */
void subscribePv(PVItem* pvItem);

/**
 * @brief Determine which PV indices have changed since the last read.
 * Optionally waits briefly for initial subscriptions to deliver a first value.
 * @param PvIndexArray Index array with per-PV metadata.
 * @param Timeout Maximum seconds to wait for first values (when needed).
 * @return List of indices that have changed or require a refresh.
 */
std::vector<uInt32> getChangedPvIndices(sLongArrayHdl* PvIndexArray, double Timeout, std::chrono::steady_clock::time_point endBy, LVBoolean* NoMDEL);

/**
 * @brief Update the cached change-hash in the PvIndexArray entries.
 * If changedIndices is empty, all entries are updated.
 * @param PvIndexArray Index array handle.
 * @param changedIndices Optional list of indices to update.
 */
void updatePvIndexArray(sLongArrayHdl* PvIndexArray, const std::vector<uInt32>& changedIndices);

/**
 * @brief Populate selected LabVIEW output arrays from the current PVItem data.
 * @param nameCount Number of PVs.
 * @param maxNumberOfValues Maximum values per PV for numeric arrays.
 * @param filter Output selection bitmask (out_filter).
 * @param PvIndexArray Index array mapping to PVItems.
 * @param ResultArray Optional per-PV result cluster array to fill.
 * @param FirstStringValue Optional first value as string per PV to fill.
 * @param FirstDoubleValue Optional first value as number per PV to fill.
 * @param DoubleValueArray Optional 2D numeric array to fill.
 * @param changedIndices Optional list of indices to update selectively.
 */
void populateOutputArrays(uInt32 nameCount, uInt32 maxNumberOfValues, int filter,
	sLongArrayHdl* PvIndexArray, sResultArrayHdl* ResultArray,
	sStringArrayHdl* FirstStringValue, sDoubleArrayHdl* FirstDoubleValue,
	sDoubleArray2DHdl* DoubleValueArray, const std::vector<uInt32>& changedIndices);

// Channel Access Callbacks

/**
 * @brief Channel Access callback for connection state changes.
 * @param args CA struct containing channel ID and operation status.
 */
void connectionChanged(connection_handler_args args);

/**
 * @brief Channel Access callback for value updates.
 * Enqueues background processing to avoid blocking the CA thread.
 * @param args CA struct containing type, count, channel ID, and data payload.
 */
void valueChanged(struct event_handler_args args);

/**
 * @brief Channel Access callback for receiving ENUM metadata (string labels).
 * @param args CA struct carrying DBR_CTRL_ENUM data.
 */
void enumInfoChanged(struct event_handler_args args);

// Event Posting Helper
/** Post a LabVIEW user event for a given PV (thread-safe snapshotting of subscribers). */
void postEventForPv(const std::string& pvName);

void executeSyncGet(PVItem* pvItem, std::chrono::steady_clock::time_point endBy);

void getValueDirectSync(
	sStringArrayHdl* PvNameArray,
	sStringArrayHdl* FieldNameArray,
	double Timeout,
	sResultArrayHdl* ResultArray,
	sStringArrayHdl* FirstStringValue,
	sDoubleArrayHdl* FirstDoubleValue,
	sDoubleArray2DHdl* DoubleValueArray,
	LVBoolean* CommunicationStatus);

namespace LockTracker {
	/** Per-thread record of the last lock acquisition site for each mutex. */
	struct thread_local_lock_info {
		std::unordered_map<void*, const char*> lastLockByMutex;
	};

	/** Thread-local lock acquisition history. */
	inline thread_local thread_local_lock_info threadLocalInfo;

	/** Global map from mutex pointer to the owning function name. */
	inline std::mutex mutexMapMutex;
	inline std::unordered_map<void*, const char*> globalMutexOwners;

	/** Update global owner information for a mutex. */
	inline void updateMutexOwner(void* mutexPtr, const char* functionName) {
		std::lock_guard<std::mutex> lock(mutexMapMutex);
		globalMutexOwners[mutexPtr] = functionName;
	}

	/** Get the current owner function name for a mutex. */
	inline const char* getMutexOwner(void* mutexPtr) {
		std::lock_guard<std::mutex> lock(mutexMapMutex);
		auto it = globalMutexOwners.find(mutexPtr);
		return (it != globalMutexOwners.end()) ? it->second : "unknown";
	}

	/** Record the last lock site for the current thread. */
	inline void setLastLock(void* mutexPtr, const char* functionName) {
		threadLocalInfo.lastLockByMutex[mutexPtr] = functionName;
	}

	/** Retrieve the last lock site for the current thread. */
	inline const char* getLastLock(void* mutexPtr) {
		auto it = threadLocalInfo.lastLockByMutex.find(mutexPtr);
		return (it != threadLocalInfo.lastLockByMutex.end()) ? it->second : "no previous lock";
	}
}
