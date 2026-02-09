#pragma once
#include "calab.h"
#include <queue>
#include <set>
#include <memory>
#include <atomic>
#include <shared_mutex>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <unordered_set>
#include <thread>
#include <functional>

// Manages EPICS CA global state, registry, and coordination across the add-in.
// This class is a thread-safe singleton used by callbacks and API entry points
// to ensure a single, consistent state for the application.
class Globals {
public:
    /**
     * @brief Get the global singleton instance.
     *
     * This method provides access to the single instance of the Globals class.
     * It uses thread-safe lazy initialization, so it can be called from any thread
     * without external locking.
     * @return A reference to the singleton Globals instance.
     */
    static Globals& getInstance() {
        static Globals instance;
        return instance;
    }

    // Legacy flag to control EPICS CA polling.
    bool bCaLabPolling = false;
    // Pointer to the debug log file.
    FILE* pCaLabDbgFile = nullptr;

    // EPICS Channel Access client context.
    ca_client_context* pcac = nullptr;
    // Shared mutex for general-purpose locking.
    mutable std::shared_timed_mutex getLock;
    // Shared mutex to protect the record field cache.
    mutable std::shared_timed_mutex fieldCacheLock;
    // Shared mutex to protect the PV registry.
    mutable std::shared_timed_mutex pvRegistryLock;
    // Registry of all known PVs, keyed by name.
    std::unordered_map<std::string, std::unique_ptr<PVItem>> pvRegistry;

    // Atomic counter for the number of PVs currently in an error state.
    std::atomic<int> pvErrorCount{ 0 };

    // Atomic counter for the number of pending EPICS CA callbacks.
    std::atomic<int> pendingCallbacks{ 0 };
    // List of active LabVIEW instances.
    std::vector<InstanceDataPtr*> instances{ };
    // Mutex to protect access to the instances vector
    mutable std::mutex instancesMutex;
    // Maps LabVIEW array handles to their corresponding instance data.
    std::unordered_map<void*, InstanceDataPtr*> arrayToInstanceMap;
    // Atomic flag indicating if the application is shutting down.
    std::atomic<bool> stopped{ false };

    /**
     * @brief Mark a PVItem as having pending connection-related work.
     *
     * This function is thread-safe and idempotent. It adds the item to an internal
     * set used for coordination and waiting.
     *
     * @param item The PVItem to track. If nullptr, the call is ignored.
     */
    void addPendingConnection(PVItem* item);

    /**
     * @brief Remove a PVItem from the pending-connection set.
     *
     * This is called after connection-related work for an item is complete.
     * It is thread-safe and a no-op if the item is not tracked.
     * Note: This does not notify waiters; call notify() separately if needed.
     *
     * @param item The PVItem to untrack. If nullptr, the call is ignored.
     */
    void removePendingConnection(PVItem* item);

    /**
     * @brief Clear the entire pending-connection set.
     *
     * This is a best-effort cleanup used during teardown. It is thread-safe.
     * Note: This does not emit notifications.
     */
    void clearPendingConnections();

    /**
     * @brief Get a snapshot of PVItems with pending connection work.
     *
     * This function is thread-safe and returns a copy to avoid callers
     * holding locks.
     *
     * @return A vector containing pointers to all PVItems currently tracked as pending.
     */
    std::vector<PVItem*> getPendingConnections() const;

    /**
     * @brief Wait for a notification or a timeout.
     *
     * This is intended for threads that poll for connection or value updates.
     * It is thread-safe, allowing multiple threads to wait concurrently.
     *
     * @param timeout Maximum duration to wait.
     */
    void waitForNotification(std::chrono::milliseconds timeout);

    /**
     * @brief Notify waiting threads about state changes.
     *
     * This wakes all threads blocked in waitForNotification(), typically after
     * connection or value updates have occurred. It is thread-safe.
     */
    void notify();

    /**
     * @brief Associate a LabVIEW memory handle with its instance data.
     *
     * This is used to route callbacks to the correct LabVIEW instance.
     * It is thread-safe.
     *
     * @param arrayPtr Opaque LabVIEW handle pointer used as a lookup key.
     * @param instance Pointer to the owning InstanceDataPtr for this array.
     */
    void registerArrayToInstance(void* arrayPtr, InstanceDataPtr* instance);

    /**
     * @brief Retrieve the instance data for a LabVIEW array handle.
     *
     * This function is thread-safe.
     *
     * @param arrayPtr Opaque LabVIEW handle pointer used as a lookup key.
     * @return Pointer to the InstanceDataPtr for the array, or nullptr if not found.
     */
    InstanceDataPtr* getInstanceForArray(void* arrayPtr);

    /**
     * @brief Register a background worker's stop function.
     *
     * The provided function will be called during teardown. If a worker with the
     * same name already exists, it will be replaced. This is thread-safe.
     *
     * @param name   A unique identifier for the worker.
     * @param stopFn A callable function to stop the worker. It should be idempotent and non-throwing.
     */
    void registerBackgroundWorker(const std::string& name, std::function<void()> stopFn);

    /**
     * @brief Stop all registered background workers.
     *
     * This is used during teardown to ensure a clean shutdown. It is thread-safe
     * and invokes each stop function in an exception-safe manner.
     */
    void stopBackgroundWorkers();

    /**
     * @brief Check if a field name is a common EPICS record field.
     *
     * Common fields are generic across record types (e.g., VAL, DESC, STAT, SEVR).
     *
     * @param fieldName The field name without the record prefix (e.g., "VAL").
     * @return true if the field is a common field, false otherwise.
     */
    bool recordFieldIsCommonField(const std::string& fieldName);

    /**
     * @brief Determine if a field exists for a given EPICS record type.
     *
     * This may consult an internal cache of record type definitions.
     *
     * @param recordTypeStr The EPICS record type as a string (e.g., "ai", "bo").
     * @param fieldName The field name without the record prefix (e.g., "EGU").
     * @return true if the field exists for this record type, false otherwise.
     */
    bool recordFieldExists(const std::string& recordTypeStr, const std::string& fieldName);

    void unregisterArraysForInstance(InstanceDataPtr* instance);

    /**
     * @brief Defer posting a LabVIEW user event for a PV name.
     *
     * Used when the PV registry lock is contended; names are coalesced and
     * posted later by the CA polling thread.
     *
     * @param pvName PV name to post later.
     */
    void enqueueDeferredEvent(const std::string& pvName);

    /**
     * @brief Drain all deferred event names.
     *
     * Returns a snapshot and clears the internal set.
     */
    std::vector<std::string> drainDeferredEvents();

    // Disable copy constructor and copy assignment operator.
    Globals(const Globals&) = delete;
    Globals& operator=(const Globals&) = delete;

private:
    /**
     * @brief Private constructor to enforce the singleton pattern.
     *
     * Initializes the global state and the EPICS CA context. It also starts a
     * polling thread if context initialization is successful.
     */
    Globals();

    /**
     * @brief Destructor for graceful shutdown.
     *
     * Stops workers and polling, closes the debug file, and clears the PV registry.
     * Ensures EPICS CA resources are released and threads are joined.
     */
    ~Globals();

    // Mutex for pendingConnections set.
    mutable std::mutex pendingConnectionsMutex;
    // Set of PVItems with pending connection or value updates.
    std::unordered_set<PVItem*> pendingConnections;

    // Mutex for the notification condition variable.
    mutable std::mutex notificationMutex;
    // Condition variable for connection/value progress notifications.
    std::condition_variable notificationCv;
    // Thread for polling EPICS CA events.
    std::thread pollThread_;

    // Mutex for the backgroundWorkers vector.
    mutable std::mutex workersMutex_;
    // Registered background workers to be stopped during teardown.
    std::vector<std::pair<std::string, std::function<void()>>> backgroundWorkers_;

    // Deferred PV event names for later posting.
    mutable std::mutex deferredEventsMutex_;
    std::unordered_set<std::string> deferredEvents_;
};

//==================================================================================================
// LOCK HIERARCHY (MUST BE FOLLOWED TO AVOID DEADLOCKS)
//==================================================================================================
// ALL CODE MUST ACQUIRE LOCKS IN THIS ORDER:
//
// 1. Globals::getLock            (shared_timed_mutex, exclusive in getValue/putValue)
// 2. Globals::pvRegistryLock     (shared_timed_mutex, shared/unique)
// 3. PVItem::ioMutex()           (mutex, per-PV)
// 4. MyInstanceData::arrayMutex  (mutex, per-VI-instance)
// 5. g_eventRegistry.mtx         (mutex, global event registry)
//
// NEVER ACQUIRE LOCKS IN REVERSE ORDER!
//
// putValue Double-Checked Locking Pattern:
//   1. Try Shared Lock → Read-Only Lookup (Fast Path)
//   2. If PV missing: Upgrade to Unique Lock → Create PV (Slow Path)
//   3. Always re-check after acquiring Unique Lock (Race Condition Protection)
//
// Example (CORRECT):
//   TimeoutSharedLock<std::shared_timed_mutex> sharedLock(pvRegistryLock, ...);
//   if (pvExists) return pv; // ✅ Fast Path
//   // Release Shared Lock (end of scope)
//   TimeoutUniqueLock<std::shared_timed_mutex> uniqueLock(pvRegistryLock, ...);
//   if (pvExists) return pv; // ✅ Double-Check
//   createPv(); // ✅ Safe Creation
//==================================================================================================
