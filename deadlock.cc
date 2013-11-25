#include <stdio.h>
#include <iostream.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sync.h>
#include <sys/siginfo.h>
#include <sys/neutrino.h>
#include <sys/netmgr.h>
#include <sys/syspage.h>

#include "PulseTimer.h"
#include "PiMutex.h"
#include "PcMutex.h"
//=============================================================================

pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond=PTHREAD_COND_INITIALIZER;

#define threadCount 10 /* Maximum number of threads*/

#define RELEASE_TIME_P1 2
#define RELEASE_TIME_P2 0

#define PRIORITY_P1	0.7
#define PRIORITY_P2	0.5

float priority[threadCount] = {0};	// priority of threads
int active_p = 0;					// detemine the active thread that should be run

bool deadlock = false;

void ThreadManager();

//-----------------------------------------------------------------------------------------
// Instantiates "Priority Inheritance" and "Priority Ceiling" mutexes.
//-----------------------------------------------------------------------------------------
PiMutex piMutex1;
PiMutex piMutex2;

/*
 * Priority ceiling must be higher than the highest priority
 * of all tasks that can access the resource.
 * In our case: max(PRIORITY_P1, PRIORITY_P2) + 0.1.
 */
PcMutex pcMutex1(0.8);
PcMutex pcMutex2(0.8);

//-----------------------------------------------------------------------------------------
// Thread 1 (highest priority)
//-----------------------------------------------------------------------------------------
void * P1(void* arg)
{
	int cnt = 1;
	int lockStatus = -1;
	while (1)
	{
		printf("\nP1: lock CPU mutex");
		pthread_mutex_lock(&mutex);

		// wait for the message from ThreadManager and check if current thread is active
		printf("\nP1: suspended, priority: %f", priority[1]);
		while (active_p != 1)
			pthread_cond_wait(&cond, &mutex);

		printf("\nP1: resumed, executing, cnt: %d", cnt);
		active_p = 0;

		if (cnt == 1)
		{
			// Try to acquire CS1 mutex after running for 1 unit
			printf("\nP1: try to lock CS1");
//			lockStatus = piMutex1.lock(&priority[1]);		// use PI mutex
			lockStatus = pcMutex1.lock(&priority[1]);		// use PC mutex
		}
		else if (cnt == 2)
		{
			printf("\nP1: try to lock CS2");
//			lockStatus = piMutex2.lock(&priority[1]);		// use PI mutex
			lockStatus = pcMutex2.lock(&priority[1]);		// use PC mutex
		}
		else if (cnt == 3)
		{
			printf("\nP1: try to unlock CS2");
//			lockStatus = piMutex2.unlock(&priority[1]);		// use PI mutex
			lockStatus = pcMutex2.unlock();		// use PC mutex
		}
		else if (cnt == 4)
		{
			printf("\nP1: try to unlock CS1");
//			lockStatus = piMutex1.unlock(&priority[1]);		// use PI mutex
			lockStatus = pcMutex1.unlock();		// use PC mutex
		}
		else if (cnt == 6)
		{
			printf("\nP1: thread execution completed");

			// remove 1st process from the ThreadManager's queue
			priority[1] = 0;

			printf("\nP1: unlock CPU mutex");
			pthread_mutex_unlock(&mutex);
			break;
		}
		printf("\nP1: executed, cnt: %d", cnt);

		// verify if deadlock occurred and update deadlock status accordingly
		if (lockStatus == -100)
			deadlock = true;

		printf("\nP1: unlock CPU mutex");
		pthread_mutex_unlock(&mutex);
		cnt++;
	}

	return NULL;
}

//-----------------------------------------------------------------------------------------
// Thread 2 (medium priority)
//-----------------------------------------------------------------------------------------
void * P2(void* arg)
{
	int cnt = 1;
	int lockStatus = -1;
	while (1)
	{
		printf("\nP2: lock CPU mutex");
		pthread_mutex_lock(&mutex);

		// wait for the message from ThreadManager and check if current thread is active
		printf("\nP2: suspended, priority: %f", priority[2]);
		while (active_p != 2)
			pthread_cond_wait(&cond, &mutex);

		printf("\nP2: resumed, executing, cnt: %d", cnt);
		active_p = 0;

		if (cnt == 2)
		{
			printf("\nP2: try to lock CS2");
//			lockStatus = piMutex2.lock(&priority[2]);		// use PI mutex
			lockStatus = pcMutex2.lock(&priority[2]);		// use PC mutex
		}
		else if (cnt == 3)
		{
			printf("\nP2: try to lock CS1");
//			lockStatus = piMutex1.lock(&priority[2]);		// use PI mutex
			lockStatus = pcMutex1.lock(&priority[2]);		// use PC mutex
		}
		else if (cnt == 4)
		{
			printf("\nP2: try to unlock CS1");
//			lockStatus = piMutex1.unlock(&priority[2]);		// use PI mutex
			lockStatus = pcMutex1.unlock();		// use PC mutex
		}
		else if (cnt == 5)
		{
			printf("\nP2: try to unlock CS2");
//			lockStatus = piMutex2.unlock(&priority[2]);		// use PI mutex
			lockStatus = pcMutex2.unlock();		// use PC mutex
		}
		else if (cnt == 6)
		{
			printf("\nP2: thread execution completed");

			// remove process from the ThreadManager's queue
			priority[2] = 0;

			printf("\nP2: unlock CPU mutex");
			pthread_mutex_unlock(&mutex);
			break;
		}
		printf("\nP2: executed, cnt: %d", cnt);

		// verify if deadlock occurred and update deadlock status accordingly
		if (lockStatus == -100)
			deadlock = true;

		printf("\nP2: unlock CPU mutex");
		pthread_mutex_unlock(&mutex);
		cnt++;
	}

	return NULL;
}


//-----------------------------------------------------------------------------------------
// ThreadManager - determines which thread should run based on its current priority.
//-----------------------------------------------------------------------------------------
void threadManager()
{
	// find thread with the highest priority and flag it as active
	float p =- 1;
	for (int i = 1; i < threadCount; i++)
	{
		if (priority[i] > p)
		{
			active_p = i;
			p = priority[i];
		}
	}
	printf("\nThread manager: activate thread %d", active_p);

	printf("\nThread manager: notify threads");
	pthread_cond_broadcast(&cond);
}

//-----------------------------------------------------------------------------------------
// Main function
//-----------------------------------------------------------------------------------------
int main(void)
{
	// initialize threads
	pthread_t P1_ID, P2_ID;

	// create and start periodic timer to generate pulses every second.
	PulseTimer* timer = new PulseTimer(1);
	timer->start();

	int cnt = 0;
	while(1)
	{
		// deadlock detector logic
		if (deadlock)
		{
			printf("\nScheduler: deadlock occurred, stop execution");
			break;
		}

		printf("\nScheduler: lock CPU mutex");
		pthread_mutex_lock(&mutex);

		// release P2 at t=0
		if (cnt == RELEASE_TIME_P2)
		{
			priority[2] = PRIORITY_P2;	// 0.6
			printf("\nP2 released");
			pthread_create(&P2_ID, NULL, P2, NULL);
		}
		 // release P1 at t=2
		else if (cnt == RELEASE_TIME_P1)
		{
			priority[1] = PRIORITY_P1;	// 0.7
			printf("\nP1 released");
			pthread_create(&P1_ID, NULL, P1, NULL);
		}
		 // terminate the program at t = 30
		if (cnt == 30)
		{
			printf("\n\n30 seconds are over, terminate program");
			break;
		}

		threadManager();

		printf("\nScheduler: unlock CPU mutex");
		pthread_mutex_unlock(&mutex);

		// wait for the timer pulse to fire
		timer->wait();
		printf("\n\n timer tick: %d\n", cnt);

		cnt++;
	}

	// stop and destroy the timer
	timer->stop();
	delete timer;
}
//=============================================================================

