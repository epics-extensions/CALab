#pragma once
#include <shared_mutex>
#include <mutex>
#include <chrono>
#include <thread>

// Provides a RAII wrapper for a shared lock on a mutex with a timeout.
// This class attempts to acquire a shared lock and releases it upon destruction.
// It is useful for situations where lock contention is expected and waiting indefinitely is not desirable.
template<typename MutexType>
class TimeoutSharedLock {
private:
	MutexType& mutex_;
	bool locked_ = false;
	const char* functionName_;
	const char* previousOwner_;
	std::chrono::time_point<std::chrono::high_resolution_clock> lockTime_;
	std::chrono::milliseconds timeout_;

public:
	// Constructs a TimeoutSharedLock and attempts to acquire a shared lock on the given mutex.
	// @param mutex The mutex to lock.
	// @param functionName An optional name of the function acquiring the lock, for debugging.
	// @param timeout The maximum time to wait for the lock.
	TimeoutSharedLock(MutexType& mutex, const char* functionName = "", std::chrono::milliseconds timeout = std::chrono::milliseconds(10000))
		: mutex_(mutex), functionName_(functionName), timeout_(timeout) {
		previousOwner_ = LockTracker::getMutexOwner(&mutex_);
		// Use try_lock_shared_for for mutex types that support timed shared locking.
		if constexpr (std::is_same_v<MutexType, std::shared_timed_mutex>) {
			locked_ = mutex_.try_lock_shared_for(timeout);
		}
		else {
			// Fallback for other mutex types that support non-timed shared locking.
			locked_ = mutex_.try_lock_shared();
		}
		if (locked_) {
			lockTime_ = std::chrono::high_resolution_clock::now();
			LockTracker::updateMutexOwner(&mutex_, functionName_);
			LockTracker::setLastLock(&mutex_, functionName_);
		}
		else {
			CaLabDbgPrintf("Warning: Shared lock timeout occurred in function: %s. Previous owner: %s",
				functionName_, previousOwner_);
		}
	}

	// Destructor that releases the shared lock if it was acquired.
	~TimeoutSharedLock() {
		if (locked_) {
			mutex_.unlock_shared();
		}
	}

	// Returns true if the lock was successfully acquired.
	bool isLocked() const { return locked_; }
	// Returns the name of the function that created the lock.
	const char* getFunctionName() const { return functionName_; }
	// Returns the name of the previous lock owner for debugging purposes.
	const char* getPreviousOwner() const { return previousOwner_; }
};

// Provides a RAII wrapper for a unique lock on a mutex with a timeout.
// This class attempts to acquire a unique lock and releases it upon destruction.
// It supports different types of mutexes, including a fallback for std::mutex.
template<typename MutexType>
class TimeoutUniqueLock {
private:
	MutexType& mutex_;
	bool locked_ = false;
	const char* functionName_;
	const char* previousOwner_;
	std::chrono::time_point<std::chrono::high_resolution_clock> lockTime_;

public:
	// Constructs a TimeoutUniqueLock and attempts to acquire a unique lock on the given mutex.
	// @param mutex The mutex to lock.
	// @param functionName An optional name of the function acquiring the lock, for debugging.
	// @param timeout The maximum time to wait for the lock.
	TimeoutUniqueLock(MutexType& mutex, const char* functionName = "", std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
		: mutex_(mutex), functionName_(functionName) {
		previousOwner_ = LockTracker::getMutexOwner(&mutex_);
		// Use try_lock_for for mutex types that support timed locking.
		if constexpr (std::is_same_v<MutexType, std::shared_timed_mutex>) {
			locked_ = mutex_.try_lock_for(timeout);
		}
		else if constexpr (std::is_same_v<MutexType, std::timed_mutex> || std::is_same_v<MutexType, std::recursive_timed_mutex>) {
			locked_ = mutex_.try_lock_for(timeout);
		}
		else {
			// For std::mutex, which doesn't support timed locking, emulate it with a busy-wait loop.
			// This loop repeatedly tries to lock the mutex with a short sleep in between, until the timeout is reached.
			const auto deadline = std::chrono::high_resolution_clock::now() + timeout;
			for (;;) {
				if (mutex_.try_lock()) { locked_ = true; break; }
				if (std::chrono::high_resolution_clock::now() >= deadline) break;
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		}
		if (locked_) {
			lockTime_ = std::chrono::high_resolution_clock::now();
			LockTracker::updateMutexOwner(&mutex_, functionName_);
			LockTracker::setLastLock(&mutex_, functionName_);
		}
		else {
			CaLabDbgPrintf(
				"Error: Unique lock timeout occurred after %lld ms in function: %s. Previous owner: %s",
				static_cast<long long>(timeout.count()),
				functionName_, previousOwner_);
		}
	}

	// Destructor that releases the unique lock if it was acquired.
	// It also logs a warning if the lock was held for an unusually long time.
	~TimeoutUniqueLock() {
		if (locked_) {
			auto unlockTime = std::chrono::high_resolution_clock::now();
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(unlockTime - lockTime_);
			// Log a warning if the lock was held for more than 60 seconds.
			if (duration > std::chrono::milliseconds(60000)) {
				CaLabDbgPrintf(
					"Warning: TimeoutUniqueLock in function %s held for %lld ms.",
					functionName_,
					static_cast<long long>(duration.count()));
			}
			mutex_.unlock();
		}
	}

	// Returns true if the lock was successfully acquired.
	bool isLocked() const { return locked_; }
	// Returns the name of the function that created the lock.
	const char* getFunctionName() const { return functionName_; }
	// Returns the name of the previous lock owner for debugging purposes.
	const char* getPreviousOwner() const { return previousOwner_; }
};