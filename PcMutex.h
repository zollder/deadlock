#include <pthread.h>
#include <list>

#ifndef PcMutex_h
#define PcMutex_h

//-----------------------------------------------------------------------------------------
// PcMutex (Priority Ceiling Mutex) class definition and implementation.
// Works as a wrapper around standard pthread_mutex functions.
// Implements Highest Locker’s Priority Protocol (HLP).
//-----------------------------------------------------------------------------------------
class PcMutex
{
	//-----------------------------------------------------------------------------------------
	// Thread priority data holder
	//-----------------------------------------------------------------------------------------
	struct ThreadInfo
	{
		int threadId;
		float *threadPtr;
		float nativePriority;
	};

	//-----------------------------------------------------------------------------------------
	// Public members
	//-----------------------------------------------------------------------------------------
	public:
		//-----------------------------------------------------------------------------------------
		// Constructor (initializes pcMutex)
		//-----------------------------------------------------------------------------------------
		PcMutex()
		{
			printf("Initializing pcMutex ...\n");
			pthread_mutex_init(&pcMutex, NULL);
			history = new list<ThreadInfo>;

			// Sets critical section (resource) priority to that of highest priority thread
			// among those using the resource. Should be determined in advance with static analysis.
			csPriority = 0;

			// initilize lock status
			locked = false;
		}

		//-----------------------------------------------------------------------------------------
		// Destructor
		//-----------------------------------------------------------------------------------------
		virtual ~PcMutex()
		{
			printf("Destroying pcMutex ...\n");
			int status = pthread_mutex_destroy(&pcMutex);
			if (status != 0)
				printf("Error destroying pcMutex");
			delete history;
		}

		//-----------------------------------------------------------------------------------------
		// Locks pcMutex for critical section.
		// Hoist task priority to that of critical section (determined in advance by static analysis)
		// Keeps references (history) to suspended tasks, as manager doesn't provide this functionality.
		// Returns 0 (success) or error code (failure).
		//-----------------------------------------------------------------------------------------
		int lock(int threadId, float priorities[], PcMutex pcMutexes[], int size)
		{
			int lockStatus = -1;

			bool lockExists = false;
			bool self = false;
			float maxCeiling = 0;
			PcMutex *lockedMutex;
			int csPosition = 0;

			// verify if there are locked mutexes
			for (int i = 0; i < size; i++)
			{
				if (pcMutexes[i].isLocked())
				{
					lockExists = true;
					lockedMutex = &pcMutexes[i];
					csPosition = i;
				}
				if (pcMutexes[i].getCsOwner() == threadId)
					self = true;
				if (pcMutexes[i].getCsPriority() > maxCeiling)
					maxCeiling = pcMutexes[i].getCsPriority();
			}

			// one of the external mutexes locked?
			if (lockExists)
			{
				// locking thread is the same as the current thread (self)?
				if (self && !isLocked())
				{
					// lock, update status, save info about locking thread
					printf("\nPcMutex: locking CS, thread %d", threadId);
					lockStatus = pthread_mutex_lock(&pcMutex);
					locked = true;

					printf("\nPcMutex: saving state, thread %d", threadId);
					ThreadInfo threadData;
					threadData.threadId = threadId;
					threadData.threadPtr = &priorities[threadId];
					threadData.nativePriority = priorities[threadId];
					saveHistory(threadData);
				}
				// thread priority > ceiling and mutex unlocked?
				else if (priorities[threadId] > maxCeiling && !isLocked())
				{
					// lock, update status, save info about locking thread
					printf("\nPcMutex: locking CS, thread %d", threadId);
					lockStatus = pthread_mutex_lock(&pcMutex);
					locked = true;

					printf("\nPcMutex: saving state, thread %d", threadId);
					ThreadInfo threadData;
					threadData.threadId = threadId;
					threadData.threadPtr = &priorities[threadId];
					threadData.nativePriority = priorities[threadId];
					saveHistory(threadData);
				}
				// thread priority > ceiling and mutex locked?
				else if (priorities[threadId] > maxCeiling && isLocked())
				{
					// save state, suspend thread
					printf("\nPcMutex: CS already locked, suspend thread %d", threadId);

					printf("\nPcMutex: saving state, thread %d", threadId);
					ThreadInfo threadData;
					threadData.threadId = threadId;
					threadData.threadPtr = &priorities[threadId];
					threadData.nativePriority = priorities[threadId];
					saveHistory(threadData);

					// suspend by setting its priority to 0
					printf("\nPcMutex: suspend thread %d", threadId);
					priorities[threadId] = 0;
				}
				// ceiling > locking thread priority > locked thread priority?
				else if (priorities[threadId] > priorities[lockedMutex->getCsOwner()])
				{
					// save thread info in the target mutex
					printf("\nPcMutex: saving state on target CS, thread %d", threadId);
					ThreadInfo threadData;
					threadData.threadId = threadId;
					threadData.threadPtr = &priorities[threadId];
					threadData.nativePriority = priorities[threadId];
					lockedMutex->saveHistory(threadData);

					// inherit priority
					printf("\nPcMutex: inheriting CS's priority: %f", priorities[threadId]);
					int ownerThreadId = lockedMutex->getCsOwner();
					priorities[ownerThreadId] = priorities[threadId];

					// suspend thread
					printf("\nPcMutex: suspend thread %d", threadId);
					priorities[threadId] = 0;
				}
				// suspend thread otherwise
				else
				{
					printf("\nPcMutex: saving state on target CS, thread %d", threadId);
					ThreadInfo threadData;
					threadData.threadId = threadId;
					threadData.threadPtr = &priorities[threadId];
					threadData.nativePriority = priorities[threadId];
					lockedMutex->saveHistory(threadData);

					printf("\nPcMutex: suspend thread %d", threadId);
					priorities[threadId] = 0;
				}
			}
			// all critical sections unlocked
			else
			{
				// lock CS, update status, save info about locking thread
				printf("\nPcMutex: locking CS, thread %d", threadId);
				lockStatus = pthread_mutex_lock(&pcMutex);
				locked = true;

				printf("\nPcMutex: saving state, thread %d", threadId);
				ThreadInfo threadData;
				threadData.threadId = threadId;
				threadData.threadPtr = &priorities[threadId];
				threadData.nativePriority = priorities[threadId];
				saveHistory(threadData);
			}

			return lockStatus;
		}

		//-----------------------------------------------------------------------------------------
		// Unlocks pcMutex and restores thread priorities to their original values.
		// This also resumes suspended priorities (with priority set to 0).
		//-----------------------------------------------------------------------------------------
		int unlock()
		{
			int unlockStatus = pthread_mutex_unlock(&pcMutex);

			if (unlockStatus == 0)
			{
				printf("\nPcMutex: unlocking CS, recovering priorities, resuming suspended threads");
				while(!history->empty())
				{
					// recover native priorities (also resumes suspended threads)
					*(history->front().threadPtr) = history->front().nativePriority;
					history->pop_front();
				}
			}

			return unlockStatus;
		}

		//-----------------------------------------------------------------------------------------
		// Saves history.
		//-----------------------------------------------------------------------------------------
		void saveHistory(ThreadInfo threadData)
		{
			history->push_front(threadData);
		}

		//-----------------------------------------------------------------------------------------
		// Sets CS priority.
		//-----------------------------------------------------------------------------------------
		void setCsPriority(float priority)
		{
			csPriority = priority;
		}

		//-----------------------------------------------------------------------------------------
		// Returns CS priority.
		//-----------------------------------------------------------------------------------------
		float getCsPriority()
		{
			return csPriority;
		}

		//-----------------------------------------------------------------------------------------
		// Returns lock status of the mutex.
		//-----------------------------------------------------------------------------------------
		bool isLocked()
		{
			return locked;
		}

		//-----------------------------------------------------------------------------------------
		// Returns lock status of the mutex.
		//-----------------------------------------------------------------------------------------
		int getCsOwner()
		{
			return history->back().threadId;
		}


	//-----------------------------------------------------------------------------------------
	// Private members
	//-----------------------------------------------------------------------------------------
	private:
		pthread_mutex_t pcMutex;
		list<ThreadInfo> *history;
		float csPriority;
		bool locked;
};

#endif
