//==================================================================================================
// Name      : calab.cpp
// Authors   : Carsten Winkler, Brian Powell
// Version   : 1.8.0.3
// Copyright : HZB
// Description: Modernized CALab core for reading/writing EPICS PVs and LabVIEW events
// GitHub    : https://github.com/epics-extensions/CALab
//
// The following terms apply to all files associated with the software. HZB hereby grants
// permission to use, copy, and modify this software and its documentation for non-commercial
// educational or research purposes, provided that existing copyright notices are retained
// in all copies. The receiver of the software provides HZB with all enhancements, including
// complete translations, made by the receiver.
// IN NO EVENT SHALL HZB BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL,
// OR CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR
// ANY DERIVATIVES THEREOF, EVEN IF HZB HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// HZB SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
// THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, AND HZB HAS NO OBLIGATION TO PROVIDE
// MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
//==================================================================================================

#include <locale.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <shared_mutex>
#include <cstdarg>
#include <ctime>
#include <cctype>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <limits>
#include <cstring>
#include <cstdio>
#include "calab.h"
#include "TimeoutUniqueLock.h"
#include "globals.h"
#include <cinttypes>
#if defined _WIN32 || defined _WIN64
#include <alarm.h>
#include <epicsVersion.h>
#include <envDefs.h>
#include <epicsStdio.h>
#else
#include "epics_compat.h"
#include <epicsVersion.h>
#endif

#ifndef CALAB_VERSION
#define CALAB_VERSION "1.8.0.3"
#endif


// Utility for building a vector of sequential integers.
namespace {
	inline std::vector<uInt32> makeIndexRange(uInt32 n) {
		std::vector<uInt32> v;
		v.reserve(n);
		for (uInt32 i = 0; i < n; ++i) v.push_back(i);
		return v;
	}
}

namespace {
	// RAII guard to ensure the current thread is attached to the EPICS CA context.
	struct CaContextGuard {
		bool attached = false;
		CaContextGuard() {
			Globals& g = Globals::getInstance();
			// Attach only if this thread does not already have a context.
			if (ca_current_context() == nullptr && g.pcac) {
				if (ca_attach_context(g.pcac) == ECA_NORMAL) {
					attached = true;
				}
			}
		}
		~CaContextGuard() {
			if (attached) {
				ca_detach_context();
			}
		}
	};
}

static_assert(sizeof(uintptr_t) == sizeof(void*), "calab: uintptr_t must be same size as void* (pointer-sized integer required)");
using atomic_ptr_t = std::atomic<uintptr_t>;
static bool try_schedule_deletion(PvIndexEntry* entry);
static void mark_deletion_done(PvIndexEntry* entry) noexcept;

// Attempt to atomically load the array slot and acquire a usage-ref for the entry.
// Guarantees that the returned entry was still the current element when acquired.
// If the slot is empty or acquisition fails, returns nullptr.
// Note: eltPtr must point to the array element storage (interpreted as atomic_ptr_t).
static PvIndexEntry* acquireEntryFromElt(void* eltPtr) noexcept {
	if (!eltPtr) return nullptr;
	auto atomicElt = reinterpret_cast<atomic_ptr_t*>(eltPtr);

	for (;;) {
		uintptr_t raw = atomicElt->load(std::memory_order_acquire);
		if (!raw) return nullptr;

		PvIndexEntry* entry = reinterpret_cast<PvIndexEntry*>(raw);
		if (!entry) return nullptr;

		{
			if (!isLiveEntry(entry)) {
				uintptr_t expected = raw;
				(void)atomicElt->compare_exchange_strong(
					expected, 0, std::memory_order_acq_rel, std::memory_order_acquire);
				return nullptr;
			}

			if (!entry->try_acquire()) {
				return nullptr;
			}
		}

		uintptr_t now = atomicElt->load(std::memory_order_acquire);
		if (now == raw) {
			return entry;
		}

		entry->release();
	}
}


// Release an acquired entry reference.
static void releaseEntry(PvIndexEntry* entry) noexcept {
	if (!entry) return;
	entry->release();
}

static bool tryReclaimAndDelete(PvIndexEntry* entry,
	std::chrono::seconds hardTimeout = std::chrono::seconds(30)) noexcept {
	if (!entry) return true;

	if (!try_schedule_deletion(entry)) {
		return true;
	}

	if (!isLiveEntry(entry)) {
		mark_deletion_done(entry);
		return true;
	}

	Globals& g = Globals::getInstance();

	entry->mark_deleting();

	if (hardTimeout.count() > 0) {
		const auto start = std::chrono::steady_clock::now();
		while (entry->current_refs() != 0) {
			ca_pend_event(0.001);
			g.waitForNotification(std::chrono::milliseconds(20));

			if (std::chrono::steady_clock::now() - start > hardTimeout) {
				mark_deletion_done(entry);
				return false;
			}
		}
	}
	else {
		if (entry->current_refs() != 0) {
			CaLabDbgPrintf("tryReclaimAndDelete: immediate-delete requested but refs=%u for %p",
				entry->current_refs(), static_cast<void*>(entry));
			mark_deletion_done(entry);
			return false;
		}
	}

	if (entry->metaInfo) { delete entry->metaInfo; entry->metaInfo = nullptr; }
	entry->pvItem = nullptr;

#if defined(_MSC_VER)
#	include <crtdbg.h>
	if (_CrtIsValidHeapPointer(entry)) {
		mark_deletion_done(entry);
		delete entry;
	}
	else {
		CaLabDbgPrintf("tryReclaimAndDelete: entry %p invalid at deletion time; skipping delete",
			static_cast<void*>(entry));
		mark_deletion_done(entry);
	}
#else
	mark_deletion_done(entry);
	delete entry;
#endif

	return true;
}


// Lightweight RAII wrapper for acquired PvIndexEntry references.
struct PvEntryHandle {
	PvIndexEntry* entry = nullptr;
	PvEntryHandle() noexcept = default;

	// Disable copy to avoid double-release
	PvEntryHandle(const PvEntryHandle&) noexcept = delete;
	PvEntryHandle& operator=(const PvEntryHandle&) noexcept = delete;

	// Move semantics: transfer ownership of the acquired ref
	PvEntryHandle(PvEntryHandle&& other) noexcept : entry(other.entry) { other.entry = nullptr; }
	PvEntryHandle& operator=(PvEntryHandle&& other) noexcept {
		if (this != &other) {
			if (entry) releaseEntry(entry);
			entry = other.entry;
			other.entry = nullptr;
		}
		return *this;
	}

	PvEntryHandle(sLongArrayHdl* arr, size_t idx) noexcept {
		if (!arr || !*arr || !**arr) return;
		if (idx >= (**arr)->dimSize) return;
		// pass pointer to the element storage (element interpreted as pointer-sized integer)
		entry = acquireEntryFromElt(&(**arr)->elt[static_cast<uInt32>(idx)]);
	}
	~PvEntryHandle() noexcept { if (entry) releaseEntry(entry); }

	PvIndexEntry* get() const noexcept { return entry; }
	explicit operator bool() const noexcept { return entry != nullptr; }
	PvIndexEntry* operator->() const noexcept { return entry; }
	PvIndexEntry& operator*() const noexcept { return *entry; }
};
// Deletion scheduler to avoid double-delete races when same entry pointer appears in multiple arrays.
static std::mutex g_deletionMutex;
static std::unordered_set<PvIndexEntry*> g_deletionScheduled;

static bool try_schedule_deletion(PvIndexEntry* entry) {
	if (!entry) return false;
	std::lock_guard<std::mutex> lk(g_deletionMutex);
	auto res = g_deletionScheduled.insert(entry);
	return res.second; // true if inserted (we are owner), false if already scheduled
}

static void mark_deletion_done(PvIndexEntry* entry) noexcept {
	if (!entry) return;
	std::lock_guard<std::mutex> lk(g_deletionMutex);
	g_deletionScheduled.erase(entry);
}

static inline void sanitizePvIndexArray(sLongArray* arr, size_t dim) noexcept {
#if UINTPTR_MAX == 0xffffffffu
	if (!arr) return;
	for (size_t i = 0; i < dim; ++i) {
		arr->elt[i] = static_cast<uint64_t>(static_cast<uintptr_t>(arr->elt[i]));
	}
#else
	(void)arr;
	(void)dim;
#endif
}

static void deferredDeletionWorker(std::vector<PvIndexEntry*> entries) {
	const auto hardTimeout = std::chrono::seconds(60);
	for (PvIndexEntry* entry : entries) {
		if (!entry) continue;
		if (!tryReclaimAndDelete(entry, hardTimeout)) {
			CaLabDbgPrintf("deferredDeletionWorker: could not reclaim %p within timeout; skipping for now.", static_cast<void*>(entry));
		}
	}
}

inline bool tryClaimAndDeletePvIndexEntry(void* eltPtr) {
	if (!eltPtr) return false;
	auto atomicElt = reinterpret_cast<atomic_ptr_t*>(eltPtr);
	uintptr_t raw = atomicElt->load(std::memory_order_acquire);
	if (raw == 0) return false;
	uintptr_t expected = raw;
	// Try to atomically replace current slot with 0 (claim it)
	if (!atomicElt->compare_exchange_strong(expected, 0, std::memory_order_acq_rel, std::memory_order_acquire)) {
		return false;
	}

	PvIndexEntry* entry = reinterpret_cast<PvIndexEntry*>(raw);
	if (!entry) return false;

	// Try to become the canonical deleter for this entry.
	if (!try_schedule_deletion(entry)) {
		CaLabDbgPrintf("tryClaimAndDeletePvIndexEntry: deletion already scheduled for %p -> skipping", static_cast<void*>(entry));
		// Slot cleared by us; another path will perform (or has performed) deletion.
		return true;
	}

	// Prevent further acquirers
	entry->mark_deleting();

	// Wait for concurrent users to release their refs.
	Globals& g = Globals::getInstance();
	const auto start = std::chrono::steady_clock::now();
	const auto hardTimeout = std::chrono::seconds(30); // conservative safety timeout
	while (entry->current_refs() != 0) {
		// Let CA and other threads make progress, and wait for notifications
		ca_pend_event(0.001);
		g.waitForNotification(std::chrono::milliseconds(20));

		// Log and break-to-defer on suspiciously large refcounts (defensive)
		const unsigned refs = entry->current_refs();
		if (refs > (1u << 24)) {
			CaLabDbgPrintf("tryClaimAndDeletePvIndexEntry: suspicious refcount %u for %p during delete; deferring.", refs, static_cast<void*>(entry));
			// schedule deferred deletion by spawning worker with single entry
			std::vector<PvIndexEntry*> v;
			v.push_back(entry);
			std::thread(deferredDeletionWorker, std::move(v)).detach();
			// We already own deletion token but deferredDeletionWorker will call tryReclaimAndDelete (which uses the token logic)
			mark_deletion_done(entry);
			return true;
		}

		if (std::chrono::steady_clock::now() - start > hardTimeout) {
			CaLabDbgPrintf("tryClaimAndDeletePvIndexEntry: timeout waiting for refs to drop (%u remain). Proceeding with caution.", entry->current_refs());
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	}

	// Now safe to delete (we own the deletion token)
	if (entry->metaInfo) {
		delete entry->metaInfo;
		entry->metaInfo = nullptr;
	}
	entry->pvItem = nullptr;
	mark_deletion_done(entry);
	delete entry;

	return true;
}


// =================================================================================
// Global Variables & Asynchronous Worker
// =================================================================================

DWORD dllStatus = DLL_PROCESS_DETACH;
uInt32 globalCounter = 0;

#if defined _WIN32 || defined _WIN64
// Standard Windows DLL entry point to track process attachment/detachment.
BOOLEAN WINAPI DllMain(HINSTANCE hDllHandle, DWORD nReason, LPVOID Reserved) {
	dllStatus = nReason;
	switch (nReason) {
	case DLL_PROCESS_ATTACH: {
		break;
	}
	case DLL_PROCESS_DETACH: {
		break;
	}
	}
	return 1;
};
#endif

namespace {
	// Task structure to hold data from a CA value-changed event for background processing.
	struct ValueChangeTask {
		std::string pvName;
		int type = 0;
		unsigned nElems = 0;
		int errorCode = ECA_NORMAL; // EPICS CA status for this event (ECA_*).
		bool hasTimeMeta = false;
		uint32_t secPastEpoch = 0;
		uint32_t nsec = 0;
		uint16_t status = 0;
		uint16_t severity = 0;
		size_t dataBytes = 0;
		void* dataCopy = nullptr; // Ownership is transferred to PVItem; freed here on failure.
	};

	// Globals for the value-changed worker queue.
	std::mutex g_vcMutex;
	std::condition_variable g_vcCv;
	std::deque<ValueChangeTask> g_vcQueue;
	std::atomic<bool> g_vcStop{ false };
	std::atomic<bool> g_vcStarted{ false };
	std::mutex g_vcStartMtx;
	std::thread g_vcThread;


	// Stops the background worker thread and drains any remaining tasks.
	void stopWorker() {
		if (!g_vcStarted.load()) return;
		g_vcStop.store(true);
		g_vcCv.notify_all();
		if (g_vcThread.joinable()) {
			g_vcThread.join();
		}
		g_vcStarted.store(false);
	}

	// Processes a single value change task from the queue.
	void processValueChangeTask(ValueChangeTask& task) {
		Globals& g = Globals::getInstance();
		PVItem* pvItem = nullptr;

		// (1) Lookup PV item with a shared lock to allow concurrent lookups.
		{
			TimeoutSharedLock<std::shared_timed_mutex> pvRegistrySharedLock(
				g.pvRegistryLock, "valueChanged-lookup-worker", std::chrono::milliseconds(200));
			if (!pvRegistrySharedLock.isLocked()) {
				CaLabDbgPrintf("Error: Failed to acquire shared lock for %s in worker (lookup).", task.pvName.c_str());
				if (task.dataCopy) {
					try {
						free(task.dataCopy);
					} catch (...) {
						CaLabDbgPrintf("Error: Exception #1 during free() of dataCopy for PV %s", task.pvName.c_str());
					}
					task.dataCopy = nullptr;
				}
				return;
			}
			auto it = g.pvRegistry.find(task.pvName);
			if (it == g.pvRegistry.end()) {
				CaLabDbgPrintf("Worker: PV %s not found in registry", task.pvName.c_str());
				if (task.dataCopy) {
					try {
						free(task.dataCopy);
					}
					catch (...) {
						CaLabDbgPrintf("Error: Exception #2 during free() of dataCopy for PV %s", task.pvName.c_str());
					}
					task.dataCopy = nullptr;
				}
				return;
			}
			pvItem = it->second.get();
		}

		// (2) Acquire a short exclusive lock on the PVItem to update its data.
		{
			TimeoutUniqueLock<std::mutex> itemLock(
				pvItem->ioMutex(),
				"valueChanged-item-worker",
				std::chrono::milliseconds(200)
			);
			if (!itemLock.isLocked()) {
				CaLabDbgPrintf("Error: Failed to acquire unique lock for %s in worker (item).", task.pvName.c_str());
				if (task.dataCopy) {
					try {
						free(task.dataCopy);
					}
					catch (...) {
						CaLabDbgPrintf("Error: Exception #3 during free() of dataCopy for PV %s", task.pvName.c_str());
					}
					task.dataCopy = nullptr;
				}
				return;
			}

			pvItem->setNumberOfValues(task.nElems);
			pvItem->setErrorCode(task.errorCode);

			if (task.hasTimeMeta) {
				pvItem->setTimestamp(task.secPastEpoch);
				pvItem->setTimestampNSec(task.nsec);
				pvItem->setStatus(task.status);
				pvItem->setSeverity(task.severity);
			}

			// Transfer ownership of the data buffer to the PVItem.
			if (task.dataCopy) {
				pvItem->clearDbr();
				pvItem->setDbr(task.dataCopy);
				task.dataCopy = nullptr; // Ownership moved.
				pvItem->setHasValue(true);
			}
		}

		// If this is an .RTYP PV, update the parent's record type.
		const bool isRtypeString = task.pvName.size() >= 5 && task.pvName.compare(task.pvName.size() - 5, 5, ".RTYP") == 0;
		if (isRtypeString) {
			PVItem* parentPvItem = nullptr;
			{
				// No need to lock the registry; the parent pointer is on the pvItem itself.
				std::lock_guard<std::mutex> lock(pvItem->ioMutex());
				parentPvItem = pvItem->parent;
			}
			if (parentPvItem) {
				std::string recordType;
				{
					// Derive recordType from the freshly set value.
					std::lock_guard<std::mutex> lock(pvItem->ioMutex());
					auto values = pvItem->dbrValue2String();
					if (!values.empty()) recordType = values[0];
				}
				if (!recordType.empty()) {
					TimeoutUniqueLock<std::mutex> parentLock(
						parentPvItem->ioMutex(),
						"valueChanged-parent-worker",
						std::chrono::milliseconds(200)
					);
					if (parentLock.isLocked()) {
						pvItem->parent->setRecordType(recordType);
					}
					else {
						CaLabDbgPrintf("Error: Failed to acquire unique lock for parent of %s in worker (RTYP).", task.pvName.c_str());
					}
				}
			}
		}

		// If this is a field PV (e.g., 'base.FIELD' but not '.RTYP'), store its string value on the parent.
		{
			// Fast check: contains a dot and does not end with .RTYP.
			const size_t dotPos = task.pvName.find('.');
			if (dotPos != std::string::npos && !isRtypeString) {
				PVItem* parentPvItem = nullptr;
				{
					std::lock_guard<std::mutex> lock(pvItem->ioMutex());
					parentPvItem = pvItem->parent;
				}
				if (parentPvItem) {
					// Extract field name and its most recent string value.
					const std::string fieldName = task.pvName.substr(dotPos + 1);
					std::string fieldString;
					{
						std::lock_guard<std::mutex> lock(pvItem->ioMutex());
						// Use string conversion for strings/enums, and numeric->string otherwise.
						auto strValues = pvItem->dbrValue2String();
						if (!strValues.empty()) {
							fieldString = strValues[0];
						}
						else {
							auto numValues = pvItem->dbrValue2Double();
							if (!numValues.empty()) {
								// Store numeric field as a plain integer-like string if it's close to an integer.
								double value = numValues[0];
								long longValue = static_cast<long>(value);
								if (std::fabs(value - static_cast<double>(longValue)) < 0.0005 && std::fabs(value) < 32768.0) {
									fieldString = std::to_string(longValue);
								}
								else {
									fieldString = std::to_string(value);
								}
							}
						}
					}
					if (!fieldName.empty() && !fieldString.empty()) {
						// `setFieldString` performs its own locking; avoid double-locking the same mutex.
						parentPvItem->setFieldString(fieldName, fieldString);
					}
				}
			}
		}

		g.removePendingConnection(pvItem);

		// Notify subscribers about the update for this PV and its parent (for field changes).
		postEventForPv(task.pvName);
		if (pvItem && pvItem->parent) {
			postEventForPv(pvItem->parent->getName());
		}
		g.notify();
	}

	// Main loop for the background worker thread.
	void workerLoop() {
		while (!g_vcStop.load()) {
			ValueChangeTask task;
			{
				std::unique_lock<std::mutex> lock(g_vcMutex);
				g_vcCv.wait(lock, [] { return g_vcStop.load() || !g_vcQueue.empty(); });
				if (g_vcStop.load()) break;
				task = std::move(g_vcQueue.front());
				g_vcQueue.pop_front();
			}
			processValueChangeTask(task);
			if (task.dataCopy) { // In case processing failed before ownership transfer.
				try {
					free(task.dataCopy);
				}
				catch (...) {
					CaLabDbgPrintf("Error: Exception #4 during free() of dataCopy for PV %s", task.pvName.c_str());
				}
				task.dataCopy = nullptr;
			}
		}

		// Drain any remaining tasks on stop.
		for (;;) {
			ValueChangeTask task;
			{
				std::lock_guard<std::mutex> lock(g_vcMutex);
				if (g_vcQueue.empty()) break;
				task = std::move(g_vcQueue.front());
				g_vcQueue.pop_front();
			}
			processValueChangeTask(task);
			if (task.dataCopy) {
				try {
					free(task.dataCopy);
				}
				catch (...) {
					CaLabDbgPrintf("Error: Exception #5 during free() of dataCopy for PV %s", task.pvName.c_str());
				}
				task.dataCopy = nullptr;
			}
		}
	}

	// Ensures the background worker thread is started, creating it if necessary.
	void ensureWorkerStarted() {
		if (g_vcStarted.load()) return;
		std::lock_guard<std::mutex> lock(g_vcStartMtx);
		if (g_vcStarted.load()) return;
		g_vcStop.store(false);
		g_vcThread = std::thread(workerLoop);
		g_vcStarted.store(true);
		// Register a stop hook with Globals to ensure clean teardown.
		Globals::getInstance().registerBackgroundWorker("valueChangedWorker", [] { stopWorker(); });
	}

	// Enqueues a new task for the background worker.
	void enqueueTask(ValueChangeTask&& task) {
		ensureWorkerStarted();
		{
			std::lock_guard<std::mutex> lock(g_vcMutex);
			g_vcQueue.emplace_back(std::move(task));
		}
		g_vcCv.notify_one();
	}

	// Manages LabVIEW User Event registrations for PVs.
	struct EventRegistry {
		std::mutex mtx;
		std::unordered_map<std::string, std::vector<std::pair<LVUserEventRef, sResult*>>> map;
	} g_eventRegistry;

} // end anonymous namespace

namespace {
	// Associates LabVIEW array handles with a specific VI instance to prevent data corruption
	// in multi-VI scenarios. This is crucial for reentrant VIs.
	void bindArraysToInstance(const char* functionName,
		sLongArrayHdl* PvIndexArray,
		sResultArrayHdl* ResultArray,
		sStringArrayHdl* FirstStringValue,
		sDoubleArrayHdl* FirstDoubleValue,
		sDoubleArray2DHdl* DoubleValueArray) {

		Globals& g = Globals::getInstance();
		MyInstanceData* instanceData = nullptr;

		InstanceDataPtr* ownerPtr = nullptr;

		// 1) Try to find the instance using the stable addresses of the LabVIEW handles.
		auto tryResolve = [&](void* h) {
			if (!ownerPtr && h) {
				ownerPtr = g.getInstanceForArray(h);
			}
			};

		if (PvIndexArray && *PvIndexArray)          tryResolve(static_cast<void*>(*PvIndexArray));
		if (ResultArray && *ResultArray)            tryResolve(static_cast<void*>(*ResultArray));
		if (FirstStringValue && *FirstStringValue)  tryResolve(static_cast<void*>(*FirstStringValue));
		if (FirstDoubleValue && *FirstDoubleValue)  tryResolve(static_cast<void*>(*FirstDoubleValue));
		if (DoubleValueArray && *DoubleValueArray)  tryResolve(static_cast<void*>(*DoubleValueArray));

		// 2) Fallback: if no assignment exists, use the last known instance. This is less robust
		//    but provides a fallback for initialization scenarios.
		if (!ownerPtr && g.instances.size() == 1) {
			ownerPtr = g.instances[0];
		}
		if (!ownerPtr) {
			std::lock_guard<std::mutex> lock(g.instancesMutex);
			if (!g.instances.empty()) {
				ownerPtr = g.instances.back();
			}
		}

		if (ownerPtr == nullptr || *ownerPtr == nullptr) {
			CaLabDbgPrintf("Warning: bindArraysToInstance(%s): no valid instance found.", functionName);
			return;
		}

		instanceData = static_cast<MyInstanceDataPtr>(*ownerPtr);

		if (instanceData->firstFunctionName.empty()) {
			instanceData->firstFunctionName = functionName;
		}

		// 3) Update array pointers in instance memory and register the associated LV handles as keys.
		std::lock_guard<std::mutex> lock(instanceData->arrayMutex);
		if (PvIndexArray && *PvIndexArray && instanceData->PvIndexArray != *PvIndexArray) {
			// Debug: show previous and new PvIndexArray pointers
			CaLabDbgPrintf("bindArraysToInstance: instance=%p firstFunction='%s' ownerPtr=%p Prev.PvIndexArray=%p New.PvIndexArray=%p",
				static_cast<void*>(instanceData),
				instanceData->firstFunctionName.c_str(),
				static_cast<void*>(ownerPtr),
				static_cast<void*>(instanceData->PvIndexArray),
				static_cast<void*>(*PvIndexArray));
			instanceData->PvIndexArray = *PvIndexArray;
			g.registerArrayToInstance(static_cast<void*>(*PvIndexArray), ownerPtr);
		}
		if (ResultArray && *ResultArray && instanceData->ResultArray != *ResultArray) {
			instanceData->ResultArray = *ResultArray;
			g.registerArrayToInstance(static_cast<void*>(*ResultArray), ownerPtr);
		}
		if (FirstStringValue && *FirstStringValue && instanceData->FirstStringValue != *FirstStringValue) {
			instanceData->FirstStringValue = *FirstStringValue;
			g.registerArrayToInstance(static_cast<void*>(*FirstStringValue), ownerPtr);
		}
		if (FirstDoubleValue && *FirstDoubleValue && instanceData->FirstDoubleValue != *FirstDoubleValue) {
			instanceData->FirstDoubleValue = *FirstDoubleValue;
			g.registerArrayToInstance(static_cast<void*>(*FirstDoubleValue), ownerPtr);
		}
		if (DoubleValueArray && *DoubleValueArray && instanceData->DoubleValueArray != *DoubleValueArray) {
			instanceData->DoubleValueArray = *DoubleValueArray;
			g.registerArrayToInstance(static_cast<void*>(*DoubleValueArray), ownerPtr);
		}
	}
}

// =================================================================================
// Public API Implementation (Exported Functions)
// =================================================================================
extern "C" EXPORT void getValue(sStringArrayHdl* PvNameArray, sStringArrayHdl* FieldNameArray, sLongArrayHdl* PvIndexArray, double Timeout, sResultArrayHdl* ResultArray, sStringArrayHdl* FirstStringValue, sDoubleArrayHdl* FirstDoubleValue, sDoubleArray2DHdl* DoubleValueArray, LVBoolean* CommunicationStatus, LVBoolean* FirstCall, LVBoolean* NoMDEL, LVBoolean* IsInitialized, int filter)
{
	if (PvIndexArray == nullptr || PvNameArray == nullptr || *PvNameArray == nullptr || DSCheckHandle(*PvNameArray) != noErr || (**PvNameArray)->dimSize == 0) {
		*CommunicationStatus = 1;
		return;
	}
	Globals& g = Globals::getInstance();
	if (g.stopped.load()) {
		*CommunicationStatus = 1;
		return;
	}
	if (g.bCaLabPolling) {
		*NoMDEL = true;
	}
	CaContextGuard _caThreadAttach;
	*CommunicationStatus = 0;

	// === PHASE 1: Resolve or create instance binding ===
	MyInstanceData* boundInstance = nullptr;
	InstanceDataPtr* ownerPtr = nullptr;
	{
		std::lock_guard<std::mutex> instLock(g.instancesMutex);

		// Try to find existing instance from array handles
		auto tryResolve = [&](void* h) {
			if (!ownerPtr && h) {
				ownerPtr = g.getInstanceForArray(h);
			}
			};

		if (PvIndexArray && *PvIndexArray)          tryResolve(static_cast<void*>(*PvIndexArray));
		if (!ownerPtr && ResultArray && *ResultArray)          tryResolve(static_cast<void*>(*ResultArray));
		if (!ownerPtr && FirstStringValue && *FirstStringValue) tryResolve(static_cast<void*>(*FirstStringValue));
		if (!ownerPtr && FirstDoubleValue && *FirstDoubleValue) tryResolve(static_cast<void*>(*FirstDoubleValue));
		if (!ownerPtr && DoubleValueArray && *DoubleValueArray) tryResolve(static_cast<void*>(*DoubleValueArray));

		// Fallback: find a suitable instance
		if (!ownerPtr && !g.instances.empty()) {
			for (auto it = g.instances.rbegin(); it != g.instances.rend(); ++it) {
				if (*it && **it) {
					MyInstanceData* candidate = static_cast<MyInstanceData*>(**it);
					if (candidate && !candidate->isUnreserving.load(std::memory_order_acquire)
						&& candidate->firstFunctionName.empty()) {
						ownerPtr = *it;
						break;
					}
				}
			}
			if (!ownerPtr) {
				for (auto it = g.instances.rbegin(); it != g.instances.rend(); ++it) {
					if (*it && **it) {
						MyInstanceData* candidate = static_cast<MyInstanceData*>(**it);
						if (candidate && !candidate->isUnreserving.load(std::memory_order_acquire)
							&& candidate->firstFunctionName == "getValue") {
							ownerPtr = *it;
							break;
						}
					}
				}
			}
		}

		if (ownerPtr && *ownerPtr) {
			boundInstance = static_cast<MyInstanceData*>(*ownerPtr);
			if (boundInstance && boundInstance->firstFunctionName.empty()) {
				boundInstance->firstFunctionName = "getValue";
			}
		}
	}

	if (!boundInstance) {
		CaLabDbgPrintf("getValue: No instance available, proceeding without instance tracking");
	}

	if (boundInstance && boundInstance->isUnreserving.load(std::memory_order_acquire)) {
		*CommunicationStatus = 1;
		return;
	}

	// === PHASE 2: Activate call guard ===
	struct _CallGuard {
		MyInstanceData* d;
		bool active = false;
		_CallGuard(MyInstanceData* d) : d(d) {
			if (d && !d->isUnreserving.load(std::memory_order_acquire)) {
				d->activeCalls.fetch_add(1, std::memory_order_acq_rel);
				active = true;
			}
		}
		~_CallGuard() {
			if (d && active) {
				d->activeCalls.fetch_sub(1, std::memory_order_acq_rel);
				Globals::getInstance().notify();
			}
		}
		bool isActive() const { return active; }
	} callGuard(boundInstance);

	if (boundInstance && !callGuard.isActive()) {
		*CommunicationStatus = 1;
		return;
	}

	// === PHASE 3: Main processing ===
	if (filter == 0) {
		filter = out_filter::defaultFilter;
	}
	uInt32 nameCount = static_cast<uInt32>((**PvNameArray)->dimSize);
	uInt32 maxNumberOfValues = 0;
	const std::chrono::steady_clock::time_point hardDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<long long>((Timeout > 0.0 ? Timeout : 0.0) * 1000.0));

	// Determine if this is the first call or if the input arrays have changed
	bool needsReinit = *NoMDEL || *FirstCall || !*IsInitialized || !*PvIndexArray || !**PvIndexArray || (**PvIndexArray)->dimSize != nameCount;
	if (needsReinit) {
		std::unordered_set<std::string> uninitializedPvNames;
		if (!setupPvIndexAndRegistry(PvNameArray, FieldNameArray, PvIndexArray, CommunicationStatus, &uninitializedPvNames)) {
			CaLabDbgPrintf("Error initializing getValue.");
			bindArraysToInstance("getValue", PvIndexArray, ResultArray, FirstStringValue, FirstDoubleValue, DoubleValueArray);
			return;
		}

		// === CRITICAL: Bind PvIndexArray IMMEDIATELY after it's created ===
		// Only log if actually binding for the first time
		if (boundInstance && PvIndexArray && *PvIndexArray) {
			std::lock_guard<std::mutex> instLock(g.instancesMutex);
			InstanceDataPtr* myOwnerPtr = nullptr;
			for (auto* ptr : g.instances) {
				if (ptr && *ptr == boundInstance) {
					myOwnerPtr = ptr;
					break;
				}
			}
			if (myOwnerPtr) {
				std::lock_guard<std::mutex> arrLock(boundInstance->arrayMutex);
				if (boundInstance->PvIndexArray != *PvIndexArray) {
					boundInstance->PvIndexArray = *PvIndexArray;
					g.registerArrayToInstance(static_cast<void*>(*PvIndexArray), myOwnerPtr);
				}
			}
		}

		if (*FieldNameArray && **FieldNameArray && (**FieldNameArray)->dimSize > 0 && (filter & out_filter::pviFieldValues || (filter & out_filter::pviFieldNames))) {
			for (uInt32 i = 0; i < nameCount; ++i) {
				PvEntryHandle entry{ PvIndexArray, i };
				if (!entry || !entry->pvItem) continue;
				std::lock_guard<std::mutex> lock(entry->pvItem->ioMutex());
				if (!entry->pvItem->getFields().empty()) {
					uninitializedPvNames.insert(entry->pvItem->getName());
				}
			}
		}
		else {
			for (uInt32 i = 0; i < nameCount; ++i) {
				PvEntryHandle entry{ PvIndexArray, i };
				if (entry && entry->pvItem) {
					std::lock_guard<std::mutex> lock(entry->pvItem->ioMutex());
					if (entry->pvItem->channelId == nullptr || entry->pvItem->getRecordType().empty()) {
						uninitializedPvNames.insert(entry->pvItem->getName());
					}
				}
			}
		}
		subscribeBasePVs(uninitializedPvNames, Timeout, hardDeadline, NoMDEL);
		connectPVs(uninitializedPvNames, Timeout, hardDeadline);
		if (filter & out_filter::pviFieldValues) {
			waitForUninitializedPvs(uninitializedPvNames, Timeout, hardDeadline);
		}
		*FirstCall = false;
		*IsInitialized = true;
	}

	// Bind arrays to ensure instance tracking (also updates if arrays changed)
	bindArraysToInstance("getValue", PvIndexArray, ResultArray, FirstStringValue, FirstDoubleValue, DoubleValueArray);

	// Check for changes
	std::vector<uInt32> changedIndexList = getChangedPvIndices(PvIndexArray, Timeout, hardDeadline, NoMDEL);
	bool hasChanges = !changedIndexList.empty();
	if (!hasChanges && FieldNameArray && *FieldNameArray && **FieldNameArray && (**FieldNameArray)->dimSize > 0) {
		hasChanges = true;
	}
	if (!hasChanges && !needsReinit) {
		if (CommunicationStatus) {
			const int globalErrors = g.pvErrorCount.load(std::memory_order_relaxed);
			if (globalErrors == 0) {
				*CommunicationStatus = 0;
			}
			else {
				bool anyPvError = false;
				for (uInt32 i = 0; i < nameCount; ++i) {
					PvEntryHandle entry{ PvIndexArray, i };
					if (!entry || !entry->pvItem) continue;
					if (entry->pvItem->getErrorCode() > (int)ECA_NORMAL) { anyPvError = true; break; }
				}
				*CommunicationStatus = anyPvError ? 1 : 0;
			}
		}
		return;
	}

	// Acquire processing lock
	TimeoutUniqueLock<std::shared_timed_mutex> getLockGuard(g.getLock, "getValue");
	if (!getLockGuard.isLocked()) {
		CaLabDbgPrintf("Warning: Could not acquire an exclusive lock for getValue");
		*CommunicationStatus = 1;
		return;
	}
	bool didPrepare = false;
	{
		TimeoutSharedLock<std::shared_timed_mutex> rlock(g.pvRegistryLock, "getValue-check", std::chrono::milliseconds(30000));
		if (!rlock.isLocked()) {
			CaLabDbgPrintf("Error: Failed to acquire shared lock for getValue (check).");
			return;
		}

		// Determine maxNumberOfValues
		if (DoubleValueArray && *DoubleValueArray && **DoubleValueArray) {
			maxNumberOfValues = (**DoubleValueArray)->dimSizes[1];
		}
		if (!maxNumberOfValues &&
			ResultArray && *ResultArray && **ResultArray &&
			(**ResultArray)->dimSize > 0) {
			auto valueNumHandle = (**ResultArray)->result[0].ValueNumberArray;
			if (valueNumHandle && *valueNumHandle) {
				maxNumberOfValues = static_cast<uInt32>((*valueNumHandle)->dimSize);
			}
		}
		if (!maxNumberOfValues) {
			for (uInt32 i = 0; i < nameCount; ++i) {
				PvEntryHandle entry{ PvIndexArray, i };
				if (!entry || !entry->pvItem) continue;
				std::lock_guard<std::mutex> lock(entry->pvItem->ioMutex());
				maxNumberOfValues = std::max(maxNumberOfValues, entry->pvItem->getNumberOfValues());
			}
		}

			if (hasChanges || needsReinit) {
				cleanupMemory(ResultArray, FirstStringValue, FirstDoubleValue, DoubleValueArray, PvNameArray,
					needsReinit ? std::vector<uInt32>() : changedIndexList);

				MgErr err = prepareOutputArrays(nameCount, maxNumberOfValues, filter,
					ResultArray, FirstStringValue, FirstDoubleValue, DoubleValueArray, PvNameArray,
					needsReinit ? std::vector<uInt32>() : changedIndexList);
				if (err != noErr) {
					CaLabDbgPrintf("Error preparing output arrays: %d", err);
					*CommunicationStatus = 1;
					return;
				}
				didPrepare = true;

				populateOutputArrays(nameCount, maxNumberOfValues, filter, PvIndexArray,
					ResultArray, FirstStringValue, FirstDoubleValue, DoubleValueArray,
					needsReinit ? std::vector<uInt32>() : changedIndexList);

			if (CommunicationStatus) {
				const int globalErrors = g.pvErrorCount.load(std::memory_order_relaxed);
				if (globalErrors == 0) {
					*CommunicationStatus = 0;
				}
				else {
					bool anyPvError = false;
					for (uInt32 i = 0; i < nameCount; ++i) {
						PvEntryHandle entry{ PvIndexArray, i };
						if (!entry || !entry->pvItem) continue;
						if (entry->pvItem->getErrorCode() > (int)ECA_NORMAL) { anyPvError = true; break; }
					}
					*CommunicationStatus = anyPvError ? 1 : 0;
				}
			}
		}
	}

	if (didPrepare) {
		// Arrays may have moved due to reallocation; refresh instance bindings.
		bindArraysToInstance("getValue", PvIndexArray, ResultArray, FirstStringValue, FirstDoubleValue, DoubleValueArray);
	}

	updatePvIndexArray(PvIndexArray, changedIndexList);

	if (*NoMDEL) {
		for (uInt32 i = 0; i < nameCount; ++i) {
			PvEntryHandle entry{ PvIndexArray, i };
			if (entry && entry->pvItem && entry->pvItem->eventId) {
				std::lock_guard<std::mutex> lock(entry->pvItem->ioMutex());
				ca_clear_subscription(entry->pvItem->eventId);
				entry->pvItem->eventId = nullptr;
			}
		}
	}
}

extern "C" EXPORT void putValue(sStringArrayHdl* PvNameArray, sLongArrayHdl* PvIndexArray, sStringArray2DHdl* StringValueArray2D, sDoubleArray2DHdl* DoubleValueArray2D, sLongArray2DHdl* LongValueArray2D, uInt32 DataType, double Timeout, LVBoolean* wait4readback, sErrorArrayHdl* ErrorArray, LVBoolean* CommunicationStatus, LVBoolean* FirstCall) {
	if (PvIndexArray == nullptr || PvNameArray == nullptr || *PvNameArray == nullptr || DSCheckHandle(*PvNameArray) != noErr || (**PvNameArray)->dimSize == 0) {
		if (CommunicationStatus) *CommunicationStatus = 1;
		return;
	}

	Globals& g = Globals::getInstance();
	if (g.stopped.load()) { if (CommunicationStatus) *CommunicationStatus = 1; return; }

	// Ensure this thread is attached to the CA context while using CA APIs.
	CaContextGuard _caThreadAttach;

	// Validate PV names.
	if (!PvNameArray || !*PvNameArray || DSCheckHandle(*PvNameArray) != noErr || (**PvNameArray)->dimSize == 0) {
		if (CommunicationStatus) *CommunicationStatus = 1;
		return;
	}
	const uInt32 nameCount = static_cast<uInt32>((**PvNameArray)->dimSize);

	// Determine which value array to use based on DataType.
	const bool isString = (DataType == DT_STRING);
	const bool isDouble = (DataType == DT_SINGLE || DataType == DT_DOUBLE);
	const bool isChar = (DataType == DT_CHAR);
	const bool isShort = (DataType == DT_SHORT);
	const bool isLong32 = (DataType == DT_LONG);
	const bool isQuad64 = (DataType == DT_QUAD);

	// === Resolve instance binding early (similar to getValue) ===
	MyInstanceData* instanceData = nullptr;
	InstanceDataPtr* ownerPtr = nullptr;
	{
		std::lock_guard<std::mutex> instLock(g.instancesMutex);

		// Try to find existing instance from array handles
		if (PvIndexArray && *PvIndexArray) {
			ownerPtr = g.getInstanceForArray(static_cast<void*>(*PvIndexArray));
		}

		// Fallback: find a suitable instance
		if (!ownerPtr && !g.instances.empty()) {
			// Priority 1: Find an unused instance (no function name set yet)
			for (auto it = g.instances.rbegin(); it != g.instances.rend(); ++it) {
				if (*it && **it) {
					MyInstanceData* candidate = static_cast<MyInstanceData*>(**it);
					if (candidate && !candidate->isUnreserving.load(std::memory_order_acquire)
						&& candidate->firstFunctionName.empty()) {
						ownerPtr = *it;
						break;
					}
				}
			}
			// Priority 2: Find an instance already assigned to putValue
			if (!ownerPtr) {
				for (auto it = g.instances.rbegin(); it != g.instances.rend(); ++it) {
					if (*it && **it) {
						MyInstanceData* candidate = static_cast<MyInstanceData*>(**it);
						if (candidate && !candidate->isUnreserving.load(std::memory_order_acquire)
							&& candidate->firstFunctionName == "putValue") {
							ownerPtr = *it;
							break;
						}
					}
				}
			}
		}

		if (ownerPtr && *ownerPtr) {
			instanceData = static_cast<MyInstanceData*>(*ownerPtr);
			// Set function name immediately if not already set
			if (instanceData && instanceData->firstFunctionName.empty()) {
				instanceData->firstFunctionName = "putValue";
			}

			// === CRITICAL: Bind PvIndexArray to this instance immediately ===
			if (PvIndexArray && *PvIndexArray) {
				std::lock_guard<std::mutex> arrLock(instanceData->arrayMutex);
				if (instanceData->PvIndexArray != *PvIndexArray) {
					instanceData->PvIndexArray = *PvIndexArray;
					g.registerArrayToInstance(static_cast<void*>(*PvIndexArray), ownerPtr);
				}
			}
		}
	}

	// If no instance found, log warning but continue
	if (!instanceData) {
		CaLabDbgPrintf("putValue: No instance available, proceeding without instance tracking");
	}

	if (instanceData && instanceData->isUnreserving.load(std::memory_order_acquire)) {
		if (CommunicationStatus) *CommunicationStatus = 1;
		return;
	}

	// === Activate call guard ===
	struct _CallGuard {
		MyInstanceData* d;
		bool active = false;
		_CallGuard(MyInstanceData* d) : d(d) {
			if (d && !d->isUnreserving.load(std::memory_order_acquire)) {
				d->activeCalls.fetch_add(1, std::memory_order_acq_rel);
				active = true;
			}
		}
		~_CallGuard() {
			if (d && active) {
				d->activeCalls.fetch_sub(1, std::memory_order_acq_rel);
				Globals::getInstance().notify();
			}
		}
		bool isActive() const { return active; }
	} __cg(instanceData);

	// If we couldn't activate the guard (instance is unreserving), abort
	if (instanceData && !__cg.isActive()) {
		if (CommunicationStatus) *CommunicationStatus = 1;
		return;
	}

	// Local context to encapsulate helpers and shared state for this put operation.
	struct PutValueCtx {
		Globals& globals;
		sStringArrayHdl* PvNameArray;
		sLongArrayHdl* PvIndexArray;
		sStringArray2DHdl* StringValueArray2D;
		sDoubleArray2DHdl* DoubleValueArray2D;
		sLongArray2DHdl* LongValueArray2D;
		sErrorArrayHdl* ErrorArray;
		bool isString;
		bool isDouble;
		bool isChar;
		bool isShort;
		bool isLong32;
		bool isQuad64;

		static std::string getLVString(LStrHandle h) {
			if (!h) return std::string();
			const size_t len = static_cast<size_t>((*h)->cnt);
			const char* s = reinterpret_cast<const char*>((*h)->str);
			return std::string(s, len);
		}

		static bool parseNumericString(const std::string& txt, double& out) {
			auto parseClassic = [](const std::string& s, double& value) -> bool {
				const char* start = s.c_str();
				char* end = nullptr;
#if defined _WIN32 || defined _WIN64
				static _locale_t cLocale = _create_locale(LC_NUMERIC, "C");
				value = _strtod_l(start, &end, cLocale);
#else
				static locale_t cLocale = newlocale(LC_NUMERIC_MASK, "C", nullptr);
				if (cLocale) {
					value = strtod_l(start, &end, cLocale);
				}
				else {
					value = std::strtod(start, &end);
				}
#endif
				if (start == end) return false;
				while (*end && std::isspace(static_cast<unsigned char>(*end))) { ++end; }
				return (*end == '\0');
			};

			if (parseClassic(txt, out)) return true;

			size_t lastComma = std::string::npos;
			size_t lastDot = std::string::npos;
			for (size_t i = 0; i < txt.size(); ++i) {
				if (txt[i] == ',') lastComma = i;
				else if (txt[i] == '.') lastDot = i;
			}
			const bool hasComma = (lastComma != std::string::npos);
			const bool hasDot = (lastDot != std::string::npos);

			if (!hasComma && !hasDot) {
				return false;
			}

			// Normalize decimal separators to allow both "1.23" and "1,23".
			if (!hasComma && hasDot) return false;

			char decimal = ',';
			char thousands = '\0';
			if (hasComma && hasDot) {
				if (lastComma > lastDot) { decimal = ','; thousands = '.'; }
				else { decimal = '.'; thousands = ','; }
			}

			std::string normalized;
			normalized.reserve(txt.size());
			for (char c : txt) {
				if (thousands && c == thousands) continue;
				if (c == decimal) normalized.push_back('.');
				else normalized.push_back(c);
			}

			if (normalized == txt) {
				return false;
			}

			return parseClassic(normalized, out);
		}

		void ensureErrorArray(uInt32 n) {
			if (!ErrorArray || n < 1) return;
			if (!*ErrorArray || (**ErrorArray)->dimSize != n) {
				if (*ErrorArray && DSCheckHandle(*ErrorArray) == noErr) 
					DSDisposeHandle(*ErrorArray);
				*ErrorArray = nullptr;
				const size_t sz =
					sizeof(sErrorArray) +            // dimSize + 1 element
					(static_cast<size_t>(n) - 1) * sizeof(sError);   // remaining n-1 elements
				*ErrorArray = reinterpret_cast<sErrorArrayHdl>(DSNewHClr(static_cast<uInt32>(sz)));
				if (*ErrorArray) { (**ErrorArray)->dimSize = n; }
			}
			if (*ErrorArray) {
				for (uInt32 i = 0; i < n; ++i) {
					(**ErrorArray)->result[i].status = 0;
					(**ErrorArray)->result[i].code = 0;
				}
			}
		}

		void setErrorAt(uInt32 idx, uInt32 err, const std::string& msg) {
			if (!ErrorArray || !*ErrorArray) {
				return;
			}
			sError& e = (**ErrorArray)->result[idx];
			const uInt32 lvCode = (err <= (int)ECA_NORMAL) ? 0u : static_cast<uInt32>(err) + ERROR_OFFSET;

			if (lvCode != 0u && e.code == lvCode && e.source && *e.source && (*e.source)->cnt) {
				return;
			}
			e.status = 0;
			e.code = lvCode;
			setLVString(e.source, msg);
		}

		void getDimensions(uInt32& rows, uInt32& cols) const {
			rows = cols = 0;
			if (isDouble && DoubleValueArray2D && *DoubleValueArray2D) { rows = (**DoubleValueArray2D)->dimSizes[0]; cols = (**DoubleValueArray2D)->dimSizes[1]; }
			else if ((isChar || isShort || isLong32 || isQuad64) && LongValueArray2D && *LongValueArray2D) { rows = (**LongValueArray2D)->dimSizes[0]; cols = (**LongValueArray2D)->dimSizes[1]; }
			else if (isString && StringValueArray2D && *StringValueArray2D) { rows = (**StringValueArray2D)->dimSizes[0]; cols = (**StringValueArray2D)->dimSizes[1]; }
		}

		PVItem* getPvItemForIndex(uInt32 i) {
			if (!PvNameArray || !*PvNameArray || i >= (**PvNameArray)->dimSize) {
				return nullptr;
			}

			LStrHandle h = (**PvNameArray)->elt[i];
			if (!h || !*h) {
				return nullptr;
			}

			std::string pvName(reinterpret_cast<const char*>((*h)->str), (*h)->cnt);
			if (pvName.empty()) {
				return nullptr;
			}

			// ---- atomischer Leseversuch mit schneller Validierung ----
			if (PvIndexArray && *PvIndexArray && **PvIndexArray && (**PvIndexArray)->dimSize > i) {
				PvEntryHandle entry{ PvIndexArray, i};
				if (entry) {
					PVItem* result = entry->pvItem;
					if (result) {
						// Note: caller must still handle nullptr result.
						return result;
					}
				}
				// If we couldn't obtain PV from index array, fall through to registry lookup below.
			}

			// Lookup in registry (shared lock)
			{
				TimeoutSharedLock<std::shared_timed_mutex> sharedLock(
					globals.pvRegistryLock, "putValue-lookup", std::chrono::milliseconds(50));
				if (sharedLock.isLocked()) {
					auto it = globals.pvRegistry.find(pvName);
					if (it != globals.pvRegistry.end()) {
						PVItem* item = it->second.get();
						if (!item->isConnected()) {
							connectPv(item, false);
						}
						return item;
					}
				}
				else {
					CaLabDbgPrintf("putValue: Shared lock timeout for %s", pvName.c_str());
					return nullptr;
				}
			}

			// If missing, create it (with registry unique lock)
			for (int retry = 0; retry < 2; ++retry) {
				TimeoutUniqueLock<std::shared_timed_mutex> uniqueLock(
					globals.pvRegistryLock,
					"putValue-add-missing",
					std::chrono::milliseconds(100 + retry * 100)
				);
				if (!uniqueLock.isLocked()) {
					if (retry == 1) {
						CaLabDbgPrintf("putValue: Unique lock timeout after retries for %s", pvName.c_str());
						return nullptr;
					}
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}

				auto it = globals.pvRegistry.find(pvName);
				if (it != globals.pvRegistry.end()) {
					return it->second.get();
				}

				auto newPv = std::make_unique<PVItem>(pvName);
				PVItem* pvItem = newPv.get();
				globals.pvRegistry[pvName] = std::move(newPv);

				// Atomically claim and delete old element if present
				if (PvIndexArray && *PvIndexArray && **PvIndexArray && (**PvIndexArray)->dimSize > i) {
					tryClaimAndDeletePvIndexEntry(&(**PvIndexArray)->elt[i]);
				}

				PVMetaInfo* metaInfo = new PVMetaInfo(pvItem);
				metaInfo->functionName = "putValue";
				PvIndexEntry* entry = new PvIndexEntry(pvItem, metaInfo);

				auto atomicElt = reinterpret_cast<atomic_ptr_t*>(&(**PvIndexArray)->elt[i]);
				atomicElt->store(reinterpret_cast<uintptr_t>(entry), std::memory_order_release);

				connectPv(pvItem, false);
				return pvItem;
			}

			return nullptr;
		}

		struct PutCtx {
			std::atomic<uInt32> completed{ 0 };
			std::atomic<uInt32> refs{ 1 };
		};

		int doPut(chtype type, unsigned long count, chid ch, const void* data,
			bool doWaitRequested, PutCtx* putCtx, uInt32& totalWrites) {
			if (ca_state(ch) != cs_conn) {
				return ECA_DISCONN;
			}
			if (doWaitRequested) {
				putCtx->refs.fetch_add(1, std::memory_order_relaxed);
				auto putCallback = [](struct event_handler_args args) {
					PutCtx* ctx = static_cast<PutCtx*>(args.usr);
					if (!ctx) {
						return;
					}
					ctx->completed.fetch_add(1, std::memory_order_relaxed);
					Globals::getInstance().notify();
					if (ctx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
						delete ctx;
						ctx = nullptr;
					}
					};
				int rc = ca_array_put_callback(type, count, ch, data, putCallback, putCtx);
				if (rc == ECA_NORMAL) {
					totalWrites++;
				}
				else {
					putCtx->refs.fetch_sub(1, std::memory_order_acq_rel);
				}
				return rc;
			}
			else {
				return ca_array_put(type, count, ch, data);
			}
		}
	};

	PutValueCtx ctx{ g, PvNameArray, PvIndexArray, StringValueArray2D, DoubleValueArray2D, LongValueArray2D, ErrorArray,
					 isString, isDouble, isChar, isShort, isLong32, isQuad64 };
	ctx.ensureErrorArray(nameCount);

	const bool doWaitRequested = (wait4readback && *wait4readback);

	// Ensure PvIndexArray can hold all PVs.
	if (PvIndexArray) {
		bool needsResize = !*PvIndexArray || (**PvIndexArray) == nullptr || (**PvIndexArray)->dimSize != nameCount;
		if (needsResize) {
			if (NumericArrayResize(iQ, 1, (UHandle*)PvIndexArray, nameCount) != noErr || !*PvIndexArray || !**PvIndexArray) {
				const size_t headerSize = sizeof(sLongArray);  // dimSize + 1 element
				const size_t elemSize = sizeof(uint64_t);   // sizeof(uint64_t)
				const size_t totalSize = headerSize + (static_cast<size_t>(nameCount) - 1) * elemSize;
				*PvIndexArray = reinterpret_cast<sLongArrayHdl>(DSNewHClr(static_cast<uInt32>(totalSize)));
				if (*PvIndexArray && **PvIndexArray) { (**PvIndexArray)->dimSize = nameCount; }
			}
			else {
				(**PvIndexArray)->dimSize = nameCount;
			}

			// Sanity check element size
			if (*PvIndexArray && **PvIndexArray && sizeof((**PvIndexArray)->elt[0]) < sizeof(uintptr_t)) {
				CaLabDbgPrintf("Error: PvIndexArray element size mismatch in putValue (%zu != %zu).",
					sizeof((**PvIndexArray)->elt[0]), sizeof(uintptr_t));
				if (instanceData && instanceData->PvIndexArray == *PvIndexArray) {
					// undo binding to prevent later UB
					instanceData->PvIndexArray = nullptr;
				}
			}
			if (*PvIndexArray && **PvIndexArray && sizeof((**PvIndexArray)->elt[0]) >= sizeof(uintptr_t)) {
				sanitizePvIndexArray(**PvIndexArray, static_cast<size_t>(nameCount));
			}

			// === Only bind when array was actually created/resized ===
			if (instanceData && *PvIndexArray) {
				std::lock_guard<std::mutex> instLock(g.instancesMutex);
				InstanceDataPtr* myOwnerPtr = nullptr;
				for (auto* ptr : g.instances) {
					if (ptr && *ptr == instanceData) {
						myOwnerPtr = ptr;
						break;
					}
				}
				if (myOwnerPtr) {
					std::lock_guard<std::mutex> arrLock(instanceData->arrayMutex);
					if (instanceData->PvIndexArray != *PvIndexArray) {
						instanceData->PvIndexArray = *PvIndexArray;
						g.registerArrayToInstance(static_cast<void*>(*PvIndexArray), myOwnerPtr);
					}
				}
			}
		}
	}

	if (!isString && !isDouble && !isChar && !isShort && !isLong32 && !isQuad64) {
		if (CommunicationStatus) *CommunicationStatus = 1;
		if (ErrorArray && *ErrorArray) {
			for (uInt32 i = 0; i < nameCount; ++i) ctx.setErrorAt(i, (uInt32)ECA_BADTYPE, "Unsupported DataType");
		}
		return;
	}

	uInt32 rows = 0, cols = 0;
	ctx.getDimensions(rows, cols);
	if (rows && rows != nameCount && rows != 1) {
		if (CommunicationStatus) *CommunicationStatus = 1;
		for (uInt32 i = 0; i < nameCount; ++i) ctx.setErrorAt(i, (uInt32)ECA_BADCOUNT, "Value array rows must match PV count or be 1");
		return;
	}

	// Stage A: Ensure all PVs exist in the index and initiate connection if needed.
	std::vector<PVItem*> items(nameCount, nullptr);
	if (FirstCall && *FirstCall) {
		std::vector<uInt32> toConnect;
		toConnect.reserve(nameCount);
		for (uInt32 i = 0; i < nameCount; ++i) {
			PVItem* item = ctx.getPvItemForIndex(i);
			items[i] = item;
			if (!item) continue;
			const bool needsConnect = (item->channelId == nullptr || !item->isConnected());
			if (needsConnect) {
				toConnect.push_back(i);
			}
		}
		ca_flush_io();

		if (!toConnect.empty()) {
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(Timeout * 1000.0));
			while (true) {
				bool allConnected = true;
				for (uInt32 idx : toConnect) {
					PVItem* p = items[idx];
					if (!p || !p->channelId || !p->isConnected()) { allConnected = false; break; }
				}
				if (allConnected) break;

				const auto now = std::chrono::steady_clock::now();
				if (now >= deadline) {
					uInt32 notConnected = 0;
					for (uInt32 idx : toConnect) {
						PVItem* p = items[idx];
						if (!p || !p->channelId || !p->isConnected()) { ++notConnected; }
					}
					if (CommunicationStatus) *CommunicationStatus = 1;
					break;
				}

				const double remainingSec = std::chrono::duration<double>(deadline - now).count();
				const double slice = remainingSec > 0.05 ? 0.05 : (remainingSec > 0.0 ? remainingSec : 0.0);
				ca_pend_event(static_cast<double>(slice));
				Globals::getInstance().waitForNotification(std::chrono::milliseconds(10));
			}
		}
	}
	else {
		for (uInt32 i = 0; i < nameCount; ++i) {
			PVItem* item = ctx.getPvItemForIndex(i);
			items[i] = item;
		}
	}

	// Stage B: If `doWaitRequested`, subscribe and wait for initial values.
	if (doWaitRequested) {
		std::vector<uInt32> toSubscribe;
		toSubscribe.reserve(nameCount);
		for (uInt32 i = 0; i < nameCount; ++i) {
			PVItem* p = items[i];
			if (!p) continue;
			bool needsSubscription = false;
			{
				std::lock_guard<std::mutex> lk(p->ioMutex());
				needsSubscription = (p->channelId != nullptr && p->isConnected() && p->eventId == nullptr);
			}
			if (needsSubscription) { subscribePv(p); toSubscribe.push_back(i); }
		}
		ca_flush_io();

		if (!toSubscribe.empty()) {
			const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(Timeout * 1000.0));
			while (std::chrono::steady_clock::now() < deadline) {
				bool allHaveValue = true;
				for (uInt32 idx : toSubscribe) {
					PVItem* p = items[idx];
					if (!p || !p->hasValue()) { allHaveValue = false; break; }
				}
				if (allHaveValue) break;
				ca_poll();
				Globals::getInstance().waitForNotification(std::chrono::milliseconds(20));
			}
		}
	}

	// Stage C: Perform the writes.
	std::vector<int> caStatus(nameCount, ECA_NORMAL);
	uInt32 totalWrites = 0;
	bool putCallbacksTimedOut = false;

	PutValueCtx::PutCtx* putCtx = doWaitRequested ? new PutValueCtx::PutCtx() : nullptr;

	for (uInt32 i = 0; i < nameCount; ++i) {
		PVItem* item = items[i];
		if (!item || !item->isConnected() || !item->channelId) {
			caStatus[i] = ECA_DISCONNCHID;
			ctx.setErrorAt(i, (uInt32)ECA_DISCONNCHID, "PV not connected");
			continue;
		}
		const chid channelID = item->channelId;
		const unsigned elementCapacity = ca_element_count(channelID);
		const short nativeType = item->getDbrType();

		uInt32 availableCount = 1;
		size_t startIndex = 0;
		if (rows == nameCount) {
			availableCount = (cols ? cols : 1u);
			startIndex = static_cast<size_t>(i) * (cols ? cols : 1u);
		}
		else {
			if (nameCount == 1) { availableCount = (cols ? cols : 1u); startIndex = 0; }
			else {
				if (cols == 1) { availableCount = 1; startIndex = 0; }
				else if (cols == nameCount) { availableCount = 1; startIndex = i; }
				else { availableCount = (cols ? cols : 1u); startIndex = 0; }
			}
		}

		if (isDouble) {
			if (!DoubleValueArray2D || !*DoubleValueArray2D) {
				ctx.setErrorAt(i, (uInt32)ECA_BADCOUNT, "Missing DoubleValueArray2D"); caStatus[i] = ECA_BADCOUNT;
				if (CommunicationStatus) *CommunicationStatus = 1;
				continue;
			}
			const uInt32 nToWrite = std::min<unsigned>(availableCount, elementCapacity ? elementCapacity : availableCount);
			const double* basePtr = &((**DoubleValueArray2D)->elt[startIndex]);
			if (nativeType == DBF_STRING) {
				std::vector<dbr_string_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) std::snprintf(temp[j], sizeof(dbr_string_t), "%.15g", basePtr[j]);
				caStatus[i] = ctx.doPut(DBR_STRING, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_SHORT) {
				std::vector<dbr_short_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_short_t>(std::max<long long>(std::numeric_limits<dbr_short_t>::min(), std::min<long long>(std::numeric_limits<dbr_short_t>::max(), static_cast<long long>(basePtr[j]))));
				caStatus[i] = ctx.doPut(DBR_SHORT, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_FLOAT) {
				std::vector<dbr_float_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_float_t>(basePtr[j]);
				caStatus[i] = ctx.doPut(DBR_FLOAT, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_ENUM) {
				std::vector<dbr_enum_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_enum_t>(std::max<long long>(0, std::min<long long>(std::numeric_limits<dbr_enum_t>::max(), static_cast<long long>(basePtr[j]))));
				caStatus[i] = ctx.doPut(DBR_ENUM, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_CHAR) {
				std::vector<dbr_char_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_char_t>(std::max<long long>(0, std::min<long long>(255, static_cast<long long>(basePtr[j]))));
				caStatus[i] = ctx.doPut(DBR_CHAR, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_LONG) {
				std::vector<dbr_long_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_long_t>(std::max<long long>(std::numeric_limits<dbr_long_t>::min(), std::min<long long>(std::numeric_limits<dbr_long_t>::max(), static_cast<long long>(basePtr[j]))));
				caStatus[i] = ctx.doPut(DBR_LONG, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else {
				caStatus[i] = ctx.doPut(DBR_DOUBLE, nToWrite, channelID, basePtr, doWaitRequested, putCtx, totalWrites);
			}

			auto updateError = [&]() {
				const bool handleOk = (ErrorArray && *ErrorArray && (**ErrorArray) && i < (**ErrorArray)->dimSize);
				bool needsUpdate = true;
				if (handleOk) {
					sError& e = (**ErrorArray)->result[i];
					const uInt32 lvCode = (caStatus[i] <= (int)ECA_NORMAL) ? 0u : static_cast<uInt32>(caStatus[i]) + ERROR_OFFSET;
					if (e.code == lvCode && e.source && *e.source && (*e.source)->cnt > 0) {
						needsUpdate = false;
					}
				}
				if (caStatus[i] == ECA_NORMAL) {
					ctx.setErrorAt(i, (uInt32)ECA_NORMAL, std::string(ca_message_safe(ECA_NORMAL)));
				}
				else if (needsUpdate) {
					ctx.setErrorAt(i, (uInt32)caStatus[i], "ca_array_put failed. " + std::string(ca_message_safe(caStatus[i])));
				}
				};
			if (instanceData) { std::lock_guard<std::mutex> lock(instanceData->arrayMutex); updateError(); }
			else { updateError(); }

			if (CommunicationStatus) {
				*CommunicationStatus = (caStatus[i] != ECA_NORMAL);
			}
		}
		else if (isChar || isShort || isLong32 || isQuad64) {
			if (!LongValueArray2D || !*LongValueArray2D) {
				ctx.setErrorAt(i, (uInt32)ECA_BADCOUNT, "Missing LongValueArray2D"); caStatus[i] = ECA_BADCOUNT;
				if (CommunicationStatus) *CommunicationStatus = 1;
				continue;
			}
			const uInt32 nToWrite = std::min<unsigned>(availableCount, elementCapacity ? elementCapacity : availableCount);
			const uint64_t* basePtr = &((**LongValueArray2D)->elt[startIndex]);

			auto decodeSigned = [&](uint64_t raw) -> int64_t {
				if (isChar)   return static_cast<int64_t>(static_cast<int8_t>(raw & 0xFFu));
				if (isShort)  return static_cast<int64_t>(static_cast<int16_t>(raw & 0xFFFFu));
				if (isLong32) return static_cast<int64_t>(static_cast<int32_t>(raw & 0xFFFFFFFFu));
				return static_cast<int64_t>(raw);
				};

			if (nativeType == DBF_STRING) {
				std::vector<dbr_string_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) {
					std::string s = std::to_string(decodeSigned(basePtr[j]));
					strncpy(temp[j], s.c_str(), sizeof(dbr_string_t) - 1);
					temp[j][sizeof(dbr_string_t) - 1] = '\0';
				}
				caStatus[i] = ctx.doPut(DBR_STRING, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_SHORT) {
				std::vector<dbr_short_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_short_t>(std::max<int64_t>(std::numeric_limits<dbr_short_t>::min(), std::min<int64_t>(std::numeric_limits<dbr_short_t>::max(), decodeSigned(basePtr[j]))));
				caStatus[i] = ctx.doPut(DBR_SHORT, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_FLOAT) {
				std::vector<dbr_float_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_float_t>(decodeSigned(basePtr[j]));
				caStatus[i] = ctx.doPut(DBR_FLOAT, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_ENUM) {
				std::vector<dbr_enum_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_enum_t>(std::max<int64_t>(0, std::min<int64_t>(std::numeric_limits<dbr_enum_t>::max(), decodeSigned(basePtr[j]))));
				caStatus[i] = ctx.doPut(DBR_ENUM, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_CHAR) {
				std::vector<dbr_char_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_char_t>(static_cast<uint8_t>(decodeSigned(basePtr[j])));
				caStatus[i] = ctx.doPut(DBR_CHAR, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_DOUBLE) {
				std::vector<dbr_double_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_double_t>(decodeSigned(basePtr[j]));
				caStatus[i] = ctx.doPut(DBR_DOUBLE, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else {
				std::vector<dbr_long_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_long_t>(std::max<int64_t>(std::numeric_limits<dbr_long_t>::min(), std::min<int64_t>(std::numeric_limits<dbr_long_t>::max(), decodeSigned(basePtr[j]))));
				caStatus[i] = ctx.doPut(DBR_LONG, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}

			auto updateError = [&]() {
				const bool handleOk = (ErrorArray && *ErrorArray && (**ErrorArray) && i < (**ErrorArray)->dimSize);
				bool needsUpdate = true;
				if (handleOk) {
					sError& e = (**ErrorArray)->result[i];
					const uInt32 lvCode = (caStatus[i] <= (int)ECA_NORMAL) ? 0u : static_cast<uInt32>(caStatus[i]) + ERROR_OFFSET;
					if (e.code == lvCode && e.source && *e.source && (*e.source)->cnt > 0) {
						needsUpdate = false;
					}
				}
				if (caStatus[i] == ECA_NORMAL) {
					ctx.setErrorAt(i, (uInt32)ECA_NORMAL, std::string(ca_message_safe(ECA_NORMAL)));
				}
				else if (needsUpdate) {
					ctx.setErrorAt(i, (uInt32)caStatus[i], "ca_array_put failed. " + std::string(ca_message_safe(caStatus[i])));
				}
				};
			if (instanceData) { std::lock_guard<std::mutex> lock(instanceData->arrayMutex); updateError(); }
			else { updateError(); }

			if (CommunicationStatus) {
				*CommunicationStatus = (caStatus[i] != ECA_NORMAL);
			}
		}
		else if (isString) {
			if (!StringValueArray2D || !*StringValueArray2D) {
				ctx.setErrorAt(i, (uInt32)ECA_BADCOUNT, "Missing StringValueArray2D"); caStatus[i] = ECA_BADCOUNT;
				if (CommunicationStatus) *CommunicationStatus = 1;
				continue;
			}
			const uInt32 nToWrite = std::min<unsigned>(availableCount, elementCapacity ? elementCapacity : availableCount);

			if (nativeType == DBF_STRING) {
				std::vector<dbr_string_t> temp(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) {
					std::string s = PutValueCtx::getLVString((**StringValueArray2D)->elt[startIndex + j]);
					strncpy(temp[j], s.c_str(), sizeof(dbr_string_t) - 1);
					temp[j][sizeof(dbr_string_t) - 1] = '\0';
				}
				caStatus[i] = ctx.doPut(DBR_STRING, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
			}
			else if (nativeType == DBF_CHAR) {
				std::string s = PutValueCtx::getLVString((**StringValueArray2D)->elt[startIndex]);
				uInt32 count = (uInt32)std::min<size_t>(elementCapacity ? elementCapacity : s.size(), s.size());
				if (count == 0) { dbr_char_t zero = 0; caStatus[i] = ctx.doPut(DBR_CHAR, 1, channelID, &zero, doWaitRequested, putCtx, totalWrites); }
				else { caStatus[i] = ctx.doPut(DBR_CHAR, count, channelID, s.c_str(), doWaitRequested, putCtx, totalWrites); }
			}
			else {
				bool allOk = true;
				std::vector<double> numericValues(nToWrite);
				for (uInt32 j = 0; j < nToWrite; ++j) {
					std::string s = PutValueCtx::getLVString((**StringValueArray2D)->elt[startIndex + j]);
					if (!PutValueCtx::parseNumericString(s, numericValues[j])) { allOk = false; break; }
				}
				if (!allOk) { ctx.setErrorAt(i, (uInt32)ECA_BADTYPE, "Invalid numeric string(s)"); caStatus[i] = ECA_BADTYPE; continue; }

				if (nativeType == DBF_DOUBLE) {
					std::vector<dbr_double_t> temp(nToWrite);
					for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_double_t>(numericValues[j]);
					caStatus[i] = ctx.doPut(DBR_DOUBLE, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
				}
				else if (nativeType == DBF_FLOAT) {
					std::vector<dbr_float_t> temp(nToWrite);
					for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_float_t>(numericValues[j]);
					caStatus[i] = ctx.doPut(DBR_FLOAT, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
				}
				else if (nativeType == DBF_SHORT) {
					std::vector<dbr_short_t> temp(nToWrite);
					for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_short_t>(std::max<long long>(std::numeric_limits<dbr_short_t>::min(), std::min<long long>(std::numeric_limits<dbr_short_t>::max(), static_cast<long long>(numericValues[j]))));
					caStatus[i] = ctx.doPut(DBR_SHORT, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
				}
				else if (nativeType == DBF_ENUM) {
					std::vector<dbr_enum_t> temp(nToWrite);
					for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_enum_t>(std::max<long long>(0, std::min<long long>(std::numeric_limits<dbr_enum_t>::max(), static_cast<long long>(numericValues[j]))));
					caStatus[i] = ctx.doPut(DBR_ENUM, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
				}
				else if (nativeType == DBF_LONG) {
					std::vector<dbr_long_t> temp(nToWrite);
					for (uInt32 j = 0; j < nToWrite; ++j) temp[j] = static_cast<dbr_long_t>(std::max<long long>(std::numeric_limits<dbr_long_t>::min(), std::min<long long>(std::numeric_limits<dbr_long_t>::max(), static_cast<long long>(numericValues[j]))));
					caStatus[i] = ctx.doPut(DBR_LONG, nToWrite, channelID, temp.data(), doWaitRequested, putCtx, totalWrites);
				}
			}

			auto updateError = [&]() {
				const bool handleOk = (ErrorArray && *ErrorArray && (**ErrorArray) && i < (**ErrorArray)->dimSize);
				bool needsUpdate = true;
				if (handleOk) {
					sError& e = (**ErrorArray)->result[i];
					const uInt32 lvCode = (caStatus[i] <= (int)ECA_NORMAL) ? 0u : static_cast<uInt32>(caStatus[i]) + ERROR_OFFSET;
					if (e.code == lvCode && e.source && *e.source && (*e.source)->cnt > 0) {
						needsUpdate = false;
					}
				}
				if (caStatus[i] == ECA_NORMAL) {
					ctx.setErrorAt(i, (uInt32)ECA_NORMAL, std::string(ca_message_safe(ECA_NORMAL)));
				}
				else if (needsUpdate) {
					ctx.setErrorAt(i, (uInt32)caStatus[i], "ca_array_put failed. " + std::string(ca_message_safe(caStatus[i])));
				}
				};
			if (instanceData) { std::lock_guard<std::mutex> lock(instanceData->arrayMutex); updateError(); }
			else { updateError(); }

			if (CommunicationStatus) {
				*CommunicationStatus = (caStatus[i] != ECA_NORMAL);
			}
		}
	}
	ca_poll();

	// If using callbacks, wait for all write callbacks to complete.
	if (doWaitRequested) {
		bool callbacksTimedOut = false;
		if (totalWrites > 0) {
			const auto writeDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(Timeout * 1000.0));
			while (std::chrono::steady_clock::now() < writeDeadline && putCtx->completed.load(std::memory_order_relaxed) < totalWrites) {
				ca_pend_event(0.02);
				Globals::getInstance().waitForNotification(std::chrono::milliseconds(10));
			}
			if (putCtx->completed.load(std::memory_order_relaxed) < totalWrites) {
				callbacksTimedOut = true;
			}
		}
		if (putCtx && putCtx->refs.fetch_sub(1, std::memory_order_acq_rel) == 1) {
			delete putCtx;
			putCtx = nullptr;
		}
		putCtx = nullptr;
		putCallbacksTimedOut = callbacksTimedOut;
	}

	// Finalize status.
	int errorCount = 0;
	for (uInt32 i = 0; i < nameCount; ++i) {
		if (caStatus[i] == ECA_NORMAL) continue;
		++errorCount;
	}
	if (putCallbacksTimedOut) {
		++errorCount;
	}
	if (CommunicationStatus && errorCount) {
		*CommunicationStatus = 1;
	}
	if (FirstCall && *FirstCall) *FirstCall = 0;
}

extern "C" EXPORT void info(sStringArray2DHdl* InfoStringArray2D, sResultArrayHdl* ResultArray, LVBoolean* FirstCall) {
	// Don't enter if library is shutting down.
	Globals& g = Globals::getInstance();
	if (g.stopped.load()) return;

	// Collect and populate environment information
	auto envInfo = collectEnvironmentInfo();
	populateInfoStringArray(InfoStringArray2D, envInfo);

	// Collect and populate PV information
	populateAllResultArray(ResultArray);

	if (FirstCall && *FirstCall) *FirstCall = 0;
}

extern "C" EXPORT void disconnectPVs(sStringArrayHdl* PvNameArray, bool All) {
	Globals& g = Globals::getInstance();
	if (g.stopped.load()) return;

	if (All) {
		std::lock_guard<std::mutex> lock(g.instancesMutex);
		if (g.instances.size() > 1) {
			// name of instance
			std::string instNames;
			int numberOfNames = 0;
			for (auto* instPtr : g.instances) {
				if (instPtr && *instPtr) {
					MyInstanceData* inst = static_cast<MyInstanceData*>(*instPtr);
					if (inst && !inst->firstFunctionName.empty()) {
						if (!instNames.empty()) instNames += ", ";
						instNames += inst->firstFunctionName;
						numberOfNames++;
					}
				}
			}
			if (numberOfNames > 1) {
				return;
			}
		}
	}

	CaContextGuard _caThreadAttach;

	// Collect list of items to be disconnected without holding the registry write lock for long.
	std::vector<PVItem*> itemsToDisconnect;

	if (All) {
		TimeoutSharedLock<std::shared_timed_mutex> rlock(g.pvRegistryLock, "disconnectPVs-all", std::chrono::milliseconds(500));
		if (!rlock.isLocked()) {
			CaLabDbgPrintf("disconnectPVs: Failed to acquire pvRegistryLock (all)");
			return;
		}
		itemsToDisconnect.reserve(g.pvRegistry.size());
		for (auto& kv : g.pvRegistry) {
			if (kv.second) itemsToDisconnect.push_back(kv.second.get());
		}
	}
	else {
		// Validate input.
		if (!PvNameArray || !*PvNameArray || DSCheckHandle(*PvNameArray) != noErr || !(**PvNameArray) || (**PvNameArray)->dimSize == 0) {
			return;
		}
		TimeoutSharedLock<std::shared_timed_mutex> rlock(g.pvRegistryLock, "disconnectPVs-some", std::chrono::milliseconds(500));
		if (!rlock.isLocked()) {
			CaLabDbgPrintf("disconnectPVs: Failed to acquire pvRegistryLock");
			return;
		}

		auto getLVString = [](LStrHandle h) -> std::string {
			if (!h) return {};
			const char* s = reinterpret_cast<const char*>((*h)->str);
			const size_t n = static_cast<size_t>((*h)->cnt);
			return std::string(s, n);
			};

		// Deduplicate and collect potential parents in one pass.
		std::unordered_set<PVItem*> uniq;
		std::unordered_set<PVItem*> potentialParents;
		uniq.reserve((**PvNameArray)->dimSize * 3);
		potentialParents.reserve((**PvNameArray)->dimSize);

		for (uInt32 i = 0; i < (**PvNameArray)->dimSize; ++i) {
			std::string name = getLVString((**PvNameArray)->elt[i]);
			if (name.empty()) continue;

			// Direct item (can be base, field or .RTYP)
			auto it = g.pvRegistry.find(name);
			if (it != g.pvRegistry.end() && it->second) {
				PVItem* item = it->second.get();
				uniq.insert(item);
				potentialParents.insert(item);
			}
		}

		// If any of the requested items are base PVs, add all their children.
		if (!potentialParents.empty()) {
			for (auto& kv : g.pvRegistry) {
				if (!kv.second) continue;
				PVItem* child = kv.second.get();
				PVItem* parentPtr = nullptr;
				{
					std::lock_guard<std::mutex> lk(child->ioMutex());
					parentPtr = child->parent;
				}
				if (parentPtr && potentialParents.find(parentPtr) != potentialParents.end()) {
					uniq.insert(child);
				}
			}
		}

		itemsToDisconnect.reserve(itemsToDisconnect.size() + uniq.size());
		for (PVItem* p : uniq) itemsToDisconnect.push_back(p);
	}

	// Perform disconnection (outside registry lock), update internal PV status.
	for (PVItem* item : itemsToDisconnect) {
		if (!item) continue;

		evid ev = nullptr;
		chid ch = nullptr;
		std::string itemName;
		{
			std::lock_guard<std::mutex> lk(item->ioMutex());
			// Save CA handles, then set to null internally.
			ev = item->eventId;
			ch = item->channelId;
			item->eventId = nullptr;
			item->channelId = nullptr;

			// Internal status: disconnected and data released.
			item->setConnected(false);
			item->setHasValue(false);
			item->setStatus(epicsAlarmComm);
			item->setSeverity(epicsSevInvalid);
			item->setErrorCode(ECA_DISCONNCHID);
			item->clearDbr();
			item->clearFields();
			item->updateChangeHash();
			itemName = item->getName();
		}

		// Release CA resources.
		if (ev) {
			int st = ca_clear_subscription(ev);
			if (st != ECA_NORMAL) {
				CaLabDbgPrintf("disconnectPVs: ca_clear_subscription failed (%d) for %s", st, itemName.c_str());
			}
		}
		if (ch) {
			int st = ca_clear_channel(ch);
			if (st != ECA_NORMAL) {
				CaLabDbgPrintf("disconnectPVs: ca_clear_channel failed (%d) for %s", st, itemName.c_str());
			}
		}

		// Remove from pending list (if it was there).
		g.removePendingConnection(item);

		// Post events for this PV (and parent if applicable).
		postEventForPv(itemName);
		{
			PVItem* parentPtr = nullptr;
			std::lock_guard<std::mutex> lk(item->ioMutex());
			parentPtr = item->parent;
			if (parentPtr) {
				postEventForPv(parentPtr->getName());
			}
		}
	}

	// Process CA and wake up waiting threads.
	ca_flush_io();
	ca_poll();
	g.notify();
}

extern "C" EXPORT uInt32 getCounter() {
	return ++globalCounter;
}

// =================================================================================
// LabVIEW VI Lifecycle Implementation
// =================================================================================

extern "C" EXPORT MgErr reserved(InstanceDataPtr* instanceState) {
	MyInstanceData* data = new MyInstanceData();
	*instanceState = data;
	{
		std::lock_guard<std::mutex> lock(Globals::getInstance().instancesMutex);
		Globals::getInstance().instances.push_back(instanceState);
	}
	return 0;
}

extern "C" EXPORT MgErr unreserved(InstanceDataPtr* instanceState) {
	try {
		if (!instanceState || !*instanceState) {
			CaLabDbgPrintfD("unreserved: Called with null instanceState");
			return 0;
		}
		MyInstanceData* data = static_cast<MyInstanceData*>(*instanceState);
		if (!data) {
			CaLabDbgPrintfD("unreserved: Instance data pointer is null");
			return 0;
		}

		Globals& g = Globals::getInstance();

		// get name
		std::string funcName = data->firstFunctionName;
		// Early exit for unused instances (no function was ever called)
		if (data->firstFunctionName.empty()) {
			std::lock_guard<std::mutex> lock(g.instancesMutex);
			g.instances.erase(
				std::remove(g.instances.begin(), g.instances.end(), instanceState),
				g.instances.end()
			);
			delete data;
			*instanceState = nullptr;
			return 0;
		}

		// Signal that we're unreserving - this prevents new calls from starting
		data->isUnreserving.store(true, std::memory_order_release);

		// Notify waiting threads immediately
		g.notify();

		// Wait for active calls with timeout and active polling
		const auto waitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
		int iterations = 0;

		while (data->activeCalls.load(std::memory_order_acquire) > 0) {
			if (std::chrono::steady_clock::now() >= waitDeadline) {
				break;
			}
			ca_poll();
			g.waitForNotification(std::chrono::milliseconds(5));
			++iterations;
		}


		MgErr err = noErr;
		bool skipCleanup = false;
		auto markCorruptAndSkip = [&](const char* reason) {
			if (!skipCleanup) {
				CaLabDbgPrintfD("unreserved: corruption detected (%s); skipping further cleanup", reason);
			}
			skipCleanup = true;
		};

		// Helper to check if array is shared by another instance
		auto isArraySharedByOtherInstance = [&](void* arrayHandle) -> bool {
			if (!arrayHandle)
				return false;
			std::lock_guard<std::mutex> lock(g.instancesMutex);
			for (auto* instPtr : g.instances) {
				if (instPtr == instanceState || !instPtr || !*instPtr)
					continue;
				auto* otherInst = static_cast<MyInstanceData*>(*instPtr);
				if (!otherInst)
					continue;

				std::lock_guard<std::mutex> lk(otherInst->arrayMutex);

				if ((otherInst->PvIndexArray && *otherInst->PvIndexArray == arrayHandle) ||
					(otherInst->ResultArray && *otherInst->ResultArray == arrayHandle) ||
					(otherInst->FirstStringValue && *otherInst->FirstStringValue == arrayHandle) ||
					(otherInst->FirstDoubleValue && *otherInst->FirstDoubleValue == arrayHandle) ||
					(otherInst->DoubleValueArray && *otherInst->DoubleValueArray == arrayHandle)) {
					return true;
				}
			}
			return false;
		};



		// === CRITICAL: Collect and reset PVs BEFORE cleaning up arrays ===
		// This must happen for getValue AND putValue instances
		std::vector<PVItem*> pvsToReset;
		if (data->firstFunctionName == "getValue" || data->firstFunctionName == "putValue") {
			// Phase 1: Collect base PVs from PvIndexArray BEFORE it gets cleared
			std::vector<PVItem*> basePvs;
			{
				std::lock_guard<std::mutex> lk(data->arrayMutex);
				if (data->PvIndexArray && *data->PvIndexArray) {
					sLongArrayHdl hdl = data->PvIndexArray;
					sLongArray* arr = *data->PvIndexArray;
					try {
						MgErr hErr = DSCheckHandle(hdl);
						if (hErr != noErr) {
							CaLabDbgPrintfD("unreserved: #1 DSCheckHandle failed for %s PvIndexArray=%p (err=%d)", funcName.c_str(),	static_cast<void*>(arr), hErr);
							markCorruptAndSkip("#1 DSCheckHandle failed");
						}
						else {
							const size_t dim = arr->dimSize;
							if (dim > 1'000'000) { // adjust limit as needed
								CaLabDbgPrintfD("unreserved: #1 %s Implausible dimSize=%" PRIu64 " for PvIndexArray=%p, skipping basePvs", funcName.c_str(), static_cast<uint64_t>(dim), static_cast<void*>(arr));
							}
							else {
								basePvs.reserve(arr->dimSize);
								for (size_t i = 0; i < arr->dimSize; ++i) {
									PvEntryHandle entry{ &data->PvIndexArray, i };
									if (!entry || !entry->pvItem) continue;
									basePvs.push_back(entry->pvItem);
								}
							}
						}
					}
					catch (...) {
						CaLabDbgPrintf("unreserved: Exception while collecting base PVs");
						CaLabDbgPrintf("You should terminated and restart LabVIEW to avoid memory leaks.");
						markCorruptAndSkip("exception collecting base PVs");
						basePvs.clear();
						*data->PvIndexArray = nullptr;
					}
				}
			}

			// Phase 2: Collect child PVs (fields and .RTYP) while base PVs are still valid
			if (!basePvs.empty()) {
				TimeoutSharedLock<std::shared_timed_mutex> regLock(g.pvRegistryLock, "unreserved-collect-children", std::chrono::milliseconds(200));
				if (regLock.isLocked()) {
					std::unordered_set<PVItem*> baseSet(basePvs.begin(), basePvs.end());

					for (auto& kv : g.pvRegistry) {
						if (!kv.second) continue;
						PVItem* pvItem = kv.second.get();

						PVItem* parentPtr = nullptr;
						{
							std::lock_guard<std::mutex> pvLock(pvItem->ioMutex());
							parentPtr = pvItem->parent;
						}

						if (parentPtr && baseSet.find(parentPtr) != baseSet.end()) {
							pvsToReset.push_back(pvItem);
						}
					}
				}
			}

			pvsToReset.insert(pvsToReset.end(), basePvs.begin(), basePvs.end());
		}

		// Phase 3: Reset all collected PVs BEFORE cleaning up arrays
		if (!pvsToReset.empty()) {
			CaContextGuard _caThreadAttach;

			struct ResetInfo {
				PVItem* pvItem = nullptr;
				evid ev = nullptr;
				chid ch = nullptr;
				std::string pvName;
			};

			std::vector<ResetInfo> resetInfos;
			resetInfos.reserve(pvsToReset.size());

			// Set of PVs to reset for O(1) lookup
			std::unordered_set<PVItem*> toResetSet(pvsToReset.begin(), pvsToReset.end());

			// Single shared lock on the registry
			{
				TimeoutSharedLock<std::shared_timed_mutex> regLock(
					g.pvRegistryLock,
					"unreserved-reset",
					std::chrono::milliseconds(200)
				);

				if (!regLock.isLocked()) {
					// Behavior matches previous: if the lock cannot be acquired, abort reset here (conservative).
				}
				else {
					// Single scan through the registry
					for (auto& kv : g.pvRegistry) {
						if (!kv.second) continue;

						PVItem* pvItem = kv.second.get();
						if (!pvItem) continue;

						// Only process PVs we actually want to reset
						if (toResetSet.find(pvItem) == toResetSet.end()) {
							continue;
						}

						ResetInfo info;
						info.pvItem = pvItem;

						// Preserve lock order: registry (shared) first, then PV mutex.
						{
							std::lock_guard<std::mutex> pvLock(pvItem->ioMutex());

							info.ev = pvItem->eventId;
							info.ch = pvItem->channelId;
							info.pvName = pvItem->getName();  // only needed for logging/debugging

							pvItem->eventId = nullptr;
							pvItem->channelId = nullptr;
							pvItem->setConnected(false);
							pvItem->setHasValue(false);
							pvItem->setRecordType("");
							pvItem->clearEnumFetchRequested();
						}

						resetInfos.emplace_back(std::move(info));
					}
				} // regLock released here
			}

			// CA cleanup and pending removal remain outside the locks
			for (const auto& info : resetInfos) {
				if (info.ev) {
					ca_clear_subscription(info.ev);
				}
				if (info.ch) {
					ca_clear_channel(info.ch);
				}
				if (info.pvItem) {
					g.removePendingConnection(info.pvItem);
				}
			}

			ca_flush_io();
		}


		if (!skipCleanup) {
			// NOW clean up arrays owned by this instance (AFTER PV reset)
			// Collect & atomically claim entries while the array buffer is still valid.
			std::vector<PvIndexEntry*> entriesToDelete;
			{
				std::lock_guard<std::mutex> lk(data->arrayMutex);

				if (data->PvIndexArray && *data->PvIndexArray && !isArraySharedByOtherInstance(static_cast<void*>(*data->PvIndexArray))) {
					sLongArrayHdl hdl = data->PvIndexArray;
					sLongArray* arr = *data->PvIndexArray;
					try {
						if (hdl && arr) {
							MgErr hErr = DSCheckHandle(hdl);
							if (hErr != noErr) {
								CaLabDbgPrintfD("unreserved: #2 DSCheckHandle failed for %s PvIndexArray=%p (err=%d)", funcName.c_str(),
									static_cast<void*>(arr), hErr);
							}
							else {
								const uInt64 dim = arr->dimSize;
								if (dim > 1'000'000) { // adjust limit as needed
									CaLabDbgPrintfD("unreserved: #2 Implausible %s dimSize=%" PRIu64 " for PvIndexArray=%p, skipping basePvs", funcName.c_str(), dim, static_cast<void*>(arr));
								}
								else {
									entriesToDelete.reserve(arr->dimSize);
									for (uInt64 i = 0; i < arr->dimSize; ++i) {
										// Interpret the LabVIEW slot as an atomic pointer-sized integer.
										atomic_ptr_t* atomicElt = reinterpret_cast<atomic_ptr_t*>(&arr->elt[i]);
										uintptr_t raw = atomicElt->load(std::memory_order_acquire);
										if (raw == 0) continue;

										uintptr_t expected = raw;
										// Claim the slot by CAS while the array memory is still valid.
										if (atomicElt->compare_exchange_strong(expected, 0, std::memory_order_acq_rel, std::memory_order_acquire)) {
											// Successfully claimed the slot; keep the entry pointer for later deletion.
											PvIndexEntry* entry = reinterpret_cast<PvIndexEntry*>(raw);
											if (entry) {
												// Mark deleting to prevent further acquirers (defensive).
												entry->mark_deleting();
												entriesToDelete.push_back(entry);
											}
										}
									}
								}
							}
						}
					}
					catch (...) {
						CaLabDbgPrintf("unreserved: Exception while collecting PvIndex entries");
						CaLabDbgPrintf("You should terminated and restart LabVIEW to avoid memory leaks.");
						entriesToDelete.clear();
						*data->PvIndexArray = nullptr;
					}
				}

				// Detach the array pointer while we do the actual deletions outside the lock
				data->PvIndexArray = nullptr;
			}

			// Helper: plausibility check for pointer-like values.
			auto isPlausiblePointer = [](uintptr_t p) -> bool {
				// Reject very small addresses and misaligned pointers.
				if (p < 0x10000) return false;
#if (UINTPTR_MAX == 0xffffffff)
				const uintptr_t align = 4;
#else
				const uintptr_t align = 8;
#endif
				if ((p & (align - 1)) != 0) return false;
				return true;
				};

			// Perform wait + deletion for the claimed entries without holding data->arrayMutex.
			if (!entriesToDelete.empty()) {
				const auto hardTimeout = std::chrono::seconds(3);
				std::vector<PvIndexEntry*> deferred;
				for (PvIndexEntry* entry : entriesToDelete) {
					if (!entry) continue;

					const uintptr_t rawAddr = reinterpret_cast<uintptr_t>(entry);
					if (!isPlausiblePointer(rawAddr)) {
						CaLabDbgPrintf("unreserved: Implausible entry pointer 0x%016" PRIxPTR " - scheduling deferred deletion", rawAddr);
						deferred.push_back(entry);
						continue;
					}

					if (!tryReclaimAndDelete(entry, hardTimeout)) {
						deferred.push_back(entry);
					}
				}
				if (!deferred.empty()) {
					CaLabDbgPrintf("unreserved: Starting deferred deletion worker for %zu entries", deferred.size());
					std::thread(deferredDeletionWorker, std::move(deferred)).detach();
					// Short pause to give CA callbacks some time
					ca_pend_event(0.001);
				}
				entriesToDelete.clear();
			}

			if (data->ResultArray && *data->ResultArray) {
				sResultArrayHdl resultHdl = data->ResultArray;
				MgErr hErr = DSCheckHandle(resultHdl);
				if (hErr == noErr) {
					for (uInt32 i = 0; i < (*resultHdl)->dimSize; i++) {
						sResult* currentResult = &(*resultHdl)->result[i];
						if (currentResult == nullptr || currentResult->PVName == nullptr) {
							continue;
						}
						hErr = DSCheckHandle(currentResult->PVName);
						if (hErr == noErr) {
							err += CleanupResult(currentResult);
						}
					}
				}
			}
			data->ResultArray = nullptr;

			{
				std::lock_guard<std::mutex> lk(data->arrayMutex);
				if (data->FirstStringValue && *data->FirstStringValue) {
					// Validate pointer alignment before dereferencing
					const uintptr_t ptrValue = reinterpret_cast<uintptr_t>(*data->FirstStringValue);
					const bool isAligned = (ptrValue % alignof(sStringArray)) == 0;
					const bool isPlausible = ptrValue >= 0x10000;

					if (!isAligned || !isPlausible) {
						CaLabDbgPrintfD("unreserved: FirstStringValue pointer 0x%016" PRIxPTR " is invalid (aligned=%d, plausible=%d); skipping cleanup",
							ptrValue, isAligned, isPlausible);
					}
					else {
						MgErr hErr = DSCheckHandle((*data->FirstStringValue));
						if (hErr == noErr) {
							const uInt32 dim = static_cast<uInt32>((*data->FirstStringValue)->dimSize);
							if (dim > 1'000'000) {
								CaLabDbgPrintfD("unreserved: Implausible dimSize=%u for FirstStringValue; skipping cleanup", dim);
							}
							else {
								for (uInt32 j = 0; j < dim; j++) {
									if ((*data->FirstStringValue)->elt[j] && DSCheckHandle((*data->FirstStringValue)->elt[j]) == noErr)
										err += DSDisposeHandle((*data->FirstStringValue)->elt[j]);
									(*data->FirstStringValue)->elt[j] = nullptr;
								}
							}
						}
					}
				}
				data->FirstStringValue = nullptr;
				data->FirstDoubleValue = nullptr;
				data->DoubleValueArray = nullptr;
			}
		}
		else {
			std::lock_guard<std::mutex> lk(data->arrayMutex);
			data->PvIndexArray = nullptr;
			data->ResultArray = nullptr;
			data->FirstStringValue = nullptr;
			data->FirstDoubleValue = nullptr;
			data->DoubleValueArray = nullptr;
		}

		g.unregisterArraysForInstance(instanceState);

		{
			std::lock_guard<std::mutex> lock(g.instancesMutex);
			g.instances.erase(
				std::remove(g.instances.begin(), g.instances.end(), instanceState),
				g.instances.end()
			);
		}

		delete data;
		*instanceState = nullptr;

		if (err != noErr) {
			CaLabDbgPrintf("unreserved: Error releasing handles: %d", err);
		}

		return 0;
	} catch (const std::exception& ex) {
		CaLabDbgPrintf("unreserved: Exception during unreserve: %s", ex.what());
		return -1;
	}
	catch (...) {
		CaLabDbgPrintf("unreserved: Unknown exception during unreserve");
		return -1;
	}
}

extern "C" EXPORT MgErr aborted(InstanceDataPtr* instanceState) {
	return unreserved(instanceState);
}


// =================================================================================
// LabVIEW User Event API Implementation
// =================================================================================

extern "C" EXPORT void addEvent(LVUserEventRef* RefNum, sResult* ResultPtr) {
	Globals& g = Globals::getInstance();
	if (g.stopped.load()) return;

	if (!RefNum || !ResultPtr || DSCheckPtr(ResultPtr) != noErr || !ResultPtr->PVName || DSCheckHandle(ResultPtr->PVName) != noErr) {
		return;
	}
	std::string pvName;
	LStrHandle h = ResultPtr->PVName;
	if (h && *h) {
		pvName.assign(reinterpret_cast<const char*>((*h)->str), (*h)->cnt);
	}
	if (pvName.empty()) return;

	PVItem* pvItem = nullptr;
	bool valueAlreadyExists = false;

	// Ensure the PV exists in the registry.
	{
		TimeoutUniqueLock<std::shared_timed_mutex> lock(g.pvRegistryLock, "addEvent:pvRegistryLock", std::chrono::milliseconds(500));
		if (lock.isLocked()) {
			auto it = g.pvRegistry.find(pvName);
			if (it == g.pvRegistry.end()) {
				auto newPvItem = std::make_unique<PVItem>(pvName);
				pvItem = newPvItem.get();
				g.pvRegistry.emplace(pvName, std::move(newPvItem));
				valueAlreadyExists = false;
			}
			else {
				pvItem = it->second.get();
				if (pvItem) {
					valueAlreadyExists = pvItem->hasValue();
				}
			}
		}
		else {
			CaLabDbgPrintf("addEvent: lock timeout on pvRegistryLock");
		}
	}

	// Register the (RefNum, ResultPtr) pair for this PV.
	{
		std::lock_guard<std::mutex> lock(g_eventRegistry.mtx);
		g_eventRegistry.map[pvName].emplace_back(*RefNum, ResultPtr);
	}

	// If the PV already has a value, post an event immediately.
	if (valueAlreadyExists) {
		postEventForPv(pvName);
	}
	g.notify();
}

extern "C" EXPORT void destroyEvent(LVUserEventRef* RefNum) {
	Globals& g = Globals::getInstance();
	if (g.stopped.load() || !RefNum) return;

	{
		std::lock_guard<std::mutex> lock(g_eventRegistry.mtx);
		for (auto it = g_eventRegistry.map.begin(); it != g_eventRegistry.map.end(); ) {
			auto& vec = it->second;
			vec.erase(std::remove_if(vec.begin(), vec.end(),
				[&](const auto& pair) { return pair.first == *RefNum; }), vec.end());

			if (vec.empty()) {
				it = g_eventRegistry.map.erase(it);
			}
			else {
				++it;
			}
		}
	}
	g.notify();
}

void postEventForPv(const std::string& pvName) {
	if (pvName.empty()) return;

	// Snapshot subscribers to avoid holding the lock during LV calls.
	std::vector<std::pair<LVUserEventRef, sResult*>> subscribers;
	{
		std::lock_guard<std::mutex> lock(g_eventRegistry.mtx);
		auto it = g_eventRegistry.map.find(pvName);
		if (it == g_eventRegistry.map.end()) return;
		subscribers = it->second;
	}

	// Locate the PVItem.
	PVItem* pvItem = nullptr;
	{
		TimeoutSharedLock<std::shared_timed_mutex> rlock(Globals::getInstance().pvRegistryLock, "postEvent-lookup", std::chrono::milliseconds(200));
		if (!rlock.isLocked()) return;
		auto it = Globals::getInstance().pvRegistry.find(pvName);
		if (it == Globals::getInstance().pvRegistry.end() || !it->second) return;
		pvItem = it->second.get();
	}

	// Prepare and post an event for each subscriber.
	for (auto& sub : subscribers) {
		LVUserEventRef ref = sub.first;
		sResult* target = sub.second;
		if (!target) continue;

		// Update the target cluster with the current PV state.
		{
			std::lock_guard<std::mutex> lock(pvItem->ioMutex());
			fillResultFromPv(pvItem, target);
		}

		// Fire the LabVIEW user event.
		MgErr postErr = mgNoErr;
		try {
			postErr = PostLVUserEvent(ref, target);
		} catch (...) {
			CaLabDbgPrintf("postEventForPv: Exception while posting event for %s", pvName.c_str());
			continue;
		}
		if (postErr != mgNoErr) {
			// remove event from registry
			try {
				std::lock_guard<std::mutex> lock(g_eventRegistry.mtx);
				auto it = g_eventRegistry.map.find(pvName);
				if (it != g_eventRegistry.map.end()) {
					auto& vec = it->second;
					vec.erase(std::remove_if(vec.begin(), vec.end(),
						[&](const auto& pair) { return pair.first == ref; }), vec.end());
					if (vec.empty()) {
						g_eventRegistry.map.erase(it);
					}
				}
			}
			catch (...) {
				CaLabDbgPrintf("postEventForPv: Exception while removing failed event for %s", pvName.c_str());
			}
			CaLabDbgPrintf("PostLVUserEvent failed for %s with error %d", pvName.c_str(), postErr);
		}
	}
}

// =================================================================================
// Core PV & Channel Access Logic
// =================================================================================

bool setupPvIndexAndRegistry(sStringArrayHdl* PvNameArray, sStringArrayHdl* FieldNameArray, sLongArrayHdl* PvIndexArray, LVBoolean* CommunicationStatus, std::unordered_set<std::string>* uninitializedPvNames)
{
	uInt32 nameCount = static_cast<uInt32>((**PvNameArray)->dimSize);

	TimeoutUniqueLock<std::shared_timed_mutex> initializeGetValueGuard(
		Globals::getInstance().pvRegistryLock, "initializeGetValue", std::chrono::milliseconds(500));

	if (!initializeGetValueGuard.isLocked()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		TimeoutUniqueLock<std::shared_timed_mutex> retryGuard(
			Globals::getInstance().pvRegistryLock, "initializeGetValue-retry", std::chrono::milliseconds(1000));

		if (!retryGuard.isLocked()) {
			CaLabDbgPrintf("Error: Failed to acquire lock in initializeGetValue after retry.");
			*CommunicationStatus = 1;
			return false;
		}
		// ... (retry guard processing - same changes apply below)
	}

	// Clean up existing entries first
	if (PvIndexArray && *PvIndexArray && **PvIndexArray) {
		if (PvIndexArray && *PvIndexArray && **PvIndexArray) {
			for (uInt32 i = 0; i < (**PvIndexArray)->dimSize; ++i) {
				// Atomically claim and delete if present
				tryClaimAndDeletePvIndexEntry(&(**PvIndexArray)->elt[i]);
			}
		}
	}

	if (PvIndexArray != nullptr && (*PvIndexArray == nullptr || **PvIndexArray == nullptr || (**PvIndexArray)->dimSize != nameCount)) {
		if (NumericArrayResize(iQ, 1, (UHandle*)PvIndexArray, nameCount) != noErr) {
			CaLabDbgPrintf("Error: Memory allocation failed for PvIndexArray in getValue.");
			*CommunicationStatus = 1;
			return false;
		}
		if (**PvIndexArray == nullptr) {
			const size_t headerSize = sizeof(sLongArray);  // dimSize + 1 element
			const size_t elemSize = sizeof(uint64_t);   // sizeof(uint64_t)
			const size_t totalSize = headerSize + (static_cast<size_t>(nameCount) - 1) * elemSize;
			*PvIndexArray = reinterpret_cast<sLongArrayHdl>(DSNewHClr(static_cast<uInt32>(totalSize)));
			if (!*PvIndexArray) {
				CaLabDbgPrintf("Error: Memory allocation failed for PvIndexArray in getValue.");
				*CommunicationStatus = 1;
				return false;
			}
		}
		else {
			(**PvIndexArray)->dimSize = nameCount;
		}

		// Sanity check: LabVIEW array element size must equal pointer size
		if (sizeof((**PvIndexArray)->elt[0]) < sizeof(uintptr_t)) {
			CaLabDbgPrintf("Error: PvIndexArray element size (%zu) != expected pointer size (%zu).",
				sizeof((**PvIndexArray)->elt[0]), sizeof(uintptr_t));
			*CommunicationStatus = 1;
			return false;
		}
		if (*PvIndexArray && **PvIndexArray) {
			sanitizePvIndexArray(**PvIndexArray, static_cast<size_t>(nameCount));
		}
	}

	if (PvIndexArray != nullptr && *PvIndexArray != nullptr && **PvIndexArray != nullptr) {
		for (uInt32 i = 0; i < nameCount; ++i) {
			std::string pvName((char*)(*(**PvNameArray)->elt[i])->str, (size_t)(*(**PvNameArray)->elt[i])->cnt);

			auto it = Globals::getInstance().pvRegistry.find(pvName);
			PVItem* pvItem = nullptr;

			if (it == Globals::getInstance().pvRegistry.end()) {
				// New PV - create it
				auto newPvItem = std::make_unique<PVItem>(pvName);
				pvItem = newPvItem.get();
				Globals::getInstance().pvRegistry[pvName] = std::move(newPvItem);
				if (uninitializedPvNames) uninitializedPvNames->insert(pvItem->getName());
			}
			else {
				// Existing PV - check if it needs reconnection
				pvItem = it->second.get();
				bool needsReconnect = false;
				{
					std::lock_guard<std::mutex> lk(pvItem->ioMutex());

					// Check multiple conditions for stale connections
					bool hasNoChannel = (pvItem->channelId == nullptr);
					bool notConnected = !pvItem->isConnected();
					bool hasNoValue = !pvItem->hasValue();
					bool hasStaleChannel = false;

					// Validate channel state if channel exists
					if (pvItem->channelId != nullptr) {
						channel_state state = ca_state(pvItem->channelId);
						if (state != cs_conn) {
							hasStaleChannel = true;
							// Clear stale CA handles
							pvItem->eventId = nullptr;
							pvItem->channelId = nullptr;
							pvItem->setConnected(false);
							pvItem->setHasValue(false);
							pvItem->setRecordType(""); // Clear record type to force RTYP fetch
							pvItem->clearEnumFetchRequested();
						}
					}

					needsReconnect = hasNoChannel || notConnected || hasNoValue || hasStaleChannel;

				}

				if (needsReconnect && uninitializedPvNames) {
					uninitializedPvNames->insert(pvItem->getName());
				}
			}

			if (FieldNameArray && *FieldNameArray && **FieldNameArray && (**FieldNameArray)->dimSize > 0) {
				std::vector<std::pair<std::string, chanId>> fields;
				fields.reserve((**FieldNameArray)->dimSize);
				for (uInt32 f = 0; f < (**FieldNameArray)->dimSize; ++f) {
					LStrHandle fHandle = (**FieldNameArray)->elt[f];
					if (!fHandle) continue;
					const char* fStr = reinterpret_cast<const char*>((*fHandle)->str);
					const size_t fLen = static_cast<size_t>((*fHandle)->cnt);
					if (fLen == 0) continue;
					std::string fieldName(fStr, fLen);
					fields.emplace_back(fieldName, nullptr);
				}
				pvItem->setFields(fields);
			}

			// Create meta and entry and store atomically in the array
			auto metaInfo = new PVMetaInfo(pvItem);
			PvIndexEntry* entry = new PvIndexEntry(pvItem, metaInfo);

			// Use pointer-sized atomic for portability
			auto atomicElt = reinterpret_cast<atomic_ptr_t*>(&(**PvIndexArray)->elt[i]);
			atomicElt->store(reinterpret_cast<uintptr_t>(entry), std::memory_order_release);
		}
	}
	else {
		CaLabDbgPrintf("Error: PvIndexArray is null.");
		return false;
	}

	return true;
}

void subscribePv(PVItem* pvItem) {
	Globals& g = Globals::getInstance();
	if (g.stopped.load()) return;
	short dbrType;
	uInt32 numValues;
	{
		std::lock_guard<std::mutex> lock(pvItem->ioMutex());
		if (pvItem->eventId != nullptr) {
			return;
		}
		if (pvItem->channelId == nullptr) {
			pvItem->setErrorCode(ECA_DISCONNCHID);
			return;
		}
		channel_state state = ca_state(pvItem->channelId);
		if (state != cs_conn) {
			pvItem->setErrorCode(ECA_DISCONN);
			return;
		}
		dbrType = pvItem->getDbrType();
		numValues = pvItem->getNumberOfValues();
	}
	g.addPendingConnection(pvItem);

	int requestedDbrType = dbf_type_to_DBR_TIME(dbrType);

	int rc = ca_create_subscription(requestedDbrType, numValues, pvItem->channelId, DBE_VALUE | DBE_ALARM, valueChanged, pvItem, &pvItem->eventId);
	if (rc != ECA_NORMAL) {
		{
			std::lock_guard<std::mutex> lock(pvItem->ioMutex());
			pvItem->setErrorCode(rc);
			if (rc != ECA_NORMAL) pvItem->eventId = nullptr;
		}
		CaLabDbgPrintf("Warning: Could not create subscription for %s. %s", pvItem->getName().c_str(), ca_message_safe(rc));
	}
}

void connectPVs(const std::unordered_set<std::string>& basePvNames, double Timeout, std::chrono::steady_clock::time_point endBy) {
	Globals& g = Globals::getInstance();
	if (basePvNames.empty()) return;
	const size_t count = basePvNames.size();
	long offset = 1;
	long long timeoutMs = static_cast<long long>(Timeout * 1000.0 * offset);
	auto ownDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	auto deadline = std::min(ownDeadline, endBy);

	struct ParentRef {
        std::string baseName;
        PVItem* parent = nullptr;
    };

	std::vector<ParentRef> parents;
    parents.reserve(count);
    {
        TimeoutSharedLock<std::shared_timed_mutex> scanLock(g.pvRegistryLock, "connectPVs-scan", std::chrono::milliseconds(200));
        if (!scanLock.isLocked()) return;

        for (const auto& baseName : basePvNames) {
            auto it = g.pvRegistry.find(baseName);
            if (it == g.pvRegistry.end() || !it->second) continue;
            parents.push_back({ baseName, it->second.get() });
        }
    }

    auto waitForRecordType = [&](ParentRef& ref) -> bool {
        if (!ref.parent) return false;
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> lk(ref.parent->ioMutex());
                if (!ref.parent->getRecordType().empty()) {
                    return true;
                }
            }
            g.waitForNotification(std::chrono::milliseconds(20));
            ca_pend_event(0.001);
        }
        return false;
    };

    std::vector<ParentRef> readyParents;
    readyParents.reserve(parents.size());
    for (auto& ref : parents) {
        if (!ref.parent) continue;
        bool hasRecordType = false;
        {
            std::lock_guard<std::mutex> lk(ref.parent->ioMutex());
            hasRecordType = !ref.parent->getRecordType().empty();
        }
        if (!hasRecordType && !waitForRecordType(ref)) {
            continue;
        }
        readyParents.push_back(ref);
    }

	// Build and connect field PV channels for approved fields only.
	std::vector<PVItem*> fieldItems;
    fieldItems.reserve(readyParents.size() * 4);

    auto buildFieldChannels = [&](const std::vector<ParentRef>& refs) {
        if (refs.empty()) return;

        TimeoutUniqueLock<std::shared_timed_mutex> lock(g.pvRegistryLock, "connectPVs-build", std::chrono::milliseconds(500));
        if (!lock.isLocked()) return;

        for (const auto& ref : refs) {
            PVItem* parent = ref.parent;
            if (!parent) continue;

            std::string recordType;
            std::vector<std::pair<std::string, chanId>> fields;
            {
                std::lock_guard<std::mutex> lk(parent->ioMutex());
                recordType = parent->getRecordType();
                fields = parent->getFields();
            }
			if (recordType.empty() || fields.empty()) {
				continue;
			}
            for (const auto& fieldPair : fields) {
                const std::string& fieldName = fieldPair.first;
                bool isApproved = Globals::getInstance().recordFieldIsCommonField(fieldName) ||
                    Globals::getInstance().recordFieldExists(recordType, fieldName);
				if (!isApproved) {
					continue;
				}
                std::string fieldPvName = ref.baseName + "." + fieldName;
                PVItem* fieldItem = nullptr;
                auto fit = g.pvRegistry.find(fieldPvName);
                if (fit == g.pvRegistry.end()) {
                    auto newFieldPv = std::make_unique<PVItem>(fieldPvName);
                    fieldItem = newFieldPv.get();
                    fieldItem->parent = parent;
                    g.pvRegistry[fieldPvName] = std::move(newFieldPv);
                }
                else {
                    fieldItem = fit->second.get();
                    if (fieldItem && !fieldItem->parent) {
                        fieldItem->parent = parent;
                    }
                }
                if (!fieldItem) continue;

                if (fieldItem->channelId == nullptr) {
                    g.addPendingConnection(fieldItem);
                    int result = ca_create_channel(fieldPvName.c_str(), connectionChanged, fieldItem, CA_PRIORITY_DEFAULT, &fieldItem->channelId);
                    if (result != ECA_NORMAL) {
                        CaLabDbgPrintf("Warning: Could not create channel for %s. %s", fieldPvName.c_str(), ca_message_safe(result));
                        continue;
                    }
                }
                else if (!fieldItem->isConnected()) {
                    g.addPendingConnection(fieldItem);
                }
                fieldItems.push_back(fieldItem);
            }
        }
    };

    buildFieldChannels(readyParents);
	// Let CA process initial connection handshakes.
	ca_pend_io(0.001);

	// Subscribe immediately to those already connected.
	{
		std::vector<PVItem*> toSubscribe;
		TimeoutSharedLock<std::shared_timed_mutex> lock(g.pvRegistryLock, "connectPVs-initial-sub", std::chrono::milliseconds(200));
		if (lock.isLocked()) {
			for (auto* item : fieldItems) {
				if (item && item->isConnected()) {
					std::lock_guard<std::mutex> lk(item->ioMutex());
					if (item->eventId == nullptr) {
						toSubscribe.push_back(item);
					}
				}
			}
		}
		for (auto* item : toSubscribe) { subscribePv(item); }
		if (!toSubscribe.empty()) ca_poll();
	}

	timeoutMs = static_cast<long long>(Timeout * 1000.0 * offset);
	ownDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
	deadline = ownDeadline;
	// Dynamically subscribe to fields as they connect, up to the deadline.
	while (std::chrono::steady_clock::now() < deadline) {
		std::vector<PVItem*> toSubscribeDynamic;
		bool allDone = true; // All fields are either subscribed (eventId set) or no channel is pending.
		{
			TimeoutSharedLock<std::shared_timed_mutex> lock(g.pvRegistryLock, "connectPVs-dyn", std::chrono::milliseconds(200));
			if (!lock.isLocked()) break;
			for (auto* item : fieldItems) {
				if (!item) continue;
				bool needsSubscription = false;
				{
					std::lock_guard<std::mutex> lk(item->ioMutex());
					needsSubscription = (item->isConnected() && item->eventId == nullptr);
					// If not connected yet, we are not done.
					if (!item->isConnected()) allDone = false;
				}
				if (needsSubscription) toSubscribeDynamic.push_back(item);
			}
		}
		for (auto* item : toSubscribeDynamic) { subscribePv(item); }
		if (!toSubscribeDynamic.empty()) ca_poll();
		if (allDone) break;
		g.waitForNotification(std::chrono::milliseconds(100));
		ca_pend_event(0.001);
	}

	// Optional: warn on timeout for fields that are still not connected.
	/*if (std::chrono::steady_clock::now() >= deadline) {
		for (auto* item : fieldItems) {
			if (item && !item->isConnected()) {
				CaLabDbgPrintf("Warning: Field PV %s did not connect before timeout.", item->getName().c_str());
			}
		}
	}*/
}

void waitForUninitializedPvs(const std::unordered_set<std::string>& basePvNames, double Timeout, std::chrono::steady_clock::time_point endBy) {
	if (basePvNames.empty() || Timeout <= 0.0)
		return;
	Globals& g = Globals::getInstance();
	long offset = 1;
	const auto ownDeadline = std::chrono::steady_clock::now() + std::chrono::milliseconds((long long)((Timeout > 0.0 ? Timeout : 0.0) * 1000 * offset));
	const auto deadline = std::min(ownDeadline, endBy);

	while (std::chrono::steady_clock::now() < deadline) {
		bool allFieldsReady = true;

		// Collect parents and their field-name lists under a registry lock to minimize contention.
		std::vector<std::pair<PVItem*, std::vector<std::string>>> parents;
		{
			TimeoutSharedLock<std::shared_timed_mutex> lock(g.pvRegistryLock, "waitFieldValues-collect", std::chrono::milliseconds(200));
			if (!lock.isLocked()) break;
			parents.reserve(basePvNames.size());
			for (const auto& baseName : basePvNames) {
				auto it = g.pvRegistry.find(baseName);
				if (it == g.pvRegistry.end() || !it->second) continue;
				PVItem* parent = it->second.get();
				std::vector<std::string> fieldNames;
				{
					std::lock_guard<std::mutex> lk(parent->ioMutex());
					const auto& fieldsVector = parent->getFields();
					fieldNames.reserve(fieldsVector.size());
					for (const auto& f : fieldsVector) fieldNames.push_back(f.first);
				}
				if (!fieldNames.empty()) {
					parents.emplace_back(parent, std::move(fieldNames));
				}
			}
		}

		// Fast path: if there are no parents with fields, we're done.
		if (parents.empty()) return;

		// Check if all listed fields have a value (string) set on their parent.
		for (const auto& parentAndFields : parents) {
			PVItem* parent = parentAndFields.first;
			// Determine record type once per parent to validate fields.
			std::string recordType;
			{
				std::lock_guard<std::mutex> lk(parent->ioMutex());
				recordType = parent->getRecordType();
			}
			// If the record type is unknown here, skip waiting for fields of this parent.
			if (recordType.empty()) continue;
			for (const auto& fieldName : parentAndFields.second) {
				// Only wait for approved fields or those actually present in the registry.
				bool isApproved = Globals::getInstance().recordFieldIsCommonField(fieldName) ||
					Globals::getInstance().recordFieldExists(recordType, fieldName);
				bool fieldPvExists = false;
				{
					TimeoutSharedLock<std::shared_timed_mutex> lock(g.pvRegistryLock, "waitFieldValues-exists", std::chrono::milliseconds(50));
					if (lock.isLocked()) {
						fieldPvExists = (g.pvRegistry.find(parent->getName() + "." + fieldName) != g.pvRegistry.end());
					}
				}
				if (!isApproved && !fieldPvExists) {
					// Not a valid/connected field for this record; don't wait on it.
					continue;
				}
				std::string tempValue;
				if (!parent->tryGetFieldString(fieldName, tempValue) || tempValue.empty()) {
					// As a fallback, if the field PV has a value, the parent will be updated shortly.
					bool fallbackHasValue = false;
					{
						TimeoutSharedLock<std::shared_timed_mutex> lock(g.pvRegistryLock, "waitFieldValues-fallback", std::chrono::milliseconds(100));
						if (lock.isLocked()) {
							auto fit = g.pvRegistry.find(parent->getName() + "." + fieldName);
							if (fit != g.pvRegistry.end() && fit->second && fit->second->hasValue()) {
								fallbackHasValue = true;
							}
						}
					}
					if (!fallbackHasValue) { allFieldsReady = false; break; }
				}
			}
			if (!allFieldsReady) break;
		}

		if (allFieldsReady) break;

		// Process CA events and wait briefly for the next notifications.
		g.waitForNotification(std::chrono::milliseconds(50));
		ca_pend_event(0.001);
	}
}

void connectPv(PVItem* pvItem, bool connectRtyp) {
	Globals& g = Globals::getInstance();
	if (g.stopped.load()) return;
	if (!pvItem) return;

	// Check if channel needs to be created or recreated
	bool needsNewChannel = false;
	{
		std::lock_guard<std::mutex> lk(pvItem->ioMutex());
		if (pvItem->channelId == nullptr) {
			needsNewChannel = true;
		}
		else if (ca_state(pvItem->channelId) != cs_conn) {
			// Channel exists but is not connected - clear it and recreate			
			pvItem->channelId = nullptr;
			pvItem->eventId = nullptr;
			pvItem->setConnected(false);
			needsNewChannel = true;
		}
	}

	if (needsNewChannel) {
		g.addPendingConnection(pvItem);
		int result = ca_create_channel(pvItem->getName().c_str(), connectionChanged, pvItem, CA_PRIORITY_DEFAULT, &pvItem->channelId);
		if (result != ECA_NORMAL) {
			CaLabDbgPrintf("Warning: Could not create channel for %s. %s", pvItem->getName().c_str(), ca_message_safe(result));
			return;
		}
	}

	if (connectRtyp) {
		std::string rtypPvName = pvItem->getName() + ".RTYP";
		auto it = g.pvRegistry.find(rtypPvName);
		PVItem* rtypPvItem = nullptr;

		if (it == g.pvRegistry.end()) {
			auto newPvItem = std::make_unique<PVItem>(rtypPvName);
			rtypPvItem = newPvItem.get();
			rtypPvItem->parent = pvItem;
			g.pvRegistry[rtypPvName] = std::move(newPvItem);
		}
		else {
			rtypPvItem = it->second.get();
			// Ensure parent is set
			if (rtypPvItem && !rtypPvItem->parent) {
				rtypPvItem->parent = pvItem;
			}
		}

		if (rtypPvItem) {
			{
				std::lock_guard<std::mutex> lk(rtypPvItem->ioMutex());
				rtypPvItem->toBeRemoved.store(true);
			}

			// Check if RTYP channel needs recreation
			bool rtypNeedsChannel = false;
			{
				std::lock_guard<std::mutex> lk(rtypPvItem->ioMutex());
				if (rtypPvItem->channelId == nullptr) {
					rtypNeedsChannel = true;
				}
				else if (ca_state(rtypPvItem->channelId) != cs_conn) {
					rtypPvItem->channelId = nullptr;
					rtypPvItem->eventId = nullptr;
					rtypPvItem->setConnected(false);
					rtypPvItem->setHasValue(false);
					rtypNeedsChannel = true;
				}
			}

			if (rtypNeedsChannel) {
				g.addPendingConnection(rtypPvItem);
				int result = ca_create_channel(rtypPvName.c_str(), connectionChanged, rtypPvItem, CA_PRIORITY_DEFAULT, &rtypPvItem->channelId);
				if (result != ECA_NORMAL) {
					CaLabDbgPrintf("Warning: Could not create channel for %s. %s", rtypPvName.c_str(), ca_message_safe(result));
				}
			}
		}
	}
}

void subscribeBasePVs(const std::unordered_set<std::string>& basePvNames, double Timeout, std::chrono::steady_clock::time_point endBy, LVBoolean* NoMDEL) {
	if (basePvNames.empty())
		return;
	Globals& g = Globals::getInstance();
	long offset = 1;
	const auto ownDeadline = std::chrono::steady_clock::now() +
		std::chrono::milliseconds((long long)((Timeout > 0.0 ? Timeout : 0.0) * 1000 * offset));
	const auto deadline = std::min(ownDeadline, endBy);

	// Connect base and .RTYP channels.
	{
		TimeoutUniqueLock<std::shared_timed_mutex> lock(g.pvRegistryLock, "subscribeBasePVs-connect");
		if (!lock.isLocked())
			return;
		for (const auto& name : basePvNames) {
			auto it = g.pvRegistry.find(name);
			if (it != g.pvRegistry.end() && it->second) {
				g.addPendingConnection(it->second.get());
				connectPv(it->second.get(), /*connectRtyp=*/true);
			}
			else {
				CaLabDbgPrintf("Warning: Base PV not found in registry: %s", name.c_str());
			}
		}
	}

	// Flush and process initial connections
	ca_flush_io();
	ca_pend_event(0.01); // Give CA time to process connection callbacks

	// Wait for RTYP values up to the deadline and subscribe channels as they connect.
	int loopCount = 0;
	while (std::chrono::steady_clock::now() < deadline) {
		std::vector<PVItem*> toSubscribeDynamic;
		bool allDone = true;
		{
			TimeoutSharedLock<std::shared_timed_mutex> lock(g.pvRegistryLock, "subscribeBasePVs-rtyp-check", std::chrono::milliseconds(200));
			if (!lock.isLocked()) break;
			for (const auto& name : basePvNames) {
				// Subscribe base PV if it just connected and has no event yet.
				auto bit = g.pvRegistry.find(name);
				if (bit != g.pvRegistry.end() && bit->second) {
					PVItem* baseItem = bit->second.get();
					bool needsSubscription = false;
					{
						std::lock_guard<std::mutex> lk(baseItem->ioMutex());
						needsSubscription = (NoMDEL == nullptr || !*NoMDEL) && (baseItem->isConnected() && baseItem->eventId == nullptr);
						if (!baseItem->isConnected()) {
							allDone = false;
						}
					}
					if (needsSubscription) {
						toSubscribeDynamic.push_back(baseItem);
					}
				}

				// Subscribe .RTYP PV if it just connected and has no event yet.
				auto rit = g.pvRegistry.find(name + ".RTYP");
				if (rit != g.pvRegistry.end() && rit->second) {
					PVItem* rtypItem = rit->second.get();
					bool needsSubscriptionRtyp = false;
					{
						std::lock_guard<std::mutex> lk(rtypItem->ioMutex());
						needsSubscriptionRtyp = (rtypItem->isConnected() && rtypItem->eventId == nullptr);
						if (!rtypItem->isConnected()) allDone = false;
					}
					if (needsSubscriptionRtyp) {
						toSubscribeDynamic.push_back(rtypItem);
					}
				}
				else {
					allDone = false;
				}
			}
		}
		// Perform subscriptions outside the registry lock.
		for (auto* item : toSubscribeDynamic) {
			subscribePv(item);
		}
		if (!toSubscribeDynamic.empty()) {
			ca_flush_io();
			ca_poll();
		}
		if (allDone) {
			break;
		}

		// Use shorter wait intervals for faster reconnection
		int waitMs = (loopCount < 5) ? 20 : 50;
		g.waitForNotification(std::chrono::milliseconds(waitMs));
		ca_pend_event(0.005);
		++loopCount;
	}

	/*if (std::chrono::steady_clock::now() >= deadline) {
		CaLabDbgPrintf("subscribeBasePVs: Timeout after %d iterations", loopCount);
	}*/
}

std::vector<uInt32> getChangedPvIndices(sLongArrayHdl* PvIndexArray, double Timeout, std::chrono::steady_clock::time_point endBy, LVBoolean* NoMDEL /*= nullptr*/) {
	std::vector<uInt32> changedIndices;
	if (!PvIndexArray || !*PvIndexArray) return changedIndices;

	const bool isNoMDEL = (NoMDEL != nullptr && *NoMDEL);
	uInt32 count = (uInt32)(**PvIndexArray)->dimSize;

	// In NoMDEL mode: ALL connected PVs should be fetched every time
	if (isNoMDEL) {
		// Add all connected PVs to changedIndices
		for (uInt32 i = 0; i < count; ++i) {
			PvEntryHandle entry{ PvIndexArray, i};
			if (!entry) continue;
			PVItem* pvItem = entry->pvItem;
			if (pvItem && pvItem->isConnected() && pvItem->channelId) {
				changedIndices.push_back(i);
			}
		}

		// Fetch values synchronously with ca_get for ALL PVs (original logic unchanged)
		if (!changedIndices.empty()) {
			Globals& g = Globals::getInstance();

			// Ensure we're attached to the CA context
			bool wasAttached = (ca_current_context() != nullptr);
			if (!wasAttached && g.pcac) {
				ca_attach_context(g.pcac);
			}

			// Structure to hold pending get operations
			struct PendingGet {
				uInt32 idx{0};
				PVItem* pvItem{nullptr};
				int requestedDbrType{0};
				uInt32 nElems{0};
				size_t elemSize{0};
				std::vector<char> buffer{};
				int rc{ECA_NORMAL};
			};

			std::vector<PendingGet> pendingGets;
			pendingGets.reserve(changedIndices.size());

			// Phase 1: Issue all ca_array_get calls
			for (uInt32 idx : changedIndices) {
				PvEntryHandle entry{ PvIndexArray, idx };
				if (!entry) continue;
				PVItem* pvItem = entry->pvItem;
				if (!pvItem) {continue; }

				if (!pvItem->isConnected() || !pvItem->channelId) {continue; }

				// Verify channel state
				if (ca_state(pvItem->channelId) != cs_conn) {continue; }

				short dbrType = pvItem->getDbrType();
				uInt32 nElems = pvItem->getNumberOfValues();
				int requestedDbrType = dbf_type_to_DBR_TIME(dbrType);

				if (requestedDbrType < 0 ||
					!dbr_size || !dbr_value_offset || !dbr_value_size ||
					static_cast<unsigned short>(requestedDbrType) >= dbr_text_dim)
				{
					CaLabDbgPrintf(
						"getChangedPvIndices: Unsupported or invalid DBR type %d (requested=%d) for PV '%s'; skipping synchronous get.",
						static_cast<int>(dbrType),
						requestedDbrType,
						pvItem->getName().c_str());
					continue;
				}
				// Calculate element size
				size_t elemSize = 0;
				switch (requestedDbrType) {
				case DBR_TIME_CHAR:   elemSize = sizeof(dbr_char_t); break;
				case DBR_TIME_SHORT:  elemSize = sizeof(dbr_short_t); break;
				case DBR_TIME_LONG:   elemSize = sizeof(dbr_long_t); break;
				case DBR_TIME_FLOAT:  elemSize = sizeof(dbr_float_t); break;
				case DBR_TIME_DOUBLE: elemSize = sizeof(dbr_double_t); break;
				case DBR_TIME_ENUM:   elemSize = sizeof(dbr_enum_t); break;
				case DBR_TIME_STRING: elemSize = MAX_STRING_SIZE; break;
				default: elemSize = sizeof(dbr_double_t); break;
				}

				size_t headerSize = dbr_size[requestedDbrType];
				size_t bufSize = headerSize + elemSize * (nElems > 0 ? nElems - 1 : 0);

				PendingGet pg;
				pg.idx = idx;
				pg.pvItem = pvItem;
				pg.requestedDbrType = requestedDbrType;
				pg.nElems = nElems;
				pg.elemSize = elemSize;
				pg.buffer.resize(bufSize);

				// Issue the get request
				pg.rc = ca_array_get(requestedDbrType, nElems, pvItem->channelId, pg.buffer.data());

				if (pg.rc == ECA_NORMAL) {
					pendingGets.push_back(std::move(pg));
				}
				else {
					// Immediate error - set on PV
					std::lock_guard<std::mutex> lock(pvItem->ioMutex());
					pvItem->setErrorCode(pg.rc);
				}
			}

			// Phase 2: Wait for all pending gets with a single ca_pend_io
			int pendIoResult = ECA_NORMAL;
			if (!pendingGets.empty()) {
				ca_flush_io();
				pendIoResult = ca_pend_io(Timeout > 0.0 ? Timeout : 1.0);
			}

			// Phase 3: Collect all results
			for (auto& pg : pendingGets) {
				PVItem* pvItem = pg.pvItem;

				if (pendIoResult == ECA_NORMAL) {
					// Success - process received data
					std::lock_guard<std::mutex> lock(pvItem->ioMutex());

					// Extract time metadata
					const struct dbr_time_double* timePtr =
						reinterpret_cast<const struct dbr_time_double*>(pg.buffer.data());
					pvItem->setTimestamp(timePtr->stamp.secPastEpoch);
					pvItem->setTimestampNSec(timePtr->stamp.nsec);
					pvItem->setStatus(timePtr->status);
					pvItem->setSeverity(timePtr->severity);

					// Copy value data
					size_t dataBytes = pg.elemSize * pg.nElems;
					void* dataCopy = malloc(dataBytes);
					if (dataCopy) {
						memcpy(dataCopy, dbr_value_ptr(pg.buffer.data(), pg.requestedDbrType), dataBytes);
						pvItem->clearDbr();
						pvItem->setDbr(dataCopy);
						pvItem->setHasValue(true);
						pvItem->setErrorCode(ECA_NORMAL);
					}
				}
				else {
					// Timeout or error
					std::lock_guard<std::mutex> lock(pvItem->ioMutex());
					pvItem->setErrorCode(pendIoResult);
				}
			}

			// Detach context if we attached it
			if (!wasAttached && g.pcac) {
				ca_detach_context();
			}
		}

		return changedIndices;
	}

	// Normal mode (with subscriptions): only return PVs that have changes
	bool waitForSubscription = false;
	for (uInt32 i = 0; i < count; ++i) {
		PvEntryHandle entry{ PvIndexArray, i };
		if (!entry) continue;
		if (!entry->metaInfo || !entry->metaInfo->pvItem) { continue; }
		PVItem* pvItem = entry->metaInfo->pvItem;

		if (pvItem->isConnected() && !pvItem->hasValue()) {
			// Normal mode: subscribe and wait for value
			waitForSubscription = true;
			subscribePv(pvItem);
			changedIndices.push_back(i);
		}
		else if (entry->metaInfo->hasChanged()) {
			changedIndices.push_back(i);
		}
	}

	// Normal mode: Wait until all PVs in changedIndices have a value via subscription
	if (waitForSubscription) {
		long offset = 1;
		auto startTime = std::chrono::steady_clock::now();

		double effectiveTimeout = (Timeout > 0.0 ? Timeout : 0.0) * offset;
		auto ownEndTime = startTime + std::chrono::milliseconds(static_cast<long long>(effectiveTimeout * 1000.0));
		auto endTime = std::min(ownEndTime, endBy);

		while (true) {
			bool allHaveValues = true;
			int currentHasValue = 0;

			for (uInt32 idx : changedIndices) {
				PvEntryHandle entry{ PvIndexArray, idx };
				if (!entry) { allHaveValues = false; break; }
				if (entry && entry->metaInfo && entry->metaInfo->pvItem->isConnected() &&
					!entry->metaInfo->pvItem->hasValue()) {
					allHaveValues = false;
				}
				else if (entry && entry->metaInfo && entry->metaInfo->pvItem->hasValue()) {
					currentHasValue++;
				}
				if (!allHaveValues) break;
			}

			if (allHaveValues) {
				break;
			}

			auto currentTime = std::chrono::steady_clock::now();
			if (currentTime > endTime) {
				break;
			}

			Globals::getInstance().waitForNotification(std::chrono::milliseconds(100));
			ca_pend_event(0.001);
		}
	}

	// After values have arrived, check for ENUM PVs that still need their labels.
	std::vector<uInt32> enumPendingIndices;
	for (uInt32 i = 0; i < count; ++i) {
		PvEntryHandle entry{ PvIndexArray, i };
		if (!entry) continue;
		if (!entry->metaInfo || !entry->metaInfo->pvItem) {continue; }
		PVItem* pvItem = entry->metaInfo->pvItem;

		if (pvItem->isConnected() && pvItem->hasValue()) {
			short dbrType = pvItem->getDbrType();
			if (dbrType == DBR_ENUM || dbrType == DBR_TIME_ENUM) {
				const auto& labels = pvItem->getEnumStrings();
				if (labels.empty()) {
					enumPendingIndices.push_back(i);
				}
			}
		}
	}

	// Wait for ENUM labels to arrive
	if (!enumPendingIndices.empty()) {
		auto enumTimeout = std::chrono::milliseconds(500);
		auto ownEndTime = std::chrono::steady_clock::now() + enumTimeout;
		auto endTime = std::min(ownEndTime, endBy);

		while (std::chrono::steady_clock::now() < endTime) {
			bool allEnumsReady = true;
			for (uInt32 idx : enumPendingIndices) {
				PvEntryHandle entry{ PvIndexArray, idx };
				if (!entry) { allEnumsReady = false; break; }
				if (!entry->metaInfo || !entry->metaInfo->pvItem) { allEnumsReady = false; break; }
				PVItem* pvItem = entry->metaInfo->pvItem;
				const auto& labels = pvItem->getEnumStrings();
				if (labels.empty()) {
					allEnumsReady = false;
					break;
				}
			}
			if (allEnumsReady) break;

			Globals::getInstance().waitForNotification(std::chrono::milliseconds(50));
			ca_pend_event(0.001);
		}

		for (uInt32 idx : enumPendingIndices) {
			if (std::find(changedIndices.begin(), changedIndices.end(), idx) == changedIndices.end()) {
				changedIndices.push_back(idx);
			}
		}
	}

	return changedIndices;
}

void updatePvIndexArray(sLongArrayHdl* PvIndexArray, const std::vector<uInt32>& changedIndices /*= {}*/) {
	if (!PvIndexArray || !*PvIndexArray) return;

	if (changedIndices.empty()) {
		// Fallback to a full update if no specific list is provided.
		for (uInt32 i = 0; i < (**PvIndexArray)->dimSize; ++i) {
			PvEntryHandle entry{ PvIndexArray, i };
			if (!entry) continue;
			if (entry->metaInfo) {
				entry->metaInfo->update();
			}
		}
	}
	else {
		// Update only the changed entries.
		for (uInt32 idx : changedIndices) {
			if (idx < (**PvIndexArray)->dimSize) {
				PvEntryHandle entry{ PvIndexArray, idx };
				if (!entry) continue;
				if (entry->metaInfo) {
					entry->metaInfo->update();
				}
			}
		}
	}
}

void populateOutputArrays(uInt32 nameCount, uInt32 maxNumberOfValues, int filter, sLongArrayHdl* PvIndexArray, sResultArrayHdl* ResultArray, sStringArrayHdl* FirstStringValue, sDoubleArrayHdl* FirstDoubleValue, sDoubleArray2DHdl* DoubleValueArray, const std::vector<uInt32>& changedIndices) {
	// Determine the indices to update.
	const std::vector<uInt32> indicesToUpdate = changedIndices.empty() ?
		makeIndexRange(nameCount) : changedIndices;

	// Update only the changed elements.
	for (uInt32 idx : indicesToUpdate) {
		if (idx >= nameCount) continue;

		PvEntryHandle entry{ PvIndexArray, idx };
		if (!entry || !entry->pvItem || !entry->metaInfo) {
			continue;
		}
		PVItem* pvItem = entry->pvItem;
		// Prepare a copy of field names while holding the PV lock, then allocate/fill outside.
		std::vector<std::string> fieldNamesCopy;
		std::vector<std::string> cachedStringValues;
		std::vector<double> cachedNumericValues;
		short dbrType = DBR_CHAR;
		bool isNumericType = false;

		// Hold the entry acquired for the duration we use metaInfo/pvItem.
		{
			std::lock_guard<std::mutex> lock(pvItem->ioMutex());
			const bool needsStringValues = (filter & (out_filter::firstValueAsString | out_filter::pviValuesAsString)) != 0;
			const bool needsNumericValues = (filter & (out_filter::firstValueAsNumber | out_filter::pviValuesAsNumber | out_filter::valueArrayAsNumber)) != 0;

			dbrType = pvItem->getDbrType();
			isNumericType = (dbrType >= DBR_CHAR && dbrType <= DBR_DOUBLE) || (dbrType >= DBR_TIME_CHAR && dbrType <= DBR_TIME_DOUBLE);

			if (needsNumericValues || (needsStringValues && isNumericType)) {
				cachedNumericValues = pvItem->dbrValue2Double();
			}

			if (needsStringValues && !isNumericType) {
				cachedStringValues = pvItem->dbrValue2String(); // Native Strings
			}
		}

		if (!cachedNumericValues.empty() && isNumericType && (filter & (out_filter::firstValueAsString | out_filter::pviValuesAsString))) {
			cachedStringValues.reserve(cachedNumericValues.size());
			for (double val : cachedNumericValues) {
				cachedStringValues.emplace_back(pvItem->FormatUnit(val, std::string()));
			}
		}

		{
			std::lock_guard<std::mutex> lock(pvItem->ioMutex());

			// Ensure ErrorIO reflects the current error state for all PVs, not just changed ones.
			if ((filter & out_filter::pviError) && ResultArray && *ResultArray && PvIndexArray && *PvIndexArray && **PvIndexArray) {
				// Fast skip if no PV currently has an error.
				if (Globals::getInstance().pvErrorCount.load(std::memory_order_relaxed) > 0) {
					for (uInt32 i = 0; i < nameCount; ++i) {
						sResult* currentResult = &(**ResultArray)->result[i];
						PvEntryHandle currentEntry{ PvIndexArray, i };
						if (!currentEntry || !currentEntry->pvItem) {
							continue;
						}
						const int err = currentEntry->pvItem->getErrorCode();
						const uInt32 lvCode = (err <= (int)ECA_NORMAL) ? 0u : static_cast<uInt32>(err) + ERROR_OFFSET;
						if (currentResult->ErrorIO.code == lvCode && currentResult->ErrorIO.source && *currentResult->ErrorIO.source && (*currentResult->ErrorIO.source)->cnt) {
							// No change; skip updating the string to avoid unnecessary memory operations.
						}
						else {
							currentResult->ErrorIO.code = lvCode;
							currentResult->ErrorIO.status = 0;
							// Always provide a message; on success this will be "Normal successful completion".
							setLVString(currentResult->ErrorIO.source, currentEntry->pvItem->getErrorAsString());
						}
					}
				}
			}

			// Store the first value as a string.
			if ((filter & out_filter::firstValueAsString) && FirstStringValue && *FirstStringValue && !cachedStringValues.empty()) {
				setLVString((**FirstStringValue)->elt[idx], cachedStringValues[0]);
			}
			// Store the first value as a number.
			if ((filter & out_filter::firstValueAsNumber) && FirstDoubleValue && *FirstDoubleValue && !cachedNumericValues.empty()) {
				(**FirstDoubleValue)->elt[idx] = cachedNumericValues[0];
			}

			// Store the value array as numbers (LabVIEW 2D arrays are row-major: offset = row * cols + col).
			if ((filter & out_filter::valueArrayAsNumber) && DoubleValueArray && *DoubleValueArray) {
				const uInt32 rows = (**DoubleValueArray)->dimSizes[0]; // expected: = nameCount
				const uInt32 cols = (**DoubleValueArray)->dimSizes[1]; // expected: = maxNumberOfValues
				double* destination = (**DoubleValueArray)->elt;

				if (!cachedNumericValues.empty() && rows == nameCount && cols == maxNumberOfValues) {
					const uInt32 valuesSize = static_cast<uInt32>(cachedNumericValues.size());
					const uInt32 copyCount = std::min(valuesSize, cols);

					const uInt32 base = idx * cols; // row-major: write contiguous segment per row
					for (uInt32 j = 0; j < copyCount; ++j) {
						destination[base + j] = cachedNumericValues[j];
					}
					// Clear remaining columns in this row to avoid stale data
					for (uInt32 j = copyCount; j < cols; ++j) {
						destination[base + j] = 0.0;
					}
				}
			}
			// Populate the ResultArray.
			if (filter & out_filter::pviAll && ResultArray && *ResultArray) {
				sResult* currentResult = &(**ResultArray)->result[idx];
				// Status and Severity as numbers.
				if (filter & out_filter::pviStatusAsNumber) currentResult->StatusNumber = pvItem->getStatus();
				if (filter & out_filter::pviSeverityAsNumber) currentResult->SeverityNumber = pvItem->getSeverity();
				if (filter & out_filter::pviTimestampAsNumber) currentResult->TimeStampNumber = pvItem->getTimestamp();

				// Status and Severity as strings.
				if (filter & out_filter::pviStatusAsString) setLVString(currentResult->StatusString, pvItem->getStatusAsString());
				if (filter & out_filter::pviSeverityAsString) setLVString(currentResult->SeverityString, pvItem->getSeverityAsString());
				if (filter & out_filter::pviTimestampAsString) setLVString(currentResult->TimeStampString, pvItem->getTimestampAsString());

				// Error cluster (status/code/source).
				if (filter & out_filter::pviError) {
					const int err = pvItem->getErrorCode();
					const uInt32 lvCode = (err <= (int)ECA_NORMAL) ? 0u : static_cast<uInt32>(err) + ERROR_OFFSET;
					if (currentResult->ErrorIO.code == lvCode && currentResult->ErrorIO.source && *currentResult->ErrorIO.source && (*currentResult->ErrorIO.source)->cnt) {
						// No change; skip updating the string to avoid unnecessary memory operations.
					}
					else {
						currentResult->ErrorIO.code = lvCode;
						currentResult->ErrorIO.status = 0;
						setLVString(currentResult->ErrorIO.source, pvItem->getErrorAsString());
					}
				}

				// Values as strings and numbers.
				if (filter & out_filter::pviValuesAsString && currentResult->StringValueArray && !cachedStringValues.empty()) {
					const std::vector<std::string>& values = cachedStringValues;
					const uInt32 valuesSize = static_cast<uInt32>(values.size());
					const uInt32 copyCount = std::min(valuesSize, maxNumberOfValues);
					sStringArray* lvArray = (*currentResult->StringValueArray);
					for (uInt32 j = 0; j < copyCount; ++j) {
						LStrHandle& stringHandle = lvArray->elt[j];
						const std::string& valueString = values[j];
						// Determine effective length up to the first NUL character.
						size_t effectiveLen = valueString.find('\0');
						if (effectiveLen == std::string::npos) effectiveLen = valueString.size();
						// Skip work if unchanged.
						if (stringHandle && LStrLen(*stringHandle) == (int32)effectiveLen && (effectiveLen == 0 || memcmp(LStrBuf(*stringHandle), valueString.data(), effectiveLen) == 0)) {
							continue;
						}
						setLVString(stringHandle, valueString);
					}
				}
				if ((filter & out_filter::pviValuesAsNumber) && currentResult->ValueNumberArray && !cachedNumericValues.empty()) {
					const std::vector<double>& values = cachedNumericValues;
					const uInt32 valuesSize = static_cast<uInt32>(values.size());
					const uInt32 copyCount = std::min(valuesSize, maxNumberOfValues);
					double* destination = (*currentResult->ValueNumberArray)->elt;
					if (copyCount > 0) {
						memcpy(destination, values.data(), static_cast<size_t>(copyCount) * sizeof(double));
					}
					// Clear remaining elements to avoid stale data.
					for (uInt32 j = copyCount; j < maxNumberOfValues; ++j) {
						destination[j] = 0.0;
					}
					if (filter & out_filter::pviElements) currentResult->valueArraySize = copyCount;
				}
				else {
					if (filter & out_filter::pviElements) currentResult->valueArraySize = pvItem->getNumberOfValues();
				}
				if ((filter & (out_filter::pviFieldNames | out_filter::pviFieldValues)) && pvItem->parent == nullptr) {
					const auto& fields = pvItem->getFields();
					const uInt32 fieldCount = (uInt32)fields.size();

					if ((filter & out_filter::pviFieldNames) && fieldCount > 0) {
						if (!currentResult->FieldNameArray || !*currentResult->FieldNameArray ||
							(*currentResult->FieldNameArray)->dimSize != fieldCount) {
							if (currentResult->FieldNameArray)
								DeleteStringArray(currentResult->FieldNameArray);
							const size_t totalSize =
								sizeof(sStringArray) +                                    // header + 1 element
								(static_cast<size_t>(fieldCount) - 1) * sizeof(LStrHandle); // remaining elements
							currentResult->FieldNameArray =	reinterpret_cast<sStringArrayHdl>(DSNewHClr(static_cast<uInt32>(totalSize)));
							if (currentResult->FieldNameArray)
								(*currentResult->FieldNameArray)->dimSize = fieldCount;
						}
					}
					if ((filter & out_filter::pviFieldValues) && fieldCount > 0) {
						if (!currentResult->FieldValueArray || !*currentResult->FieldValueArray ||
							(*currentResult->FieldValueArray)->dimSize != fieldCount) {
							if (currentResult->FieldValueArray)
								DeleteStringArray(currentResult->FieldValueArray);
							const size_t totalSize =
								sizeof(sStringArray) +                                    // header + 1 element
								(static_cast<size_t>(fieldCount) - 1) * sizeof(LStrHandle); // remaining elements
							currentResult->FieldValueArray = reinterpret_cast<sStringArrayHdl>(DSNewHClr(static_cast<uInt32>(totalSize)));
							if (currentResult->FieldValueArray)
								(*currentResult->FieldValueArray)->dimSize = fieldCount;
						}
					}

					for (uInt32 f = 0; f < fieldCount; ++f) {
						const std::string& fieldName = fields[f].first;
						if ((filter & out_filter::pviFieldNames) && currentResult->FieldNameArray && *currentResult->FieldNameArray) {
							setLVString((*currentResult->FieldNameArray)->elt[f], fieldName);
						}
						if ((filter & out_filter::pviFieldValues) && currentResult->FieldValueArray && *currentResult->FieldValueArray) {
							std::string valueStr;
							if (!pvItem->tryGetFieldString_callerLocked(fieldName, valueStr)) {
								valueStr.clear();
							}
							setLVString((*currentResult->FieldValueArray)->elt[f], valueStr);
						}
					}

					if (!(filter & out_filter::pviFieldNames) && currentResult->FieldNameArray) {
						DeleteStringArray(currentResult->FieldNameArray);
						currentResult->FieldNameArray = nullptr;
					}
					if (!(filter & out_filter::pviFieldValues) && currentResult->FieldValueArray) {
						DeleteStringArray(currentResult->FieldValueArray);
						currentResult->FieldValueArray = nullptr;
					}

					if (fields.empty()) {
						if (currentResult->FieldNameArray) {
							DeleteStringArray(currentResult->FieldNameArray);
							currentResult->FieldNameArray = nullptr;
						}
						if (currentResult->FieldValueArray) {
							DeleteStringArray(currentResult->FieldValueArray);
							currentResult->FieldValueArray = nullptr;
						}
					}
				}
				else {
					if (currentResult->FieldNameArray) {
						DeleteStringArray(currentResult->FieldNameArray);
						currentResult->FieldNameArray = nullptr;
					}
					if (currentResult->FieldValueArray) {
						DeleteStringArray(currentResult->FieldValueArray);
						currentResult->FieldValueArray = nullptr;
					}
				}
			}
		}
		// Done with this entry
	}
}

// =================================================================================
// Channel Access Callbacks
// =================================================================================

void connectionChanged(connection_handler_args args) {
	Globals& g = Globals::getInstance();
	if (g.stopped.load()) return;

	std::string pvName(ca_name(args.chid));
	PVItem* pvItem = static_cast<PVItem*>(ca_puser(args.chid));
	if (!pvItem) {
		{
			std::shared_lock<std::shared_timed_mutex> rlock(g.pvRegistryLock);
			auto it = g.pvRegistry.find(pvName);
			if (it == g.pvRegistry.end()) {
				// PV was likely cleared during unreserved - this is expected, not an error
				return;
			}
			pvItem = it->second.get();
		}
	}
	short dbrType = ca_field_type(args.chid);
	uInt32 nElems = ca_element_count(args.chid);

	{
		std::lock_guard<std::mutex> lock(pvItem->ioMutex());
		bool isConnecting = (args.op == CA_OP_CONN_UP);
		bool wasConnected = pvItem->isConnected();
		pvItem->setConnected(isConnecting);
		if (!wasConnected && isConnecting) {
			pvItem->setErrorCode(ECA_NORMAL);
			pvItem->setDbrType(dbrType);
			pvItem->setNumberOfValues(nElems);
		}
		if (args.op == CA_OP_CONN_DOWN) {
			pvItem->setSeverity(epicsSevInvalid);
			pvItem->setStatus(epicsAlarmComm);
			pvItem->setErrorCode(ECA_DISCONNCHID);
			pvItem->updateChangeHash();
		}
	}
	g.removePendingConnection(pvItem);
	g.notify();

	// Notify subscribers about the connection state change.
	postEventForPv(pvName);
}

void valueChanged(struct event_handler_args args) {
	Globals& g = Globals::getInstance();
	if (g.stopped.load()) return;

	// Collect minimal information and enqueue the work for background processing.
	const int type = args.type;
	const unsigned nElems = args.count;
	const int statusCode = args.status;
	const char* name = ca_name(args.chid);
	std::string pvName(name);
	ValueChangeTask task;
	task.pvName = std::move(pvName);
	task.type = type;
	task.nElems = nElems;
	task.errorCode = statusCode;

	// Only copy data if the status is OK and the pointer is valid.
	if (statusCode == ECA_NORMAL && args.dbr) {
		size_t elemSize = 0;
		switch (type) {
		case DBR_CHAR:    case DBR_TIME_CHAR:    elemSize = sizeof(dbr_char_t);   break;
		case DBR_SHORT:   case DBR_TIME_SHORT:   elemSize = sizeof(dbr_short_t);  break;
		case DBR_LONG:    case DBR_TIME_LONG:    elemSize = sizeof(dbr_long_t);   break;
		case DBR_FLOAT:   case DBR_TIME_FLOAT:   elemSize = sizeof(dbr_float_t);  break;
		case DBR_DOUBLE:  case DBR_TIME_DOUBLE:  elemSize = sizeof(dbr_double_t); break;
		case DBR_ENUM:    case DBR_TIME_ENUM:    elemSize = sizeof(dbr_enum_t);   break;
		case DBR_STRING:  case DBR_TIME_STRING:  elemSize = MAX_STRING_SIZE;      break;
		default:
			CaLabDbgPrintf("valueChanged: Unknown DBR-type %d", type);
			return;
		}

		const size_t dataBytes = elemSize * nElems;
		void* copy = malloc(dataBytes);
		if (!copy) {
			CaLabDbgPrintf("valueChanged: malloc(%u) failed", (unsigned)dataBytes);
			return;
		}
		memcpy(copy, dbr_value_ptr(args.dbr, args.type), dataBytes);
		task.dataBytes = dataBytes;
		task.dataCopy = copy;
	}
	else if (statusCode != ECA_NORMAL) {
		// Set the error on the PV immediately and log it.
		if (args.usr) {
			PVItem* pvItem = static_cast<PVItem*>(args.usr);
			{
				std::lock_guard<std::mutex> lk(pvItem->ioMutex());
				pvItem->setErrorCode(statusCode);
			}
			CaLabDbgPrintf("valueChanged error for %s: %s", task.pvName.c_str(), pvItem->getErrorAsString().c_str());
		}
		else {
			CaLabDbgPrintf("valueChanged error for %s: status=%d", task.pvName.c_str(), statusCode);
		}
	}

	if (type >= DBR_TIME_STRING && type <= DBR_TIME_DOUBLE && statusCode == ECA_NORMAL && args.dbr) {
		const struct dbr_time_double* timePtr =
			reinterpret_cast<const struct dbr_time_double*>(args.dbr);
		task.hasTimeMeta = true;
		task.secPastEpoch = timePtr->stamp.secPastEpoch;
		task.nsec = timePtr->stamp.nsec;
		task.status = timePtr->status;
		task.severity = timePtr->severity;
	}

	enqueueTask(std::move(task));

	// If this is an ENUM PV and its enum labels have not been fetched yet, request them now.
	if (args.usr && args.status == ECA_NORMAL) {
		PVItem* pvItem = static_cast<PVItem*>(args.usr);
		// Use the field type from the channel for robustness.
		short fieldType = ca_field_type(args.chid);
		if ((pvItem->getDbrType() == DBF_ENUM || fieldType == DBF_ENUM)) {
			// Only request labels if we don't have them yet and no request is in-flight.
			const auto& labels = pvItem->getEnumStrings();
			if (labels.empty() && pvItem->tryMarkEnumFetchRequested()) {
				g.addPendingConnection(pvItem);
				int rc = ca_array_get_callback(DBR_CTRL_ENUM, 1, args.chid, enumInfoChanged, pvItem);
				if (rc != ECA_NORMAL) {
					pvItem->setErrorCode(rc);
					pvItem->clearEnumFetchRequested();
					g.removePendingConnection(pvItem);
					CaLabDbgPrintf("Info: enum metadata request for %s deferred: %s", pvItem->getName().c_str(), ca_message_safe(rc));
				}
			}
		}
	}
}

void enumInfoChanged(struct event_handler_args args) {
	PVItem* pvItem = static_cast<PVItem*>(args.usr);
	if (pvItem) {
		dbr_ctrl_enum* enumValue = (dbr_ctrl_enum*)args.dbr;
		if (enumValue) {
			pvItem->setEnumValue(static_cast<const dbr_ctrl_enum*>(args.dbr));
		}
		// Enum labels have arrived; allow future refreshes.
		pvItem->clearEnumFetchRequested();
		Globals::getInstance().removePendingConnection(pvItem);
		// Wake any threads that may be waiting for enum metadata.
		Globals::getInstance().notify();
	}
}


// =================================================================================
// Memory Management Helpers
// =================================================================================

MgErr DeleteStringArray(sStringArrayHdl array) {
	MgErr err = noErr;
	if (!array) return err;
	err = DSCheckHandle(array);
	if (err != noErr || !*array)
		return err;
	for (uInt32 i = 0; i < (*array)->dimSize; i++) {
		if ((*array)->elt[i] && DSCheckHandle((*array)->elt[i]) == noErr) {
			err += DSDisposeHandle((*array)->elt[i]);
		}
		(*array)->elt[i] = nullptr;
	}
	if (array && DSCheckHandle(array) == noErr) {
		err += DSDisposeHandle(array);
	}
	array = nullptr;
	return err;
}

MgErr CleanupResult(sResult* currentResult) {
	MgErr err = noErr;
	if (currentResult->PVName && DSCheckHandle(currentResult->PVName) == noErr)
		err += DSDisposeHandle(currentResult->PVName);
	currentResult->PVName = nullptr;
	if (currentResult->StringValueArray && DSCheckHandle(currentResult->StringValueArray) == noErr)
		err += DeleteStringArray(currentResult->StringValueArray);
	currentResult->StringValueArray = nullptr;
	if (currentResult->ValueNumberArray && DSCheckHandle(currentResult->ValueNumberArray) == noErr)
		err += DSDisposeHandle(currentResult->ValueNumberArray);
	currentResult->ValueNumberArray = nullptr;
	if (currentResult->StatusString && DSCheckHandle(currentResult->StatusString) == noErr)
		err += DSDisposeHandle(currentResult->StatusString);
	currentResult->StatusString = nullptr;
	currentResult->StatusNumber = 0;
	if (currentResult->SeverityString && DSCheckHandle(currentResult->SeverityString) == noErr)
		err += DSDisposeHandle(currentResult->SeverityString);
	currentResult->SeverityString = nullptr;
	currentResult->SeverityNumber = 0;
	if (currentResult->TimeStampString && DSCheckHandle(currentResult->TimeStampString) == noErr)
		err += DSDisposeHandle(currentResult->TimeStampString);
	currentResult->TimeStampString = nullptr;
	currentResult->TimeStampNumber = 0;
	if (currentResult->FieldNameArray && DSCheckHandle(currentResult->FieldNameArray) == noErr)
		err += DeleteStringArray(currentResult->FieldNameArray);
	currentResult->FieldNameArray = nullptr;
	if (currentResult->FieldValueArray && DSCheckHandle(currentResult->FieldValueArray) == noErr)
		err += DeleteStringArray(currentResult->FieldValueArray);
	currentResult->FieldValueArray = nullptr;
	if (currentResult->ErrorIO.source && DSCheckHandle(currentResult->ErrorIO.source) == noErr)
		err += DSDisposeHandle(currentResult->ErrorIO.source);
	currentResult->ErrorIO.source = nullptr;
	currentResult->ErrorIO.code = 0;
	currentResult->ErrorIO.status = 0;
	currentResult->valueArraySize = 0;
	currentResult = nullptr;
	return err;
}

void cleanupMemory(sResultArrayHdl* ResultArray, sStringArrayHdl* FirstStringValue, sDoubleArrayHdl* FirstDoubleValue, sDoubleArray2DHdl* DoubleValueArray, sStringArrayHdl* PvNameArray, const std::vector<uInt32>& changedIndices = {})
{
	if (!ResultArray || !PvNameArray || !*PvNameArray || !(**PvNameArray)) {
		return;
	}

	MgErr err = noErr;
	bool needsCleanup = false;

	// Check if the dimensions match. If not, a full cleanup is required.
	if (*ResultArray) {
		if ((**ResultArray)->dimSize && ((**ResultArray)->dimSize != (**PvNameArray)->dimSize)) {
			needsCleanup = true;
		}
	}

	if (needsCleanup) {
		// If a full cleanup is needed (due to array resize) or if no specific indices were provided.
		if (changedIndices.empty()) {
			// Perform a full cleanup.
			if (*ResultArray) {
				// Clean up all Result elements.
				for (uInt32 i = 0; i < (**ResultArray)->dimSize; i++) {
					sResult* currentResult = &(**ResultArray)->result[i];
					err += CleanupResult(currentResult);
				}
				// Free the ResultArray itself.
				if (*ResultArray && DSCheckHandle(*ResultArray) == noErr)
					err += DSDisposeHandle(*ResultArray);
				*ResultArray = nullptr;
			}
			// Free the FirstStringValue array.
			if (*FirstStringValue) {
				for (uInt32 j = 0; j < (**FirstStringValue)->dimSize; j++) {
					if ((**FirstStringValue)->elt[j] && DSCheckHandle((**FirstStringValue)->elt[j]) == noErr)
						err += DSDisposeHandle((**FirstStringValue)->elt[j]);
					(**FirstStringValue)->elt[j] = nullptr;
				}
				if(*FirstStringValue && DSCheckHandle(*FirstStringValue) == noErr)
					err += DSDisposeHandle(*FirstStringValue);
				*FirstStringValue = nullptr;
			}

			// Free the FirstDoubleValue array.
			if (*FirstDoubleValue && DSCheckHandle(*FirstDoubleValue) == noErr)
				err += DSDisposeHandle(*FirstDoubleValue);
			*FirstDoubleValue = nullptr;

			// Free the DoubleValueArray.
			if (*DoubleValueArray && DSCheckHandle(*DoubleValueArray) == noErr)
				err += DSDisposeHandle(*DoubleValueArray);
			*DoubleValueArray = nullptr;
		}
		else {
			// Selectively clean only the changed elements.
			// Note: We cannot free individual elements of the top-level array,
			// but we can clean the contents of the Result struct.
			if (*ResultArray) {
				for (uInt32 idx : changedIndices) {
					if (idx < (**ResultArray)->dimSize) {
						sResult* currentResult = &(**ResultArray)->result[idx];
						err += CleanupResult(currentResult);
					}
				}
			}
		}

		// Log errors if any occurred during cleanup.
		if (err != noErr) {
			CaLabDbgPrintf("cleanupMemory: Error when releasing handles, error code: %d", err);
		}
	}
}

MgErr prepareOutputArrays(uInt32 nameCount, uInt32 maxNumberOfValues, int filter, sResultArrayHdl* ResultArray, sStringArrayHdl* FirstStringValue, sDoubleArrayHdl* FirstDoubleValue, sDoubleArray2DHdl* DoubleValueArray, sStringArrayHdl* PvNameArray, const std::vector<uInt32>& changedIndices = {})
{
	MgErr err = noErr;
	bool completeInitNeeded = changedIndices.empty();

	// 1. Prepare FirstStringValue array.
	if (filter & out_filter::firstValueAsString && FirstStringValue) {
		if (!*FirstStringValue || !**FirstStringValue || (**FirstStringValue)->dimSize != nameCount) {
			err = NumericArrayResize(uQ, 1, (UHandle*)FirstStringValue, nameCount);
			if (err != noErr) return err;
			(**FirstStringValue)->dimSize = nameCount;
		}
	}

	// 2. Prepare FirstDoubleValue array.
	if (filter & out_filter::firstValueAsNumber && FirstDoubleValue) {
		if (!*FirstDoubleValue || !**FirstDoubleValue || (**FirstDoubleValue)->dimSize != nameCount) {
			err = NumericArrayResize(fD, 1, (UHandle*)FirstDoubleValue, nameCount);
			if (err != noErr) return err;
			(**FirstDoubleValue)->dimSize = nameCount;
		}
	}

	// 3. Prepare DoubleValueArray (2D).
	if (filter & out_filter::valueArrayAsNumber && DoubleValueArray) {
		if (!*DoubleValueArray || !**DoubleValueArray ||
			(**DoubleValueArray)->dimSizes[0] != nameCount ||
			(**DoubleValueArray)->dimSizes[1] != maxNumberOfValues) {

			err = NumericArrayResize(fD, 2, (UHandle*)DoubleValueArray, nameCount * maxNumberOfValues);
			if (err != noErr) return err;
			(**DoubleValueArray)->dimSizes[0] = nameCount;
			(**DoubleValueArray)->dimSizes[1] = maxNumberOfValues;
		}
	}

	// 4. Prepare ResultArray and its nested arrays.
	//bool useResultArray = (filter & (out_filter::pviAll & ~out_filter::pviFieldValues)) != 0;
	bool useResultArray = (filter & out_filter::pviAll) != 0;
	if (useResultArray && ResultArray) {
		if (!*ResultArray || !**ResultArray || (**ResultArray)->dimSize != nameCount) {
			// Recreate ResultArray if it doesn't exist or has the wrong size.
			if (*ResultArray) {
				for (uInt32 i = 0; i < (**ResultArray)->dimSize; i++) {
					sResult* currentResult = &(**ResultArray)->result[i];
					err += CleanupResult(currentResult);
				}
				if (*ResultArray && DSCheckHandle(*ResultArray) == noErr)
					err += DSDisposeHandle(*ResultArray);
				*ResultArray = nullptr;
			}
			// LabVIEW array handles use a 4-byte (size_t) dimSize header.
			if (nameCount > 0) {
				const size_t totalSize =
					sizeof(sResultArray) +
					(static_cast<size_t>(nameCount) - 1) * sizeof(sResult);
				*ResultArray = reinterpret_cast<sResultArrayHdl>(DSNewHClr(static_cast<uInt32>(totalSize)));
			}
			if (!(*ResultArray)) {
				CaLabDbgPrintf("Error: Memory allocation failed for ResultArray in prepareOutputArrays.");
				return mFullErr;
			}
			(**ResultArray)->dimSize = nameCount;
			completeInitNeeded = true;  // A full initialization is needed after recreation.
		}

		// Determine which indices to initialize.
		const std::vector<uInt32> indicesToInit = completeInitNeeded ?
			makeIndexRange(nameCount) : changedIndices;

		// Initialize only the relevant elements.
		for (uInt32 idx : indicesToInit) {
			if (idx >= nameCount) continue;

			sResult* currentResult = &(**ResultArray)->result[idx];

			// Copy or update the PVName.
			LStrHandle pvLStr = (**PvNameArray)->elt[idx];
			if (!currentResult->PVName || LStrLen(*currentResult->PVName) != LStrLen(*pvLStr) ||
				memcmp(LStrBuf(*currentResult->PVName), LStrBuf(*pvLStr), LStrLen(*pvLStr)) != 0) {

				err = NumericArrayResize(uB, 1, (UHandle*)&currentResult->PVName, (*pvLStr)->cnt);
				if (err != noErr) return err;
				memcpy(LStrBuf(*currentResult->PVName), LStrBuf(*pvLStr), LStrLen(*pvLStr));
				LStrLen(*currentResult->PVName) = LStrLen(*pvLStr);
			}

			// Prepare the string array within the Result cluster.
			if (filter & out_filter::pviValuesAsString && maxNumberOfValues > 0) {
				if (!currentResult->StringValueArray || !*currentResult->StringValueArray ||
					(*currentResult->StringValueArray)->dimSize != maxNumberOfValues) {

					if (currentResult->StringValueArray) DeleteStringArray(currentResult->StringValueArray);
					const size_t totalSize =
						sizeof(sStringArray) +
						(static_cast<size_t>(maxNumberOfValues) - 1) * sizeof(LStrHandle);
					currentResult->StringValueArray = reinterpret_cast<sStringArrayHdl>(DSNewHClr(static_cast<uInt32>(totalSize)));
					if (currentResult->StringValueArray) (*currentResult->StringValueArray)->dimSize = maxNumberOfValues;
				}
			}

			// Prepare the numeric array within the Result cluster.
			if (filter & out_filter::pviValuesAsNumber) {
				if (!currentResult->ValueNumberArray || !*currentResult->ValueNumberArray ||
					(*currentResult->ValueNumberArray)->dimSize != maxNumberOfValues) {

					err = NumericArrayResize(fD, 1, (UHandle*)&currentResult->ValueNumberArray, maxNumberOfValues);
					if (err != noErr) return err;
					if (currentResult && *currentResult->ValueNumberArray) {
						(*currentResult->ValueNumberArray)->dimSize = maxNumberOfValues;
					}
				}
			}
		}
	}
	return noErr;
}


// =================================================================================
// Utility & Debugging Functions
// =================================================================================

static void buildTimestampedFormat(const char* format, std::string& out) {
	char timestamp[32] = {};
	const std::time_t now = std::time(nullptr);
	std::tm tm_snapshot{};
#if defined _WIN32 || defined _WIN64
	localtime_s(&tm_snapshot, &now);
#else
	localtime_r(&now, &tm_snapshot);
#endif
	if (std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &tm_snapshot) == 0) {
		std::snprintf(timestamp, sizeof(timestamp), "0000-00-00 00:00:00");
	}
	out.clear();
	out.reserve((format ? std::strlen(format) : 0) + std::strlen(timestamp) + 4);
	out.append("[");
	out.append(timestamp);
	out.append("] ");
	if (format) {
		out.append(format);
	}
}

MgErr CaLabDbgPrintf(const char* format, ...) {
	int done = 0;
#if defined _WIN32 || defined _WIN64
	if (!dllStatus) return done; // DLL_PROCESS_DETACH
#endif
	if (!format) return done;
	std::string formatWithTimestamp;
	buildTimestampedFormat(format, formatWithTimestamp);
	va_list listPointer;
	va_start(listPointer, format);
	if (Globals::getInstance().pCaLabDbgFile) {
		done = vfprintf(Globals::getInstance().pCaLabDbgFile, formatWithTimestamp.c_str(), listPointer);
		fprintf(Globals::getInstance().pCaLabDbgFile, "\n");
		fflush(Globals::getInstance().pCaLabDbgFile);
	}
	else {
		done = DbgPrintfv(formatWithTimestamp.c_str(), listPointer);
	}
	va_end(listPointer);
	return done;
}

MgErr CaLabDbgPrintfD([[maybe_unused]] const char* format, ...) {
	int done = 0;
#ifdef _DEBUG
	if (!format) return done;
	std::string formatWithTimestamp;
	buildTimestampedFormat(format, formatWithTimestamp);
	va_list listPointer;
	va_start(listPointer, format);
	if (Globals::getInstance().pCaLabDbgFile) {
		done = vfprintf(Globals::getInstance().pCaLabDbgFile, formatWithTimestamp.c_str(), listPointer);
		fprintf(Globals::getInstance().pCaLabDbgFile, "\n");
		fflush(Globals::getInstance().pCaLabDbgFile);
	}
	else {
		done = DbgPrintfv(formatWithTimestamp.c_str(), listPointer);
	}
	va_end(listPointer);
#endif
	return done;
}

void setLVString(LStrHandle& handle, const std::string& text) {
	MgErr err = noErr;
	size_t len = text.size();
	if (!handle) {
		handle = (LStrHandle)DSNewHandle(static_cast<uInt32>(sizeof(int32) + len));
		if (!handle) return;
	}
	else {
		err = DSSetHandleSize(handle, static_cast<uInt32>(sizeof(int32) + len));
		if (err != noErr) return;
	}
	(*handle)->cnt = static_cast<int32>(len);
	if (len)
		memcpy((*handle)->str, text.data(), len);
}

void fillResultFromPv(PVItem* pvItem, sResult* target) {
	if (!pvItem || !target) return;

	setLVString(target->PVName, pvItem->getName());
	auto strValues = pvItem->dbrValue2String();
	auto numValues = pvItem->dbrValue2Double();

	const uInt32 valueCount = static_cast<uInt32>(numValues.size());
	target->valueArraySize = valueCount;

	if (!strValues.empty()) {
		if (!target->StringValueArray || !*target->StringValueArray || (*target->StringValueArray)->dimSize != valueCount) {
			if (target->StringValueArray) DeleteStringArray(target->StringValueArray);
			if (valueCount > 0) {
				const size_t totalSize =
					sizeof(sStringArray) +
					(valueCount - 1) * sizeof(LStrHandle);
				target->StringValueArray = 	reinterpret_cast<sStringArrayHdl>(DSNewHClr(static_cast<uInt32>(totalSize)));
				if (target->StringValueArray)
					(**target->StringValueArray).dimSize = valueCount;
			}
		}
		if (target->StringValueArray && *target->StringValueArray) {
			for (uInt32 i = 0; i < valueCount; ++i) {
				setLVString((*target->StringValueArray)->elt[i], strValues[i]);
			}
		}
	}

	if (!numValues.empty()) {
		if (!target->ValueNumberArray || !*target->ValueNumberArray || (*target->ValueNumberArray)->dimSize != valueCount) {
			NumericArrayResize(fD, 1, (UHandle*)&target->ValueNumberArray, valueCount);
			if (target->ValueNumberArray) (*target->ValueNumberArray)->dimSize = valueCount;
		}
		if (target->ValueNumberArray && *target->ValueNumberArray) {
			memcpy((*target->ValueNumberArray)->elt, numValues.data(), valueCount * sizeof(double));
		}
	}

	// Status/severity/timestamp
	setLVString(target->StatusString, pvItem->getStatusAsString());
	setLVString(target->SeverityString, pvItem->getSeverityAsString());
	setLVString(target->TimeStampString, pvItem->getTimestampAsString());
	target->StatusNumber = pvItem->getStatus();
	target->SeverityNumber = pvItem->getSeverity();
	target->TimeStampNumber = pvItem->getTimestamp();

	// Fields: names and values from parent's cache.
	const auto& fields = pvItem->getFields();
	if (!fields.empty()) {
		const uInt32 n = static_cast<uInt32>(fields.size());
		if (!target->FieldNameArray || !*target->FieldNameArray || (*target->FieldNameArray)->dimSize != n) {
			if (target->FieldNameArray) DeleteStringArray(target->FieldNameArray);
			if (n > 0) {
				const size_t totalSize =
					sizeof(sStringArray) +                       // dimSize + 1 Element
					(static_cast<size_t>(n) - 1) * sizeof(LStrHandle); // restliche Elemente
				target->FieldNameArray = reinterpret_cast<sStringArrayHdl>(DSNewHClr(static_cast<uInt32>(totalSize)));
				if (target->FieldNameArray)
					(**target->FieldNameArray).dimSize = n;
			}

		}
		if (!target->FieldValueArray || !*target->FieldValueArray || (*target->FieldValueArray)->dimSize != n) {
			if (target->FieldValueArray) DeleteStringArray(target->FieldValueArray);
			if(n > 0) {
				const size_t totalSize =
					sizeof(sStringArray) +                       // dimSize + 1 Element
					(static_cast<size_t>(n) - 1) * sizeof(LStrHandle); // restliche Elemente
				target->FieldValueArray = reinterpret_cast<sStringArrayHdl>(DSNewHClr(static_cast<uInt32>(totalSize)));
				if (target->FieldValueArray)
					(**target->FieldValueArray).dimSize = n;
			}
		}
		for (uInt32 i = 0; i < n; ++i) {
			const std::string& fieldName = fields[i].first;
			if (!target->FieldNameArray || !*target->FieldNameArray || !target->FieldValueArray || !*target->FieldValueArray) break;
			setLVString((*target->FieldNameArray)->elt[i], fieldName);
			std::string fieldValue;
			if (pvItem->tryGetFieldString_callerLocked(fieldName, fieldValue)) {
				setLVString((*target->FieldValueArray)->elt[i], fieldValue);
			}
			else {
				setLVString((*target->FieldValueArray)->elt[i], std::string());
			}
		}
	}

	// ErrorIO from PVItem
	const int err = pvItem->getErrorCode();
	const uInt32 lvCode = (err <= (int)ECA_NORMAL) ? 0u : static_cast<uInt32>(err) + ERROR_OFFSET;
	if (target->ErrorIO.code == lvCode && target->ErrorIO.source && *target->ErrorIO.source && (*target->ErrorIO.source)->cnt) {
		// No change; skip updating the string to avoid unnecessary memory operations.
	}
	else {
		target->ErrorIO.code = lvCode;
		target->ErrorIO.status = 0;
		setLVString(target->ErrorIO.source, pvItem->getErrorAsString());
	}
}

/**
 * @brief Collects environment and version information.
 * @return A vector of key-value pairs representing the information.
 */
std::vector<std::pair<std::string, std::string>> collectEnvironmentInfo() {
	std::vector<std::pair<std::string, std::string>> info;
	char buffer[255];

	// Version Info
#if IsOpSystem64Bit
#if defined(DEBUG) || defined(_DEBUG)
	epicsSnprintf(buffer, sizeof(buffer), "%s DEBUG 64 bit", CALAB_VERSION);
#else
	epicsSnprintf(buffer, sizeof(buffer), "%s 64 bit", CALAB_VERSION);
#endif
#else
#if defined(DEBUG) || defined(_DEBUG)
	epicsSnprintf(buffer, sizeof(buffer), "%s DEBUG 32 bit", CALAB_VERSION);
#else
	epicsSnprintf(buffer, sizeof(buffer), "%s 32 bit", CALAB_VERSION);
#endif
#endif
	info.push_back({ "CA LAB VERSION", buffer });
	info.push_back({ "COMPILED FOR EPICS BASE", EPICS_VERSION_STRING });
	info.push_back({ "CA PROTOCOL VERSION", ca_version() });

	// EPICS Environment Variables
	const ENV_PARAM* const* ppParam = env_param_list;
	while (*ppParam != NULL) {
		const char* pVal = envGetConfigParamPtr(*ppParam);
		info.push_back({ (*ppParam)->name, pVal ? pVal : "undefined" });
		ppParam++;
	}

	// CALab Environment Variables
	const char* calabPolling = getenv("CALAB_POLLING");
	info.push_back({ "CALAB_POLLING", calabPolling ? calabPolling : "undefined" });
	const char* calabNoDbg = getenv("CALAB_NODBG");
	info.push_back({ "CALAB_NODBG", calabNoDbg ? calabNoDbg : "undefined" });

	return info;
}

// =================================================================================
// Info Array Population Helper
// =================================================================================

MgErr populateInfoStringArray(sStringArray2DHdl* InfoStringArray2D, const std::vector<std::pair<std::string, std::string>>& info) {
	if (!InfoStringArray2D) return noErr;

	const uInt32 rows = static_cast<uInt32>(info.size());
	const uInt32 cols = 2u; // key + value

	// If array exists and dimensions differ, dispose existing element handles and the array
	if (*InfoStringArray2D && **InfoStringArray2D) {
		const uInt32 oldRows = (**InfoStringArray2D)->dimSizes[0];
		const uInt32 oldCols = (**InfoStringArray2D)->dimSizes[1];
		if (oldRows != rows || oldCols != cols) {
			// Free contained string handles first to avoid leaks
			const uInt32 total = oldRows * oldCols;
			for (uInt32 i = 0; i < total; ++i) {
				if ((**InfoStringArray2D)->elt[i] && DSCheckHandle(((**InfoStringArray2D)->elt[i])) == noErr) 
					DSDisposeHandle((**InfoStringArray2D)->elt[i]);
				(**InfoStringArray2D)->elt[i] = nullptr;				
			}
			if (*InfoStringArray2D && DSCheckHandle(*InfoStringArray2D) == noErr)
				DSDisposeHandle(*InfoStringArray2D);
			*InfoStringArray2D = nullptr;
		}
	}

	// Allocate array if missing
	if (!*InfoStringArray2D || **InfoStringArray2D == nullptr) {
		const uInt32 total = rows * cols;
		if (total > 0) {
			const size_t totalSize =
				sizeof(sStringArray2D) +
				(static_cast<size_t>(total) - 1) * sizeof(LStrHandle);

			*InfoStringArray2D =
				reinterpret_cast<sStringArray2DHdl>(
					DSNewHClr(static_cast<uInt32>(totalSize)));

			if (**InfoStringArray2D) {
				(**InfoStringArray2D)->dimSizes[0] = rows;
				(**InfoStringArray2D)->dimSizes[1] = cols;
			}
		}
		else {
			*InfoStringArray2D = nullptr;
		}
	}
	else {
		// Ensure dims are set (in case they matched and were kept)
		(**InfoStringArray2D)->dimSizes[0] = rows;
		(**InfoStringArray2D)->dimSizes[1] = cols;
		// Clear existing element handles to avoid stale data when shrinking
		const uInt32 total = rows * cols;
		for (uInt32 i = 0; i < total; ++i) {
			if ((**InfoStringArray2D)->elt[i] && DSCheckHandle(((**InfoStringArray2D)->elt[i])) == noErr) 
				DSDisposeHandle((**InfoStringArray2D)->elt[i]);
			(**InfoStringArray2D)->elt[i] = nullptr;			
		}
	}

	if (rows == 0) return noErr;

	// Fill content: row-major layout [row * dimSizes[1] + col]
	for (uInt32 r = 0; r < rows; ++r) {
		const auto& kv = info[r];
		const uInt32 base = r * cols;
		if (**InfoStringArray2D != nullptr) {
			setLVString((**InfoStringArray2D)->elt[base + 0], kv.first);
			setLVString((**InfoStringArray2D)->elt[base + 1], kv.second);
		}
	}

	return noErr;
}

MgErr populateAllResultArray(sResultArrayHdl* ResultArray) {
	Globals& g = Globals::getInstance();
	// Snapshot PV list under shared lock to minimize contention
	std::vector<PVItem*> items;
	{
		TimeoutSharedLock<std::shared_timed_mutex> lock(g.pvRegistryLock, "populateAllResultArray-scan", std::chrono::milliseconds(500));
		if (!lock.isLocked()) return mgArgErr;
		items.reserve(g.pvRegistry.size());
		for (const auto& kv : g.pvRegistry) {
			// Include only base PVs, not fields
			if (kv.second) {
				PVItem* pv = kv.second.get();
				std::string recordType = pv->getRecordType();
				if (recordType.empty()) continue;
				items.push_back(pv);
			}
		}
	}

	// Sort items alphanumerically by PV name
	std::sort(items.begin(), items.end(), [](PVItem* a, PVItem* b) {
		return a->getName() < b->getName();
		});

	const uInt32 n = static_cast<uInt32>(items.size());
	if (!ResultArray) return noErr;

	// If size changes, free existing per-element handles before realloc
	if (*ResultArray && **ResultArray && (**ResultArray)->dimSize != n) {
		for (uInt32 i = 0; i < (**ResultArray)->dimSize; ++i) {
			sResult* r = &(**ResultArray)->result[i];
			CleanupResult(r);
		}
		if (*ResultArray && DSCheckHandle(*ResultArray) == noErr)
			DSDisposeHandle(*ResultArray);
		*ResultArray = nullptr;
	}

	// Allocate if missing
	if (!*ResultArray || !**ResultArray) {
		// LabVIEW handle: header (size_t dimSize) + n elements
		if (n == 0) {
			*ResultArray = nullptr;
			return noErr;
		}
		const size_t totalSize =
			sizeof(sResultArray) +
			(static_cast<size_t>(n) - 1) * sizeof(sResult);
		*ResultArray = reinterpret_cast<sResultArrayHdl>(DSNewHClr(static_cast<uInt32>(totalSize)));
		if (!*ResultArray)
			return mFullErr;
		(**ResultArray)->dimSize = n;
	}

	// Fill rows
	for (uInt32 i = 0; i < n; ++i) {
		sResult* dst = &(**ResultArray)->result[i];
		PVItem* pv = items[i];
		if (!pv) continue;
		{
			std::lock_guard<std::mutex> lk(pv->ioMutex());
			fillResultFromPv(pv, dst);
		}
	}

	return noErr;
}
