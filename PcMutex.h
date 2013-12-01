#include <pthread.h>
#include <list>

#ifndef PcMutex_h
#define PcMutex_h

//-----------------------------------------------------------------------------------------
// PcMutex (Priority Ceiling Mutex) class definition and implementation.
// Works as a wrapper around standard pthread_mutex functions.
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

			//Should be determined in advance with static analysis.
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
		// Locks pcMutex for critical section based on priority ceiling protocol.
		// Keeps references (history) to suspended tasks, as manager doesn't provide this functionality.
		// Returns 0 (success) or error code (failure).
		//-----------------------------------------------------------------------------------------
		int lock(int threadId, float priorities[], PcMutex pcMutexes[], int size)
		{
			int lockStatus = -1;

			bool lockExists = false;
			bool self = false;
			PcMutex *lockedMutex;

			// determine and initialize prerequisites
			for (int i = 0; i < size; i++)
			{
				if (pcMutexes[i].isLocked())
				{
					lockExists = true;
					lockedMutex = &pcMutexes[i];
					if (pcMutexes[i].getCsOwner() == threadId)
						self = true;
					else
						self = false;
				}
			}

			// if all mutexes are unlocked
			if (!lockExists)
			{
				// lock CS, update status, save info about locking thread
				printf("\nPcMutex: locking CS%d, thread %d", getId(), threadId);
				lockStatus = pthread_mutex_trylock(&pcMutex);
				if (lockStatus != 0)
					printf("\nPcMutex: ERROR LOCKING MUTEX id: %d", getId());
				else
					locked = true;

				printf("\nPcMutex: saving thread %d state", threadId);
				saveState(createDataObj(priorities, threadId));
			}
			// at least one mutex is locked by another or same thread
			else if (self || priorities[threadId] > getCsPriority())
			{
				// if locking thread priority > ceiling OR another CS is locked by the same thread
				// AND current CS is not locked => lock CS
				if(!isLocked())
				{
					// lock CS, update status, save info about locking thread
					printf("\nPcMutex: locking CS%d, thread %d", getId(), threadId);
					lockStatus = pthread_mutex_trylock(&pcMutex);
					if (lockStatus != 0)
						printf("\nPcMutex: ERROR LOCKING MUTEX id: %d", getId());
					else
						locked = true;

					printf("\nPcMutex: saving thread %d state", threadId);
					saveState(createDataObj(priorities, threadId));
				}
				// suspend locking thread by setting its status to 0
				else
				{
					printf("\nPcMutex: CS%d already locked by thread %d", getId(), lockedMutex->getCsOwner());
					printf("\nPcMutex: saving thread %d state", threadId);
					saveState(createDataObj(priorities, threadId));

					printf("\nPcMutex: suspend thread %d", threadId);
					priorities[threadId] = 0;
				}
			}
			// if (locking priority) > (locked priority), transfer priority (inheritance) and suspend locking thread
			else
			{
				// identify target (locked) mutex owner (thread)
				int lockedThreadId = lockedMutex->getCsOwner();
				if ( priorities[threadId] > priorities[lockedThreadId])
				{
					// save donor thread info at locked/target mutex
					printf("\nPcMutex: saving thread %d state on target CS%d", lockedThreadId, lockedMutex->getId());
					lockedMutex->saveState(createDataObj(priorities, threadId));

					// transfer priority to thread that is locking target mutex
					printf("\nPcMutex: transferring priority %.2f to thread %d", priorities[threadId], lockedThreadId);
					priorities[lockedThreadId] = priorities[threadId];
				}

				// suspend locking thread by setting its priority to 0
				printf("\nPcMutex: suspend thread %d", threadId);
				priorities[threadId] = 0;
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
				printf("\nPcMutex: unlocking CS%d, recovering priorities, resuming suspended threads", getId());
				while(!history->empty())
				{
					// recover native priorities (also resumes suspended threads)
					printf("\nPcMutex: recovering thread %d priority to %.2f",
							history->front().threadId,
							history->front().nativePriority);
					*(history->front().threadPtr) = history->front().nativePriority;
					history->pop_front();
				}

				printf("\nPcMutex: resetting CS locked status");
				locked = false;
			}
			else
				printf("\nPcMutex: ERROR UNLOCKING MUTEX");

			return unlockStatus;
		}

		//-----------------------------------------------------------------------------------------
		// Creates ThreadInfo data holder. Returns created structure.
		//-----------------------------------------------------------------------------------------
		ThreadInfo createDataObj(float priorities[], int threadId)
		{
			printf("\nPcMutex: creating thread %d data holder", threadId);
			ThreadInfo threadData;
			threadData.threadId = threadId;
			threadData.threadPtr = &priorities[threadId];
			threadData.nativePriority = priorities[threadId];

			return threadData;
		}

		//-----------------------------------------------------------------------------------------
		// Saves thread's state.
		//-----------------------------------------------------------------------------------------
		void saveState(ThreadInfo threadData)
		{
			history->push_front(threadData);
		}

		//-----------------------------------------------------------------------------------------
		// Sets critical section priority.
		//-----------------------------------------------------------------------------------------
		void setCsPriority(float priority)
		{
			csPriority = priority;
		}

		//-----------------------------------------------------------------------------------------
		// Returns critical section priority.
		//-----------------------------------------------------------------------------------------
		float getCsPriority()
		{
			return csPriority;
		}

		//-----------------------------------------------------------------------------------------
		// Returns mutex lock status.
		//-----------------------------------------------------------------------------------------
		bool isLocked()
		{
			return locked;
		}

		//-----------------------------------------------------------------------------------------
		// Returns mutex owner (thread) id.
		//-----------------------------------------------------------------------------------------
		int getCsOwner()
		{
			return history->back().threadId;
		}

		//-----------------------------------------------------------------------------------------
		// Sets mutex id.
		//-----------------------------------------------------------------------------------------
		void setId(int id)
		{
			mutexId = id;
		}

		//-----------------------------------------------------------------------------------------
		// Returns mutex id.
		//-----------------------------------------------------------------------------------------
		int getId()
		{
			return mutexId;
		}

	//-----------------------------------------------------------------------------------------
	// Private members
	//-----------------------------------------------------------------------------------------
	private:
		pthread_mutex_t pcMutex;
		list<ThreadInfo> *history;
		float csPriority;
		bool locked;
		int mutexId;
};

#endif
