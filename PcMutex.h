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
		PcMutex(float priority)
		{
			printf("Initializing pcMutex ...\n");
			pthread_mutex_init(&pcMutex, NULL);
			history = new list<ThreadInfo>;

			// Sets critical section (resource) priority to that of highest priority thread
			// among those using the resource. Should be determined in advance with static analysis.
			csPriority = priority;
		}

		//-----------------------------------------------------------------------------------------
		// Destructor
		//-----------------------------------------------------------------------------------------
		virtual ~PcMutex()
		{
			printf("Destroying pcMutex ...\n");
			int status = pthread_mutex_destroy(&pcMutex);
			if (status != 0)
				printf("Error destroying pcMutex\n");
			delete history;
		}

		//-----------------------------------------------------------------------------------------
		// Locks pcMutex for critical section.
		// Hoist task priority to that of critical section (determined in advance by static analysis)
		// Keeps references (history) to suspended tasks, as manager doesn't provide this functionality.
		// Returns 0 (success) or error code (failure).
		//-----------------------------------------------------------------------------------------
		int lock(float *priorityPtr)
		{
			int lockStatus = pthread_mutex_trylock(&pcMutex);

			// if locked successfully
			if (lockStatus == 0)
			{
				printf("\nPcMutex: locking CS");

				printf("\nPcMutex: saving thread's state");
				ThreadInfo threadData;
				threadData.threadPtr = priorityPtr;
				threadData.nativePriority = *priorityPtr;
				history->push_front(threadData);

				printf("\nPcMutex: inheriting CS's priority: %f", csPriority);
				*priorityPtr = csPriority;
			}
			// if was already locked
			else if (lockStatus == 16)
			{
				printf("\nPcMutex: CS already locked");

				printf("\nPcMutex: saving thread's state");
				ThreadInfo threadData;
				threadData.threadPtr = priorityPtr;
				threadData.nativePriority = *priorityPtr;
				history->push_front(threadData);

				printf("\nPcMutex: suspend thread");
				*priorityPtr = 0;	// set its priority to 0
			}
			else
			{
				printf("\nPcMutex: deadlock occured");
				return -100;
			}

			return lockStatus;
		}

		//-----------------------------------------------------------------------------------------
		// Unlocks pcMutex and restores thread priorities to their original values.
		// This also resumes suspended priorities (with priority set to 0).
		//-----------------------------------------------------------------------------------------
		int unlock(float *currentThreadPriority)
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
		// Sets critical section (resource) priority.
		// CS priority should be known/determined before the mutex object is created.
		//-----------------------------------------------------------------------------------------
//		int setCsPriority(float maxPriority)
//		{
//			int unlockStatus = pthread_mutex_unlock(&pcMutex);
//
//			if (unlockStatus == 0)
//			{
//				printf("\nPcMutex: unlocked, recovering priorities, resuming suspended threads");
//				while(!history->empty())
//				{
//					// recover native priorities (also resumes suspended threads)
//					*(history->front().threadPtr) = history->front().nativePriority;
//					history->pop_front();
//
//					// reset CS priority
//					csPriority = 0;
//				}
//			}
//
//			return unlockStatus;
//		}

	//-----------------------------------------------------------------------------------------
	// Private members
	//-----------------------------------------------------------------------------------------
	private:
		pthread_mutex_t pcMutex;
		list<ThreadInfo> *history;
		float csPriority;
};

#endif
