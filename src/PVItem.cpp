// PVItem.cpp
#include <string>
#include <atomic>
#include <vector>
#include <mutex>
#include <sstream>
#include <cstring>
#include <condition_variable>
#include <cmath>
#include <cstdio>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <locale>
#include <exception>
#include "calab.h"
#include "PVItem.h"
#include "globals.h"

// Default constructor
PVItem::PVItem()
    : parent(nullptr),
      toBeRemoved(false),
      channelId(nullptr),
      eventId(nullptr),
      changeHash_(0),
      dbrType_(-1),
      enumFetchRequested_(false),
      enumStrings_(),
      enumValue{0, 0, 0, {}, 0},
      errorCode_(ECA_DISCONNCHID),
      fieldValues_(),
      fields_(),
      hasValue_(false),
      // io_mtx_ default-constructed
      isConnected_(false),
      isErrorCounted_(false),
      isPassive_(false),
      name_(""),
      nativeFieldType_(nullptr),
      numberOfValues_(0),
      precCached_(-1),
      recordType_(""),
      severity_(epicsSevInvalid),
      status_(epicsAlarmComm),
      timestamp_(0),
      timestamp_nsec_(0),
      userData_(nullptr)
{
    // Start as disconnected: count as error until first success
    isErrorCounted_.store(true, std::memory_order_relaxed);
    Globals::getInstance().pvErrorCount.fetch_add(1, std::memory_order_relaxed);
}

// Parameterized constructor
PVItem::PVItem(
    const std::string& name, 
    chanId cid, 
    short dbrType,
    uInt32 numVals, 
    const std::string& recType,
    void* dataBuffer, 
    uInt32 ts, 
    uInt32 nsec, 
    int16_t stat,
    int16_t sev, 
    bool connected, 
    bool hasVal,
    bool passive, 
    PVItem* parentIn, 
    evid evId,
    const std::vector<std::pair<std::string, chanId>>& fields)
    : parent(parentIn),
      toBeRemoved(false),
      channelId(cid),
      eventId(evId),
      changeHash_(0),
      dbrType_(dbrType),
      enumFetchRequested_(false),
      enumStrings_(),
      enumValue{0, 0, 0, {}, 0},
      errorCode_(ECA_DISCONNCHID),
      fieldValues_(),
      fields_(fields),
      hasValue_(hasVal),
      // io_mtx_ default-constructed
      isConnected_(connected),
      isErrorCounted_(false),
      isPassive_(passive),
      name_(name),
      nativeFieldType_(dataBuffer),
      numberOfValues_(numVals),
      precCached_(-1),
      recordType_(recType),
      severity_(sev),
      status_(stat),
      timestamp_(ts),
      timestamp_nsec_(nsec),
      userData_(nullptr)
{
    // Start as disconnected and counted until a successful event updates it
    isErrorCounted_.store(true, std::memory_order_relaxed);
    Globals::getInstance().pvErrorCount.fetch_add(1, std::memory_order_relaxed);
}

// Copy constructor
PVItem::PVItem(const PVItem& other)
    : parent(other.parent),
      toBeRemoved(other.toBeRemoved.load()),
      channelId(other.channelId),
      eventId(other.eventId),
      changeHash_(other.changeHash_.load()),
      dbrType_(other.dbrType_.load()),
      enumFetchRequested_(other.enumFetchRequested_.load()),
      enumStrings_(other.enumStrings_),
      enumValue(other.enumValue),
      errorCode_(other.errorCode_.load()),
      fieldValues_(other.fieldValues_),
      fields_(other.fields_),
      hasValue_(other.hasValue_.load()),
      // io_mtx_ default-constructed
      isConnected_(other.isConnected_.load()),
      isErrorCounted_(false), // copies never contribute to error count
      isPassive_(other.isPassive_.load()),
      name_(other.name_),
      nativeFieldType_(other.nativeFieldType_.load()),
      numberOfValues_(other.numberOfValues_),
      precCached_(other.precCached_.load()),
      recordType_(other.recordType_),
      severity_(other.severity_.load()),
      status_(other.status_.load()),
      timestamp_(other.timestamp_.load()),
      timestamp_nsec_(other.timestamp_nsec_.load()),
      userData_(other.userData_)
{
    // Avoid double-counting on copies; original tracks error counting.
}

// Assignment operator
PVItem& PVItem::operator=(const PVItem& other) {
    if (this != &other) {
        // Preserve and adjust error counting if necessary
        const bool wasCounted = isErrorCounted_.load(std::memory_order_relaxed);

        parent = other.parent;
        toBeRemoved.store(other.toBeRemoved.load());
        channelId = other.channelId;
        eventId = other.eventId;
        changeHash_.store(other.changeHash_.load());
        dbrType_.store(other.dbrType_.load());
        enumFetchRequested_.store(other.enumFetchRequested_.load());
        enumStrings_ = other.enumStrings_;
        enumValue = other.enumValue;
        errorCode_.store(other.errorCode_.load());
        fieldValues_ = other.fieldValues_;
        fields_ = other.fields_;
        hasValue_.store(other.hasValue_.load());
        isConnected_.store(other.isConnected_.load());
        isErrorCounted_.store(false, std::memory_order_relaxed);
        isPassive_.store(other.isPassive_.load());
        name_ = other.name_;
        nativeFieldType_.store(other.nativeFieldType_.load());
        numberOfValues_ = other.numberOfValues_;
        precCached_.store(other.precCached_.load());
        recordType_ = other.recordType_;
        severity_.store(other.severity_.load());
        status_.store(other.status_.load());
        timestamp_.store(other.timestamp_.load());
        timestamp_nsec_.store(other.timestamp_nsec_.load());
        userData_ = other.userData_;

        // Do not count copies; if previously counted, decrement now
        if (wasCounted) {
            Globals::getInstance().pvErrorCount.fetch_sub(1, std::memory_order_relaxed);
        }
    }
    return *this;
}

// Destructor
PVItem::~PVItem() {
    // Adjust global error counter if we were counted
    if (isErrorCounted_.load(std::memory_order_relaxed)) {
        Globals::getInstance().pvErrorCount.fetch_sub(1, std::memory_order_relaxed);
        isErrorCounted_.store(false, std::memory_order_relaxed);
    }
    //CaLabDbgPrintf("~PVItem: Destroy %s", name_.c_str());

    if (Globals::getInstance().stopped.load()) return;
    void* ptr = (void*)nativeFieldType_.load();
    if (ptr) {
        free(ptr);
    }
    int res = ECA_NORMAL;
    if (eventId) {
        res = ca_clear_event(eventId);
        if (res != ECA_NORMAL) {
            CaLabDbgPrintf("PVItem destructor: Event not found for %s", name_.c_str());
        }
        eventId = nullptr;
    }
    if (channelId) {
        // Check channel status before deleting
        channel_state state = ca_state(channelId);
        if (state != cs_closed && state != cs_never_conn) {
            res = ca_clear_channel(channelId);
            if (res != ECA_NORMAL) {
                CaLabDbgPrintf("PVItem destructor: Channel not found for %s, error code: %d",
                    name_.c_str(), res);
            }
        }
        else {
            CaLabDbgPrintf("PVItem destructor: Channel skipped for %s (state: %d)",
                name_.c_str(), state);
        }
        channelId = nullptr;
    }
}

// Getter methods
const std::string& PVItem::getName() const { return name_; }
short PVItem::getDbrType() const { return dbrType_.load(); }
uInt32 PVItem::getNumberOfValues() const { return numberOfValues_; }
const std::string& PVItem::getRecordType() const { return recordType_; }
const void* PVItem::getDbr() const { return nativeFieldType_.load(); }
uInt32 PVItem::getTimestamp() const { return timestamp_.load(); }
uInt32 PVItem::getTimestampNSec() const { return timestamp_nsec_.load(); }
int16_t PVItem::getStatus() const { return status_.load(); }
int16_t PVItem::getSeverity() const { return severity_.load(); }
bool PVItem::isConnected() const { return isConnected_.load(); }
bool PVItem::hasValue() const { return hasValue_.load(); }
bool PVItem::isPassive() const { return isPassive_.load(); }
const std::vector<std::pair<std::string, chanId>>& PVItem::getFields() const { return fields_; }
void* PVItem::getUserData() const { return userData_; }

// Setter methods
void PVItem::setName(const std::string& name) { name_ = name; }
void PVItem::setDbrType(short type) { dbrType_.store(type); }
void PVItem::setNumberOfValues(uInt32 num) { numberOfValues_ = num; }
void PVItem::setRecordType(const std::string& type) { recordType_ = type; }
void PVItem::setDbr(void* newDbr) { // Get and replace the old buffer
    void* old = (void*)nativeFieldType_.exchange(newDbr);
    if (old) {
        free(old);
        old = nullptr;
    }
    updateChangeHash();
}

void PVItem::clearDbr() {
    void* old = nativeFieldType_.exchange(nullptr);
    if (old) free(old);
    updateChangeHash();
}

void PVItem::clearFields()
{
    fields_.clear();
    fieldValues_.clear();
    precCached_ = -1;
    updateChangeHash();
}

void PVItem::setTimestamp(uInt32 ts) {
    if (ts != timestamp_.load()) {
        timestamp_.store(ts);
        updateChangeHash();
    }
}
void PVItem::setTimestampNSec(uInt32 nsec) {
    if (nsec != timestamp_nsec_.load()) {
        timestamp_nsec_.store(nsec);
        updateChangeHash();
    }
}
void PVItem::setStatus(int16_t status) {
    if (status != status_.load()) {
        status_.store(status);
        updateChangeHash();
    }
}
void PVItem::setSeverity(int16_t sev) {
    if (sev != severity_.load()) {
        severity_.store(sev);
        updateChangeHash();
    }
}
void PVItem::setConnected(bool connected) {
    if (connected != isConnected_.load()) {
        isConnected_.store(connected);
        updateChangeHash();
    }
}
void PVItem::setHasValue(bool hasValue) {
    if (hasValue != hasValue_.load()) {
        hasValue_.store(hasValue);
        updateChangeHash();
    }
}
void PVItem::setPassive(bool passive) { isPassive_.store(passive); }
void PVItem::setFields(const std::vector<std::pair<std::string, chanId>>& fields) { fields_ = fields; }
void PVItem::setEnumValue(const dbr_ctrl_enum* src) {
    if (src) {
        std::memcpy(&enumValue, src, sizeof(dbr_ctrl_enum));
        enumStrings_.clear();
        for (int i = 0; i < enumValue.no_str; ++i) {
            const char* s = src->strs[i];
            const void* nul = std::memchr(s, '\0', MAX_STRING_SIZE);
            const size_t len = nul ? static_cast<const char*>(nul) - s : static_cast<size_t>(MAX_STRING_SIZE);
            enumStrings_.emplace_back(s, len);
        }
    }
    else {
        std::memset(&enumValue, 0, sizeof(dbr_ctrl_enum));
        enumStrings_.clear();
    }
}

// Special method for CallbackContext
void PVItem::setCallbackContext(std::atomic<int>* pendingCallbacks,
    std::condition_variable* cv,
    std::mutex* mtx,
    void* data) {
    // Parameters currently unused. Keep block to avoid -Wunused-parameter warnings.
    (void)pendingCallbacks; (void)cv; (void)mtx; (void)data;
    /* If needed later:
    if (userData_) {
        delete static_cast<CallbackContext*>(userData_);
    }
    userData_ = new CallbackContext(pendingCallbacks, cv, mtx, data);
    */
}

std::string PVItem::info() const {
    std::ostringstream oss;
    oss << "  Name: " << name_ << "\n";
    {
        const short t = dbrType_.load();
        oss << "  dbrType: ";
        if (t >= 0 &&
            dbr_text &&
            static_cast<unsigned short>(t) < dbr_text_dim)
            {
            oss << dbr_text[t] << " (" << t << ")\n";
        }
        else {
            oss << "<invalid>(" << t << ")\n";
        }
    }
    oss << "  numberOfValues: " << numberOfValues_ << "\n";
    oss << "  recordType: " << recordType_ << "\n";
    oss << "  dbr: 0x" << std::hex << reinterpret_cast<std::uintptr_t>(nativeFieldType_.load()) << std::dec;
    std::vector<std::string> valueStrings = dbrValue2String();
    if (!valueStrings.empty()) {
        oss << " [";
        for (size_t i = 0; i < valueStrings.size(); ++i) {
            oss << valueStrings[i];
            if (i + 1 < valueStrings.size()) oss << ", ";
        }
        oss << "]";
    }
    oss << "\n";
    oss << "  timestamp: " << getTimestampAsString() << " (" << timestamp_.load() << "." << timestamp_nsec_.load() << ")\n";
    oss << "  status: " << getStatusAsString() << " (" << status_.load() << ")\n";
    oss << "  severity: " << getSeverityAsString() << " (" << severity_.load() << ")\n";
    oss << "  isConnected: " << (isConnected_.load() ? "true" : "false") << "\n";
    oss << "  hasValue: " << (hasValue_.load() ? "true" : "false") << "\n";
    oss << "  isPassive: " << (isPassive_.load() ? "true" : "false") << "\n";
    oss << "  userData: 0x" << std::hex << reinterpret_cast<std::uintptr_t>(userData_) << std::dec << "\n";
    oss << "  changeHash: " << changeHash_.load() << "\n";
    oss << "  channelId: 0x" << std::hex << reinterpret_cast<std::uintptr_t>(channelId) << std::dec << "\n";
    oss << "  parent: 0x" << std::hex << reinterpret_cast<std::uintptr_t>(parent) << std::dec << "\n";
    oss << "  eventId: 0x" << std::hex << reinterpret_cast<std::uintptr_t>(eventId) << std::dec << "\n";
    oss << "  toBeRemoved: " << (toBeRemoved.load() ? "true" : "false") << "\n";
    oss << "  fields: [";
    for (size_t i = 0; i < fields_.size(); ++i) {
        oss << "{name: " << fields_[i].first
            << ", chanId: 0x" << std::hex << reinterpret_cast<std::uintptr_t>(fields_[i].second) << std::dec << "}";
        if (i + 1 < fields_.size()) oss << ", ";
    }
    oss << "]";
    if (!enumStrings_.empty()) {
        oss << "  enumStrings: [";
        for (size_t i = 0; i < enumStrings_.size(); ++i) {
            oss << "\"" << enumStrings_[i] << "\"";
            if (i + 1 < enumStrings_.size()) oss << ", ";
        }
        oss << "]\n";
    }
    return oss.str();
}

std::vector<std::string> PVItem::dbrValue2String() const {
    std::vector<std::string> result;
    const void* data = nativeFieldType_.load();

    const short dbrTypeLocal1 = dbrType_.load();
    if (!data || dbrTypeLocal1 < 0 || numberOfValues_ == 0) {
        return result;
    }

    result.reserve(numberOfValues_);

    // Handle different DBR types
    switch (dbrTypeLocal1) {
    case DBR_STRING:
    case DBR_TIME_STRING: {
        const char* p = static_cast<const char*>(data);
        if (dbrTypeLocal1 == DBR_TIME_STRING) {
            p += sizeof(epicsTimeStamp) + 2 * sizeof(dbr_short_t);
        }
        char buf[MAX_STRING_SIZE + 1];
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            // Copy exactly MAX_STRING_SIZE bytes and null-terminate once
            std::memcpy(buf, p, MAX_STRING_SIZE);
            buf[MAX_STRING_SIZE] = '\0';

            // trim spaces
            std::string s(buf);
            size_t a = s.find_first_not_of(" \t\r\n");
            size_t b = s.find_last_not_of(" \t\r\n");
            s = (a == std::string::npos) ? std::string() : s.substr(a, b - a + 1);

            // Recognize special tokens (return as string)
            std::string u = s;
            for (auto& c : u) c = static_cast<char>(::toupper(static_cast<unsigned char>(c)));
            if (u == "INF" || u == "+INF" || u == "INFINITY" || u == "+INFINITY") {
                result.emplace_back("INF");
            }
            else if (u == "-INF" || u == "-INFINITY" || u == "_INF") {
                result.emplace_back("-INF");
            }
            else if (u == "NAN") {
                result.emplace_back("NAN");
            }
            else {
                // Try to recognize numeric strings and format them using the locale
                // Allows '.' or ',' as a decimal separator
                std::string tmp = s;
                for (char& ch : tmp) { if (ch == ',') ch = '.'; }
                const char* cstr = tmp.c_str();
                char* endp = nullptr;
                errno = 0;
                double val = std::strtod(cstr, &endp);
                if (endp && *endp == '\0' && endp != cstr && errno != ERANGE) {
                    // Numeric string: format with locale (e.g., German -> comma)
                    result.emplace_back(FormatUnit(val, std::string()));
                }
                else {
                    // Non-numeric: take as is
                    result.emplace_back(std::move(s));
                }
            }
            p += MAX_STRING_SIZE;
        }
        break;
    }

    case DBR_ENUM:
    case DBR_TIME_ENUM: {
        // Map enum indices to their string labels when available
        auto idxs = dbrValue2Long();
        if (result.capacity() < result.size() + idxs.size()) {
            result.reserve(result.size() + idxs.size());
        }
        const size_t nlabels = enumStrings_.size();
        for (const long v : idxs) {
            if (v >= 0 && static_cast<size_t>(v) < nlabels && !enumStrings_[static_cast<size_t>(v)].empty()) {
                result.emplace_back(enumStrings_[static_cast<size_t>(v)]);
            }
            else {
                // Fallback to numeric string if out of range or labels not available
                result.emplace_back(std::to_string(v));
            }
        }
        break;
    }

    default: {
        // Format numeric types to string (FormatUnit handles INF/NAN)
        auto nums = dbrValue2Double();
        if (result.capacity() < result.size() + nums.size()) {
            result.reserve(result.size() + nums.size());
        }
        for (const double& v : nums) {
            result.emplace_back(FormatUnit(v, std::string()));
        }
        break;
    }
    }
    return result;
}

std::vector<double> PVItem::dbrValue2Double() const {
    std::vector<double> result;
    const void* data = nativeFieldType_.load();

    const short dbrTypeLocal = dbrType_.load();
    if (!data || dbrTypeLocal < 0 || numberOfValues_ == 0) {
        return result;
    }

    result.reserve(numberOfValues_);

    switch (dbrTypeLocal) {
    case DBR_STRING:
    case DBR_TIME_STRING:
    {
        const char* p = static_cast<const char*>(data);
        if (dbrTypeLocal == DBR_TIME_STRING) {
            p += sizeof(epicsTimeStamp) + 2 * sizeof(dbr_short_t);
        }
        char buf[MAX_STRING_SIZE + 1];
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            // Copy exactly MAX_STRING_SIZE bytes and null-terminate once
            std::memcpy(buf, p, MAX_STRING_SIZE);
            buf[MAX_STRING_SIZE] = '\0';
            char* endp = nullptr;
            double val = std::strtod(buf, &endp);
            // If no conversion happened, fall back to 0.0
            result.push_back(endp != buf ? val : 0.0);
            p += MAX_STRING_SIZE;
        }
    }
    break;

    case DBR_CHAR:
    case DBR_TIME_CHAR:
    {
        const dbr_char_t* p = static_cast<const dbr_char_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        double* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<double>(*p++);
        }
    }
    break;

    case DBR_SHORT:
    case DBR_TIME_SHORT:
    {
        const dbr_short_t* p = static_cast<const dbr_short_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        double* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<double>(*p++);
        }
    }
    break;

    case DBR_LONG:
    case DBR_TIME_LONG:
    {
        const dbr_long_t* p = static_cast<const dbr_long_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        double* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<double>(*p++);
        }
    }
    break;

    case DBR_FLOAT:
    case DBR_TIME_FLOAT:
    {
        const dbr_float_t* p = static_cast<const dbr_float_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        double* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<double>(*p++);
        }
    }
    break;

    case DBR_DOUBLE:
    case DBR_TIME_DOUBLE:
    {
        const dbr_double_t* p = static_cast<const dbr_double_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        double* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = *p++;
        }
    }
    break;

    case DBR_ENUM:
    case DBR_TIME_ENUM:
    {
        const dbr_enum_t* p = static_cast<const dbr_enum_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        double* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<double>(*p++);
        }
    }
    break;

    default:
        CaLabDbgPrintf("dbrValue2Double: Unknown DBR-type: %d", dbrTypeLocal);
    }
    return result;
}

std::vector<long> PVItem::dbrValue2Long() const {
    std::vector<long> result;
    const void* data = nativeFieldType_.load();

    const short dbrTypeLocal2 = dbrType_.load();
    if (!data || dbrTypeLocal2 < 0 || numberOfValues_ == 0) {
        return result;
    }

    result.reserve(numberOfValues_);

    switch (dbrTypeLocal2) {
    case DBR_STRING:
    case DBR_TIME_STRING:
    {
        const char* p = static_cast<const char*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        long* out = result.data() + base;
        char buf[MAX_STRING_SIZE + 1];
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            std::memcpy(buf, p, MAX_STRING_SIZE);
            buf[MAX_STRING_SIZE] = '\0';
            char* endp = nullptr;
            long val = std::strtol(buf, &endp, 10);
            *out++ = (endp != buf) ? val : 0L;
            p += MAX_STRING_SIZE;
        }
    }
    break;

    case DBR_CHAR:
    case DBR_TIME_CHAR:
    {
        const dbr_char_t* p = static_cast<const dbr_char_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        long* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<long>(*p++);
        }
    }
    break;

    case DBR_SHORT:
    case DBR_TIME_SHORT:
    {
        const dbr_short_t* p = static_cast<const dbr_short_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        long* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<long>(*p++);
        }
    }
    break;

    case DBR_LONG:
    case DBR_TIME_LONG:
    {
        const dbr_long_t* p = static_cast<const dbr_long_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        long* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<long>(*p++);
        }
    }
    break;

    case DBR_FLOAT:
    case DBR_TIME_FLOAT:
    {
        const dbr_float_t* p = static_cast<const dbr_float_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        long* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<long>(*p++);
        }
    }
    break;

    case DBR_DOUBLE:
    case DBR_TIME_DOUBLE:
    {
        const dbr_double_t* p = static_cast<const dbr_double_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        long* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<long>(*p++);
        }
    }
    break;

    case DBR_ENUM:
    case DBR_TIME_ENUM:
    {
        const dbr_enum_t* p = static_cast<const dbr_enum_t*>(data);
        const size_t base = result.size();
        result.resize(base + numberOfValues_);
        long* out = result.data() + base;
        for (uInt32 i = 0; i < numberOfValues_; ++i) {
            *out++ = static_cast<long>(*p++);
        }
    }
    break;

    default:
        CaLabDbgPrintf("dbrValue2Long: Unknown DBR-type: %d", dbrTypeLocal2);
    }

    return result;
}

std::string PVItem::getStatusAsString(void) const {
    const short s = status_.load();
    if (s < 0) return "Unknown Status";
    const size_t n = sizeof(alarmStatusString) / sizeof(alarmStatusString[0]);
    if (static_cast<size_t>(s) >= n) return "Unknown Status";
    return alarmStatusString[s];
}

std::string PVItem::getSeverityAsString(void) const {
    const short s = severity_.load();
    if (s < 0) return "Unknown Severity";
    const size_t n = sizeof(alarmSeverityString) / sizeof(alarmSeverityString[0]);
    if (static_cast<size_t>(s) >= n) return "Unknown Severity";
    return alarmSeverityString[s];
}

std::string PVItem::getErrorAsString(void) const {
    // Translate the stored EPICS CA status code (ECA_*) to a message.
    // ca_message returns a const char* description for known codes.
    const int code = errorCode_.load();
    const char* msg = ca_message_safe(static_cast<epicsStatus>(code));
    if (!msg) return std::string();
    return std::string(msg);
}

std::string PVItem::getTimestampAsString(void) const {
    // Cache values locally to minimize atomic accesses
    const std::time_t t = static_cast<std::time_t>(timestamp_.load());
    const uInt32 nsec = timestamp_nsec_.load();

    // Initialize tm struct
    std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    // Use a statically buffered string to avoid std::ostringstream
    char buffer[32];
    // Format: "YYYY-MM-DD HH:MM:SS"
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);

    // Calculate and append microseconds directly
    char result[40];
    std::snprintf(result, sizeof(result), "%s.%06u", buffer, nsec / 1000);

    return std::string(result);
}

/**
 * @brief Formats an integer value with thousands separators and appends a unit, using the system locale.
 *
 * This function converts a 64-bit integer value to a string using the current system locale.
 * If the locale is German, it uses '.' as the thousands separator. For English or other locales, it uses ','.
 * The formatted number is then concatenated with a space and the provided unit string.
 *
 * @param value The integer value to format.
 * @param unit The unit string to append (e.g., "ms", "kg").
 * @return A string containing the formatted value followed by the unit, separated by a space.
 */
std::string PVItem::FormatUnit(double value, std::string unit) const {
    // Hot-path: use cached PREC if available (-1 means unknown)
    int precValue = precCached_.load(std::memory_order_relaxed);

    // Cache only locale information thread-locally
    static thread_local bool tlInit = false;
    static thread_local std::locale tlLocale;
    static thread_local char tlDecimalPoint = '.';
    static thread_local char tlThousandsSep = ',';
    static thread_local double tlPrecThresholds[16]; // 10^-p for p=0..15
    if (!tlInit) {
        try {
            tlLocale = std::locale("");
        }
        catch (const std::exception&) {
            tlLocale = std::locale::classic();
        }

        const auto& punct = std::use_facet<std::numpunct<char>>(tlLocale);
        tlDecimalPoint = punct.decimal_point();
        tlThousandsSep = punct.thousands_sep();

        double v = 1.0;
        for (int p = 0; p <= 15; ++p) {
            tlPrecThresholds[p] = v;
            v /= 10.0;
        }
        tlInit = true;
    }

    auto replace_decimal_before_exp = [&](std::string& str) {
        if (tlDecimalPoint == '.') return; // nothing to do
        size_t expPos = str.find_first_of("eE");
        size_t dotPos = str.find('.');
        if (dotPos != std::string::npos && (expPos == std::string::npos || dotPos < expPos)) {
            str[dotPos] = tlDecimalPoint;
        }
        };

    // If the DBR type is an integer type, always format without decimal places
    const short dbrTypeLocal3 = dbrType_.load();
    const bool isIntegerType =
        dbrTypeLocal3 == DBR_CHAR || dbrTypeLocal3 == DBR_TIME_CHAR ||
        dbrTypeLocal3 == DBR_SHORT || dbrTypeLocal3 == DBR_TIME_SHORT ||
        dbrTypeLocal3 == DBR_LONG || dbrTypeLocal3 == DBR_TIME_LONG ||
        dbrTypeLocal3 == DBR_ENUM || dbrTypeLocal3 == DBR_TIME_ENUM;

    if (isIntegerType) {
        long long iv = static_cast<long long>(value);
        std::string s = std::to_string(iv);
        // Insert thousands separators (no decimal part present)
        const char sep = tlThousandsSep;
        const bool negative = !s.empty() && s[0] == '-';
        const size_t start = negative ? 1u : 0u;
        const size_t n = s.size() - start;
        if (n > 3) {
            std::string out;
            out.reserve(s.size() + n / 3);
            if (negative) out.push_back('-');
            size_t first = n % 3; if (first == 0) first = 3;
            out.append(s, start, first);
            for (size_t i = first; i < n; i += 3) {
                out.push_back(sep);
                out.append(s, start + i, 3);
            }
            s.swap(out);
        }
        if (!unit.empty()) { s.push_back(' '); s += unit; }
        return s;
    }

    std::string s;

    // Spezialwerte zuerst
    if (std::isnan(value)) {
        s = "NAN";
        if (!unit.empty()) { s.push_back(' '); s += unit; }
        return s;
    }
    if (std::isinf(value)) {
        s = (value < 0 ? "-INF" : "INF");
        if (!unit.empty()) { s.push_back(' '); s += unit; }
        return s;
    }

    if (precValue >= 0) {
        const double absV = std::fabs(value);
        const double threshold = (precValue <= 0) ? 1.0 : tlPrecThresholds[precValue > 15 ? 15 : precValue];

        if (absV > 0.0 && absV < threshold) {
            // Too small for fixed representation with PREC: use scientific notation
            char buf[128];
            const int sig = std::max(precValue, 3); // At least 3 significant digits
            std::snprintf(buf, sizeof(buf), "%.*e", sig, value);
            s.assign(buf);
            replace_decimal_before_exp(s);
        }
        else {
            // Normal fixed representation with exactly PREC decimal places
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%.*f", precValue, value);
            s.assign(buf);
            replace_decimal_before_exp(s);

            // Add thousands separators only if needed (no decimal part)
            if (precValue == 0 && s.find('.') == std::string::npos && s.find(',') == std::string::npos) {
                const char sep = tlThousandsSep;
                const bool negative = !s.empty() && s[0] == '-';
                const size_t start = negative ? 1u : 0u;
                const size_t n = s.size() - start;
                if (n > 3) {
                    std::string out;
                    out.reserve(s.size() + n / 3);
                    if (negative) out.push_back('-');
                    size_t first = n % 3; if (first == 0) first = 3;
                    out.append(s, start, first);
                    for (size_t i = first; i < n; i += 3) {
                        out.push_back(sep);
                        out.append(s, start + i, 3);
                    }
                    s.swap(out);
                }
            }
        }
    }
    else {
        const bool useScientific = (std::abs(value) >= 1e6 || (std::abs(value) < 1e-4 && value != 0.0));
        if (useScientific) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%.*e", 6, value);
            s.assign(buf);
            replace_decimal_before_exp(s);
        }
        else {
            // Heuristic against float artifacts: choose the smallest sensible number of decimal places
            // if the value is close to a round decimal number (tolerance 5e-6).
            int chosenPrec = -1;
            for (int p = 0; p <= 6; ++p) {
                double scale = 1.0;
                for (int i = 0; i < p; ++i) scale *= 10.0;
                double r = std::round(value * scale) / scale;
                if (std::fabs(value - r) <= 5e-6) {
                    chosenPrec = p;
                    break;
                }
            }

            char buf[128];
            if (chosenPrec >= 0) {
                std::snprintf(buf, sizeof(buf), "%.*f", chosenPrec, value);
                s.assign(buf);

                // Remove trailing zeros after the '.'
                const size_t decimalPos = s.find('.');
                if (decimalPos != std::string::npos) {
                    size_t lastNonZero = s.find_last_not_of('0');
                    if (lastNonZero != std::string::npos && lastNonZero > decimalPos) {
                        s.erase(lastNonZero + 1);
                    }
                    if (!s.empty() && (s.back() == '.')) {
                        s.pop_back();
                    }
                }

                replace_decimal_before_exp(s);

                // Insert thousands separators if no decimal separators are present
                if (s.find('.') == std::string::npos && s.find(',') == std::string::npos) {
                    const char sep = tlThousandsSep;
                    const bool negative = !s.empty() && s[0] == '-';
                    const size_t start = negative ? 1u : 0u;
                    const size_t n = s.size() - start;
                    if (n > 3) {
                        std::string out;
                        out.reserve(s.size() + n / 3);
                        if (negative) out.push_back('-');
                        size_t first = n % 3; if (first == 0) first = 3;
                        out.append(s, start, first);
                        for (size_t i = first; i < n; i += 3) {
                            out.push_back(sep);
                            out.append(s, start + i, 3);
                        }
                        s.swap(out);
                    }
                }
            }
            else {
                // Fallback: default precision 6 decimal places, then trim zeros
                std::snprintf(buf, sizeof(buf), "%.*f", 6, value);
                s.assign(buf);

                const size_t decimalPos = s.find('.');
                if (decimalPos != std::string::npos) {
                    size_t lastNonZero = s.find_last_not_of('0');
                    if (lastNonZero != std::string::npos && lastNonZero > decimalPos) {
                        s.erase(lastNonZero + 1);
                    }
                    if (!s.empty() && (s.back() == '.')) {
                        s.pop_back();
                    }
                }
                replace_decimal_before_exp(s);

                if (s.find('.') == std::string::npos && s.find(',') == std::string::npos) {
                    const char sep = tlThousandsSep;
                    const bool negative = !s.empty() && s[0] == '-';
                    const size_t start = negative ? 1u : 0u;
                    const size_t n = s.size() - start;
                    if (n > 3) {
                        std::string out;
                        out.reserve(s.size() + n / 3);
                        if (negative) out.push_back('-');
                        size_t first = n % 3; if (first == 0) first = 3;
                        out.append(s, start, first);
                        for (size_t i = first; i < n; i += 3) {
                            out.push_back(sep);
                            out.append(s, start + i, 3);
                        }
                        s.swap(out);
                    }
                }
            }
        }
    }

    if (!unit.empty()) {
        s.push_back(' ');
        s += unit;
    }
    return s;
}

// Store a field's latest string value on the parent PVItem
void PVItem::setFieldString(const std::string& fieldName, const std::string& value) {
    // Fast path: cache PREC numeric without locking for reads later
    if (fieldName == "PREC" || fieldName == "prec") {
        char* endp = nullptr;
        long v = std::strtol(value.c_str(), &endp, 10);
        if (endp != value.c_str()) {
            if (v < 0) v = 0;
            if (v > 15) v = 15;
            precCached_.store(static_cast<int>(v), std::memory_order_relaxed);
        }
    }
    // Keep full history for other consumers
    std::lock_guard<std::mutex> lk(io_mtx_);
    fieldValues_[fieldName] = value;
}

bool PVItem::tryGetFieldString_callerLocked(const std::string& fieldName, std::string& out) const {
    auto it = fieldValues_.find(fieldName);
    if (it == fieldValues_.end()) return false;
    out = it->second;
    return true;
}

// Try to retrieve a field's stored string value
bool PVItem::tryGetFieldString(const std::string& fieldName, std::string& out) const {
    std::lock_guard<std::mutex> lk(io_mtx_);
    auto it = fieldValues_.find(fieldName);
    if (it == fieldValues_.end()) return false;
    out = it->second;
    return true;
}

// Get a snapshot of all stored field string values
std::vector<std::pair<std::string, std::string>> PVItem::getAllFieldStrings() const {
    std::vector<std::pair<std::string, std::string>> res;
    std::lock_guard<std::mutex> lk(io_mtx_);
    res.reserve(fieldValues_.size());
    for (const auto& kv : fieldValues_) res.emplace_back(kv.first, kv.second);
    return res;
}

// Maintain a fast global aggregate error count when this PV's error state changes.
void PVItem::setErrorCode(int code) {
    const int old = errorCode_.load(std::memory_order_relaxed);
    if (old == code) return;
    errorCode_.store(code, std::memory_order_relaxed);

    // Determine whether we should be counted as an error (err > ECA_NORMAL)
    const bool wasErr = isErrorCounted_.load(std::memory_order_relaxed);
    const bool nowErr = (code > (int)ECA_NORMAL);
    if (wasErr != nowErr) {
        isErrorCounted_.store(nowErr, std::memory_order_relaxed);
        // Adjust global counter
        auto& g = Globals::getInstance();
        if (nowErr) {
            g.pvErrorCount.fetch_add(1, std::memory_order_relaxed);
        }
        else {
            g.pvErrorCount.fetch_sub(1, std::memory_order_relaxed);
        }
    }
}
