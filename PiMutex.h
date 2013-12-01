#include <pthread.h>
#include <list>

#ifndef PiMutex_h
#define PiMutex_h

//-----------------------------------------------------------------------------------------
// PiMutex (Priority Inheritance Mutex) class definition and implementation.
// Works as a wrapper around standard pthread_mutex functions.
//-----------------------------------------------------------------------------------------
class PiMutex
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
		// Constructor (initializes piMutex)
		//-----------------------------------------------------------------------------------------
		PiMutex()
		{
			printf("Initializing piMutex ...\n");
			pthread_mutex_init(&piMutex, NULL);
			history = new list<ThreadInfo>;
			csPriority = 0;
		}

		//-----------------------------------------------------------------------------------------
		// Destructor
		//-----------------------------------------------------------------------------------------
		virtual ~PiMutex()
		{
			printf("Destroying piMutex ...\n");
			int status = pthread_mutex_destroy(&piMutex);
			if (status != 0)
				printf("Error destroying piMutex\n");
			delete history;
		}

		//-----------------------------------------------------------------------------------------
		// Locks piMutex for critical section with highest known thread priority.
		// Keeps references to priority donors (history).
		// Returns 0 (success) or error code (failure).
		//-----------------------------------------------------------------------------------------
		int lock(float *priorityPtr)
		{
			int lockStatus = pthread_mutex_trylock(&piMutex);

			// if locked successfully
			if (lockStatus == 0)
			{
				printf("\nPiMutex: locking CS");

				// keep reference to the locking thread's priority
				ThreadInfo threadData;
				threadData.threadPtr = priorityPtr;
				threadData.nativePriority = *priorityPtr;
				history->push_front(threadData);

				if (csPriority > *priorityPtr)
				{
					printf("\nPiMutex: inherit CS priority: %.2f", *priorityPtr);
					*priorityPtr = csPriority;
				}
				else
				{
					printf("\nPiMutex: update CS priority to: %.2f", *priorityPtr);
					csPriority = *priorityPtr;
				}
			}
			// if already locked by lower priority thread
			else if (lockStatus == 16 && csPriority < *priorityPtr)
			{
				printf("\nPiMutex: CS already locked, inherit priority: %.2f", *priorityPtr);

				// update CS and locking thread's priority to that of the attempting thread
				csPriority = *priorityPtr;
				*(history->back().threadPtr) = *priorityPtr;

				// keep reference to the suspended thread's priority
				ThreadInfo threadData;
				threadData.threadPtr = priorityPtr;
				threadData.nativePriority = *priorityPtr;
				history->push_front(threadData);

				printf("\nPiMutex: suspend higher priority thread");
				*priorityPtr = 0;	// set its priority to 0
			}
			else
			{
				printf("\nPiMutex: deadlock occurred");
				return -100;
			}

			return lockStatus;
		}

		//-----------------------------------------------------------------------------------------
		// Unlocks piMutex and restores thread priorities to their original values.
		// This also resumes suspended priorities (with priority set to 0).
		//-----------------------------------------------------------------------------------------
		int unlock()
		{
			int unlockStatus = pthread_mutex_unlock(&piMutex);

			if (unlockStatus == 0)
			{
				printf("\nPiMutex: unlocked, recovering priorities, resuming suspended threads");
				while(!history->empty())
				{
					// recover native priorities (also resumes suspended threads)
					*(history->front().threadPtr) = history->front().nativePriority;
					history->pop_front();

					// reset CS priority
					csPriority = 0;
				}
			}

			return unlockStatus;
		}

	//-----------------------------------------------------------------------------------------
	// Private members
	//-----------------------------------------------------------------------------------------
	private:
		pthread_mutex_t piMutex;
		list<ThreadInfo> *history;
		float csPriority;
};

#endif
