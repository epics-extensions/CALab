// PVItem.h
#pragma once

#include <string>
#include <atomic>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <iomanip>
#include "epics_compat.h"
#include "calab.h"

// EPICS CA status compatibility: ensure macros exist even if EPICS headers are unavailable here.
#ifndef ECA_NORMAL
#define ECA_NORMAL 1
#endif
#ifndef ECA_DISCONNCHID
#define ECA_DISCONNCHID (ECA_NORMAL + 1)
#endif

/**
 * @class PVItem
 * @brief Represents a single Process Variable (PV) from EPICS.
 *
 * This class encapsulates the data and metadata for a PV, including its value,
 * timestamp, status, severity, and other attributes. It provides thread-safe
 * access to its properties and manages the lifecycle of the underlying EPICS
 * channel access data.
 */
class PVItem {
public:
    PVItem();
    PVItem(
        const std::string& name, 
        chanId cid = nullptr, 
        short dbrType = 0,
        uInt32 numVals = 0, 
        const std::string& recType = "",
        void* dataBuffer = nullptr, 
        uInt32 ts = 0, 
        uInt32 nsec = 0, 
        int16_t stat = epicsAlarmComm,
        int16_t sev = epicsSevInvalid, 
        bool connected = false, 
        bool hasVal = false,
        bool passive = false, 
        PVItem* parent = nullptr, 
        evid evId = nullptr,
        const std::vector<std::pair<std::string, 
        chanId>>& fields = {}
    );
    PVItem(const PVItem& other);
    PVItem& operator=(const PVItem& other);
    ~PVItem();

    // Getters (alphabetically sorted by method name)
    std::vector<std::pair<std::string, std::string>> getAllFieldStrings() const;
    const void* getDbr() const;
    short getDbrType() const;
    const std::vector<std::string>& getEnumStrings() const { return enumStrings_; }
    int getErrorCode() const { return errorCode_.load(); }
    const std::vector<std::pair<std::string, chanId>>& getFields() const;
    const std::string& getName() const;
    uInt32 getNumberOfValues() const;
    const std::string& getRecordType() const;
    int16_t getSeverity() const;
    int16_t getStatus() const;
    uInt32 getTimestamp() const;
    uInt32 getTimestampNSec() const;
    void* getUserData() const;
    bool hasChanged(std::size_t lastHash) const { return changeHash_.load() != lastHash; }
    bool hasValue() const;
    bool isConnected() const;
    bool isPassive() const;
    bool tryGetFieldString(const std::string& fieldName, std::string& out) const;
    bool tryGetFieldString_callerLocked(const std::string& fieldName, std::string& out) const;

    // Setters (alphabetically sorted by method name)
    void setCallbackContext(std::atomic<int>* pendingCallbacks, std::condition_variable* cv, std::mutex* mtx, void* data = nullptr);
    void setConnected(bool connected);
    void setDbr(void* newDbr);
    void setDbrType(short type);
    void setEnumValue(const dbr_ctrl_enum* enumValue);
    void setErrorCode(int code);
    void setFields(const std::vector<std::pair<std::string, chanId>>& fields);
    void setFieldString(const std::string& fieldName, const std::string& value);
    void setHasValue(bool hasValue);
    void setName(const std::string& name);
    void setNumberOfValues(uInt32 num);
    void setPassive(bool passive);
    void setRecordType(const std::string& type);
    void setSeverity(int16_t sev);
    void setStatus(int16_t status);
    void setTimestamp(uInt32 ts);
    void setTimestampNSec(uInt32 nsec);
    template<typename T>
    inline void setUserData(T* data) { userData_ = data; } // define in header to avoid linker issues

    // Provides thread-safe access to the main mutex.
    std::mutex& ioMutex() const { return io_mtx_; }

    // Conversion helpers (alphabetically sorted by method name)
    std::vector<double> dbrValue2Double() const;
    std::vector<long> dbrValue2Long() const;
    std::vector<std::string> dbrValue2String() const;
    std::string FormatUnit(double value, std::string unit) const;
    std::string getErrorAsString() const;
    std::string getSeverityAsString() const;
    std::string getStatusAsString() const;
    std::string getTimestampAsString() const;

    // Enum metadata fetch coordination (alphabetically sorted)
    void clearEnumFetchRequested() { enumFetchRequested_.store(false); }
    bool tryMarkEnumFetchRequested() { bool expected = false; return enumFetchRequested_.compare_exchange_strong(expected, true); }

    // Hash functions for change detection
    std::size_t getChangeHash() const {
        return changeHash_.load();
    }
    void updateChangeHash() {
        // Incorporate seconds and nanoseconds to distinguish multiple updates within the same second
        const std::size_t tsSec = static_cast<std::size_t>(timestamp_.load());
        const std::size_t tsNsec = static_cast<std::size_t>(timestamp_nsec_.load());
        const std::size_t stat = static_cast<std::size_t>(status_.load());
        const std::size_t sevr = static_cast<std::size_t>(severity_.load());
        const std::size_t ptr = reinterpret_cast<std::size_t>(nativeFieldType_.load());
        // Mix fields using FNV-1a hash algorithm for a good distribution.
#if UINTPTR_MAX == 0xffffffffu
        constexpr std::size_t kFNVOffset = 2166136261u;
        constexpr std::size_t kFNVPrime = 16777619u;
#else
        constexpr std::size_t kFNVOffset = 1469598103934665603ull;
        constexpr std::size_t kFNVPrime = 1099511628211ull;
#endif
        std::size_t h = kFNVOffset; // FNV offset basis
        h ^= tsSec;  h *= kFNVPrime;
        h ^= tsNsec; h *= kFNVPrime;
        h ^= stat;   h *= kFNVPrime;
        h ^= sevr;   h *= kFNVPrime;
        h ^= (ptr >> 4); h *= kFNVPrime; // Shift pointer to improve hash quality
        changeHash_.store(h);
    }

    // Memory management
    void clearDbr();
    void clearFields();

    // Public members for direct access (handle with care)
    PVItem* parent;
    std::atomic<bool> toBeRemoved{ false };
    chanId channelId;
    evid eventId;

    // Other methods
    std::string info() const;

private:
    // Member data
    std::atomic<std::size_t> changeHash_{ 0 };
    std::atomic<short> dbrType_;
    // Guard to avoid issuing duplicate enum metadata requests while one is pending.
    std::atomic<bool> enumFetchRequested_{ false };
    std::vector<std::string> enumStrings_;
    dbr_ctrl_enum enumValue;
    // Last EPICS CA error/status code associated with this PV (ECA_*).
    // Defaults to a disconnect state until the first successful connection/value.
    std::atomic<int> errorCode_{ ECA_DISCONNCHID };
    // Latest string values for fields (e.g., "base.VAL"); guarded by io_mtx_.
    std::unordered_map<std::string, std::string> fieldValues_;
    std::vector<std::pair<std::string, chanId>> fields_;
    std::atomic<bool> hasValue_;
    mutable std::mutex io_mtx_;
    std::atomic<bool> isConnected_;
    // Tracks whether this PV currently contributes to the global PV error count.
    std::atomic<bool> isErrorCounted_{ false };
    std::atomic<bool> isPassive_;
    std::string name_;
    std::atomic<void*> nativeFieldType_{ nullptr };
    uInt32 numberOfValues_;
    // Cached precision from the PREC field (-1 = not set/unknown). Accessed lock-free.
    std::atomic<int> precCached_{ -1 };
    std::string recordType_;
    std::atomic<int16_t> severity_;
    std::atomic<int16_t> status_;
    std::atomic<uInt32> timestamp_;
    std::atomic<uInt32> timestamp_nsec_{ 0 };
    void* userData_;
};
