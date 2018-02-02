#include "pulsedThread.h"


/* ******************************* Non-Class Utility Functions Used by Thread. Inlined for speed ***************************************************************
******************************************************************************************************************************************************************

/* ******************* Configure timespecs and timevals for thread timing and to do the waiting for acc levels 1 and 2 **************

******************************************************** used for accLevel 0 *********************************************************
All we do is sleep for entire duration */
inline void configureSleeper (unsigned int microSeconds, struct timespec *Sleeper){
	unsigned long int timeNM; 
	timeNM = microSeconds;
	for (Sleeper->tv_sec =0; timeNM >= 1e06; timeNM -= 1e06, Sleeper->tv_sec +=1);
	Sleeper->tv_nsec = timeNM  * 1e03; // microsecs to nano secs, timespec uses nanseconds, timevals use microseconds
}

/* ************************************************** used for accLevel 1 **********************************************************
We sleep for time period - kSLEEPTURNAROUND, then wake up and spin until timed period end returns false if period is too short for sleeping, else true */
inline bool configureTurnaroundSleeper (unsigned int microSeconds, struct timespec *Sleeper){
    if (microSeconds < kSLEEPTURNAROUND){
        return false;
    }else{
        unsigned long int timeNM; // use long int to avoid overflow
        timeNM = (microSeconds - kSLEEPTURNAROUND) ;
        for (Sleeper->tv_sec =0; timeNM >= 1e06; timeNM -= 1e06, Sleeper->tv_sec +=1);
        Sleeper->tv_nsec = timeNM * 1e03;// microsecs to nano secs
        return true;
    }
}

/* ************************************** Sleeps for calculated time,then wakes and spins **************************/
inline void WAITINLINE1 (bool itSleeps, struct timespec *sleeper, struct timeval* spinEndTime){
	struct timeval currentTime;
	if (itSleeps){
		nanosleep (sleeper, NULL);
	}
	for (gettimeofday (&currentTime, NULL);(timercmp (&currentTime, spinEndTime, <)); gettimeofday (&currentTime, NULL));
}

/* ********************************************** used for accuracy levels 1 and 2 ***************************************
Configure Timers to hold periods for delay and duration, which are  added to spinEndTime for each period */
inline void configureTimer (unsigned int microSeconds, struct timeval *Timer){
    unsigned long int timeNM; // use long int to avoid overflow
    timeNM = microSeconds; // timeval uses microsecs
    for (Timer->tv_sec =0; timeNM >= 1e06; timeNM -= 1e06, Timer->tv_sec +=1);
    Timer->tv_usec = timeNM;
}


/* ******************************************** used for accuracy level 2 *******************************************************
Sleep stuff is configured on the fly, but we can calculate in advance if sleep is entirely ruled out */
inline bool  configureTurnaround (unsigned int microSeconds){
    if (microSeconds < kSLEEPTURNAROUND){
        return false;
    }else{
        return true;
    }
}
 
/* **********************************************calculates sleep times for each pulse ******************************************/
inline void WAITINLINE2 (bool itSleeps, struct timeval* turnaroundTime, struct timeval* spinEndTime){
	struct timeval currentTime;
	struct timeval sleepEndTime;
	struct timespec sleeper;
	if (itSleeps){
		timersub (spinEndTime, turnaroundTime, &sleepEndTime);  // subtract turnaround time from period end time, put in sleepEndTime 
		gettimeofday (&currentTime, NULL);
		if (timercmp (&currentTime, &sleepEndTime, <)){  // if current time is less than sleep end time, calculate a sleep time
			timersub (&sleepEndTime, &currentTime, &sleepEndTime); //subtract current time from sleep end time , so sleep end time is a duration
			TIMEVAL_TO_TIMESPEC(&sleepEndTime, &sleeper); 
			nanosleep (&sleeper, NULL);
		} 
	}
	for (gettimeofday (&currentTime, NULL);(timercmp (&currentTime, spinEndTime, <)); gettimeofday (&currentTime, NULL));
}


/* ************** the thread function needs to be a C-style function, not a class method ********************************************************
****************************************************************************************************************************************************
Last Modified:
2016/12/14 by Jamie Boyd - added endFunc */
extern "C" void* pulsedThreadFunc (void * tData){
	// cast tData to task param stuct pointer
	taskParams *theTask = (taskParams *) tData;
	// timespec structures for sleeping, acc0
	struct timespec delaySleeper;
	struct timespec durSleeper;
	bool durSleeps;
	bool delaySleeps;
	// timeval structures for spinning, acc1 and 2
	struct timeval pulseDelayUsecs;
	struct timeval pulseDurUsecs;
	struct timeval spinEndTime; // we initialize this from current time and then increment with each period
	// timeval structure of kSLEEPTURNAROUND microseconds  for acc2
	struct timeval turnaroundTime;
	// make timers for delay
	switch (theTask ->accLevel){
		case ACC_MODE_SLEEPS:
			configureSleeper (theTask->pulseDelayUsecs, &delaySleeper);
			break;
		case ACC_MODE_SLEEPS_AND_SPINS:
			delaySleeps = configureTurnaroundSleeper (theTask->pulseDelayUsecs, &delaySleeper);
			configureTimer (theTask->pulseDelayUsecs, &pulseDelayUsecs);
			break;
		case ACC_MODE_SLEEPS_AND_OR_SPINS:
			configureTimer (kSLEEPTURNAROUND, &turnaroundTime);
			configureTimer (theTask->pulseDelayUsecs, &pulseDelayUsecs);
			delaySleeps = configureTurnaround (theTask->pulseDelayUsecs);
			break;
	}
	// make timers for duration
	switch (theTask ->accLevel){
		case ACC_MODE_SLEEPS:
			configureSleeper (theTask->pulseDurUsecs, &durSleeper);
			break;
		case ACC_MODE_SLEEPS_AND_SPINS:
			durSleeps = configureTurnaroundSleeper (theTask->pulseDurUsecs, &durSleeper);
			configureTimer (theTask->pulseDurUsecs, &pulseDurUsecs);
			break;
		case ACC_MODE_SLEEPS_AND_OR_SPINS:
			configureTimer (theTask->pulseDurUsecs, &pulseDurUsecs);
			durSleeps = configureTurnaround (theTask->pulseDurUsecs);
			break;
	}
	// timing is imp., so give thread high priority
	struct sched_param param ;
	param.sched_priority = sched_get_priority_max (SCHED_RR) ;
	pthread_setschedparam (pthread_self (), SCHED_RR, &param) ;
	// loop forever, doing task and modding task
	for (;;){
		// get the lock on doTask and wait for a task to be called, or a timing or customMod param to be modded
		pthread_mutex_lock (&theTask->taskMutex);
		while (theTask->doTask==0)
			pthread_cond_wait(&theTask->taskVar, &theTask->taskMutex);
		// look for modifications of the taskVar that reconfigure sleepers
		if (theTask->doTask & kMODANY){
			if (theTask->doTask & kMODDELAY){
#if beVerbose
				printf ("thread received  signal with new delay = %d\n", theTask->pulseDelayUsecs);
#endif
				switch (theTask ->accLevel){
					case ACC_MODE_SLEEPS:
						configureSleeper (theTask->pulseDelayUsecs, &delaySleeper);
						break;
					case ACC_MODE_SLEEPS_AND_SPINS:
						delaySleeps = configureTurnaroundSleeper (theTask->pulseDelayUsecs, &delaySleeper);
						configureTimer (theTask->pulseDelayUsecs, &pulseDelayUsecs);
						break;
					case ACC_MODE_SLEEPS_AND_OR_SPINS:
						configureTimer (theTask->pulseDelayUsecs, &pulseDelayUsecs);
						delaySleeps = configureTurnaround (theTask->pulseDelayUsecs);
						break;
				}
				theTask->doTask -= kMODDELAY;
			}
			if (theTask->doTask & kMODDUR){
#if beVerbose
				printf ("thread received  signal with new duration = %d\n", theTask->pulseDurUsecs);
#endif
				switch (theTask ->accLevel){
					case ACC_MODE_SLEEPS:
						configureSleeper (theTask->pulseDurUsecs, &durSleeper);
						break;
					case ACC_MODE_SLEEPS_AND_SPINS:
						durSleeps = configureTurnaroundSleeper (theTask->pulseDurUsecs, &durSleeper);
						configureTimer (theTask->pulseDurUsecs, &pulseDurUsecs);
						break;
					case ACC_MODE_SLEEPS_AND_OR_SPINS:
						configureTimer (theTask->pulseDurUsecs, &pulseDurUsecs);
						durSleeps = configureTurnaround (theTask->pulseDurUsecs);
						break;
					}
				theTask->doTask -= kMODDUR;
			}
			if (theTask->doTask & kMODCUSTOM){
				theTask->modCustomFunc (theTask->modCustomData,theTask);
				theTask->doTask -= kMODCUSTOM;
			}
			// if after modifying timing and custom mods, we have no task to do, unlock mutex and go to top of loop, waiting on doTask again
			if (theTask->doTask == 0){
				pthread_mutex_unlock (&theTask->taskMutex);
				continue;
			}
		}
		// we are done with modding doTask, so unlock the mutex
		pthread_mutex_unlock (&theTask->taskMutex);
		 // initalize spinEndTime to current time
		if (theTask->accLevel > 0)
			gettimeofday (&spinEndTime, NULL);
		// do the task(s) as per nPulses
		switch (theTask->nPulses){
			case kPULSE:
				if (theTask->pulseDelayUsecs > 0){
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS){
						nanosleep (&delaySleeper, NULL);
					}else{
						timeradd (&spinEndTime, &pulseDelayUsecs, &spinEndTime);
						if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
							WAITINLINE1 (delaySleeps, &delaySleeper, &spinEndTime);
						}else{
							WAITINLINE2 (delaySleeps, &turnaroundTime, &spinEndTime);
						}
					}
				}
				theTask->hiFunc(theTask->taskCustomData);
				if (theTask ->accLevel == 0){
					nanosleep (&durSleeper, NULL);
				}else{
					timeradd (&spinEndTime, &pulseDurUsecs, &spinEndTime);
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
						WAITINLINE1 (durSleeps, &durSleeper, &spinEndTime);
					}else{
						WAITINLINE2 (durSleeps, &turnaroundTime, &spinEndTime);
					}
				}
				theTask->loFunc(theTask->taskCustomData);
				if (theTask->endFunc != nullptr){
					theTask->endFunc (theTask);
				}
			break;
                
		case kINFINITETRAIN: //an infinite train - don't decrement the queue, and check for delay, duration mods without breaking
			while (theTask->doTask > 0){
				if (theTask->doTask & kMODANY){
					pthread_mutex_lock (&theTask->taskMutex);
					if (theTask->doTask & kMODDELAY){
						switch (theTask ->accLevel){
							case ACC_MODE_SLEEPS:
								configureSleeper (theTask->pulseDelayUsecs, &delaySleeper);
								break;
							case ACC_MODE_SLEEPS_AND_SPINS:
								delaySleeps = configureTurnaroundSleeper (theTask->pulseDelayUsecs, &delaySleeper);
								configureTimer (theTask->pulseDelayUsecs, &pulseDelayUsecs);
								break;
							case ACC_MODE_SLEEPS_AND_OR_SPINS:
								configureTimer (theTask->pulseDelayUsecs, &pulseDelayUsecs);
								delaySleeps = configureTurnaround (theTask->pulseDelayUsecs);
								break;
						}
						theTask->doTask -= kMODDELAY;
					}
					if (theTask->doTask & kMODDUR){
						switch (theTask ->accLevel){
							case ACC_MODE_SLEEPS:
								configureSleeper (theTask->pulseDurUsecs, &durSleeper);
								break;
							case ACC_MODE_SLEEPS_AND_SPINS:
								durSleeps = configureTurnaroundSleeper (theTask->pulseDurUsecs, &durSleeper);
								configureTimer (theTask->pulseDurUsecs, &pulseDurUsecs);
								break;
							case ACC_MODE_SLEEPS_AND_OR_SPINS:
								configureTimer (theTask->pulseDurUsecs, &pulseDurUsecs);
								durSleeps = configureTurnaround (theTask->pulseDurUsecs);
								break;
						}
						theTask->doTask -= kMODDUR;
					}
					if (theTask->doTask & kMODCUSTOM){
						theTask->modCustomFunc(theTask->modCustomData,theTask);
						theTask->doTask -= kMODCUSTOM;
					}
					pthread_mutex_unlock (&theTask->taskMutex);
				}
				if (theTask->hiFunc != nullptr){
					theTask->hiFunc(theTask->taskCustomData);
				}
				if (theTask ->accLevel == 0){
					nanosleep (&durSleeper, NULL);
				}else{
					timeradd (&spinEndTime, &pulseDurUsecs, &spinEndTime);
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
						WAITINLINE1 (durSleeps, &durSleeper, &spinEndTime);
					}else{
						WAITINLINE2 (durSleeps, &turnaroundTime, &spinEndTime);
					}
				}
				if (theTask->pulseDelayUsecs > 0){
					if (theTask->loFunc != nullptr){
						theTask->loFunc(theTask->taskCustomData);
					}
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS){
						nanosleep (&delaySleeper, NULL);
					}else{
						timeradd (&spinEndTime, &pulseDelayUsecs, &spinEndTime);
						if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
							WAITINLINE1 (delaySleeps, &delaySleeper, &spinEndTime);
						}else{
							WAITINLINE2 (delaySleeps, &turnaroundTime, &spinEndTime);
						}
					}
				}
				if (theTask->endFunc != nullptr){
					theTask->endFunc (theTask);
				}
			}
			break;
			
		default: // kTRAIN
#if beVerbose
			printf ("A train was called with doTask = %d and nPulses = %d.\n", theTask->doTask, theTask->nPulses);
#endif
			for (unsigned int iTick=0; iTick < theTask->nPulses; iTick++){
				theTask->hiFunc(theTask->taskCustomData);
				if (theTask ->accLevel == ACC_MODE_SLEEPS){
					nanosleep (&durSleeper, NULL);
				}else{
					timeradd (&spinEndTime, &pulseDurUsecs, &spinEndTime);
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
						WAITINLINE1 (durSleeps, &durSleeper, &spinEndTime);
					}else{
						WAITINLINE2 (durSleeps, &turnaroundTime, &spinEndTime);
					}
				}
				if (theTask->pulseDelayUsecs > 0){
					theTask->loFunc(theTask->taskCustomData);
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS){
						nanosleep (&delaySleeper, NULL);
					}else{
						timeradd (&spinEndTime, &pulseDelayUsecs, &spinEndTime);
						if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
							WAITINLINE1 (delaySleeps, &delaySleeper, &spinEndTime);
						}else{
							WAITINLINE2 (delaySleeps, &turnaroundTime, &spinEndTime);
						}
					}
				}
			}
			if (theTask->endFunc != nullptr){
				theTask->endFunc (theTask);
			}
			break;
		}
		
		// dont decrement doTask if task is an infinite train, else decrement it as we have done a task
		if (theTask->nPulses != kINFINITETRAIN){
			pthread_mutex_lock (&theTask->taskMutex);
			theTask->doTask -=1;
			pthread_mutex_unlock (&theTask->taskMutex);
		}
#if beVerbose
		printf ("Finished a task\n");
#endif
    }
    return NULL;
}

/* ********************************************* pulsedThead Class Methods*******************************************************************************
************************************************************************************************************************************************************
Same constructors for all 3 tasks
Last Modified:
2017/11/22 by Jamie Boyd - added nullptr test for init function before running it.
2016/12/06 by Jamie Boyd added loFunc and hiFunc function pointers for flexibility 
2016/12/12 by Jamie Boyd - removed mode as separate paramater, redundant info with nPulses
2016/2/14 by Jamie Boyd - added constructor with specifications not for pulses, but for train (frequency, trainDuration in secs, dutyCycle) */
pulsedThread::pulsedThread (unsigned int gDelay, unsigned int gDur, unsigned int gPulses, void *  initData, 
int (*initFunc)(void *, void * &), void (*gLoFunc)(void *), void (*gHiFunc)(void *), int gAccLevel, int &errCode){
	
	errCode = ticks2Times (gDelay, gDur, gPulses, theTask);
	if (errCode){
#if beVerbose
		printf ("pulsedThread constructor failed.\n"); // tick2times will print error description
#endif
	}else{
#if beVerbose
		printf ("pulsedThread constructor with train Frequency = %.2f, trainDuration = %.2f, trainDutyCycle = %.2f.\n", theTask.trainFrequency, theTask.trainDuration,theTask.trainDutyCycle);
#endif
		theTask.nPulses = gPulses; // 0 = infinite train, 1 = single pulse, >=2  = number of pulses in a train, 
		theTask.pulseDelayUsecs = gDelay ; // delay to pulse, in microseconds
		theTask.pulseDurUsecs = gDur; // pulse length, in microseconds
		theTask.loFunc = gLoFunc;
		theTask.hiFunc =gHiFunc;
		theTask.modCustomData = nullptr; // this pointer is initialised null , as we don't always use it
		delCustomDataFunc = nullptr; //this function pointer is initialised null , as we don't always havea function
		theTask.endFunc = nullptr;
		theTask.accLevel =gAccLevel;
		// start doTask at 0
		theTask.doTask =0;
		// initialize the task with passed in init func
		errCode = 0;
		if (initFunc == nullptr){
#if beVerbose
			printf ("pulsedThread constructor initFunc == nullptr; taskCustomData initialized with copy of initData pointer.\n");
#endif
			theTask.taskCustomData = initData;
		}else{
#if beVerbose
			printf ("pulsedThread constructor is running the provided initFunc\n");
#endif
			errCode =initFunc(initData , theTask.taskCustomData);
		}
		if (errCode){
#if beVerbose
			printf ("pulsedThread initialization callback error: %d\n", errCode);
#endif
		}else{
			// init mutex and condition var
			pthread_mutex_init(&theTask.taskMutex, NULL);
			pthread_cond_init (&theTask.taskVar, NULL);
			// create thread
			pthread_create(&theTask.taskThread, NULL, &pulsedThreadFunc, (void *)&theTask);
		}
	}
}

pulsedThread::pulsedThread (float gFrequency, float gDutyCycle, float gTrainDuration, void *  initData, int (*initFunc)(void *, void * &), void (*gLoFunc)(void *), void (*gHiFunc)(void *), int gAccLevel, int &errCode){

	errCode = times2Ticks (gFrequency, gDutyCycle, gTrainDuration, theTask);
	if (errCode){
#if beVerbose
		printf ("pulsedThread constructor failed.\n"); // time2ticks will print error description
#endif
	}else{
#if beVerbose
		printf ("pulsedThread constructor with Number of pulses= %d, Pulse Delay microseconds = %d, Pulse Duration microseconds = %d\n", theTask.nPulses, theTask.pulseDelayUsecs ,theTask.pulseDurUsecs);
#endif
		theTask.trainFrequency = gFrequency;
		theTask.trainDuration = gTrainDuration;
		theTask.trainDutyCycle = gDutyCycle;
		theTask.loFunc = gLoFunc;
		theTask.hiFunc =gHiFunc;
		theTask.modCustomData = nullptr; // this pointer is initialised null , as we don't always use it
		delCustomDataFunc = nullptr;
		theTask.endFunc = nullptr;
		theTask.accLevel =gAccLevel;
		// start doTask at 0
		theTask.doTask =0;
		// initialize the task with passed in init func, or just set a pointer to init data if no initFunc
		errCode = 0;
		if (initFunc == nullptr){
#if beVerbose
			printf ("pulsedThread constructor initFunc == nullptr; taskCustomData initialized with copy of initData pointer.\n");
#endif
			theTask.taskCustomData = initData;
		}else{
#if beVerbose
			printf ("pulsedThread constructor is running the provided initFunc\n");
#endif
			errCode =initFunc(initData , theTask.taskCustomData);
		}
		if (errCode){
#if beVerbose
			printf ("pulsedThread initialization callback error: %d\n", errCode);
#endif
		}else{
			// init mutex and condition var
			pthread_mutex_init(&theTask.taskMutex, NULL);
			pthread_cond_init (&theTask.taskVar, NULL);
			// create thread
			pthread_create(&theTask.taskThread, NULL, &pulsedThreadFunc, (void *)&theTask);
		}
	}
}


/* ****************************************************************************************************
gets the lock on the task and increments doTask to signal the thread to do task it is configured to do
will start an infinite train, though there is a separate function for that
Last Modified:
2016/12/13 by Jamie Boyd - improved locking
2015/09/28 by Jamie Boyd - original version */
void pulsedThread::DoTask(void){
	pthread_mutex_lock (&theTask.taskMutex);
	// for infinite task, just make sure bit 0 is set
	if (theTask.nPulses == kINFINITETRAIN){
		theTask.doTask |= 1;
	}else{
		if ((theTask.doTask &~kMODANY) < (kMODDELAY -2)){ // kMODDELAY marks lowest of the signal bits, so is 1 greater than max tasks we can accept
			theTask.doTask +=1;
		}
	}
	pthread_cond_signal(&theTask.taskVar);
	pthread_mutex_unlock( &theTask.taskMutex );
}

/* ****************************************************************************************************
// gets the lock on the task and increments doTask to signal the thread to do task it is configured to do nTasks times
//Last Modified:
2015/09/28 by Jamie Boyd - initial version
2016/12/13 by Jamie Boyd - improved locking */
void pulsedThread::DoTasks(unsigned int nTasks){
	pthread_mutex_lock (&theTask.taskMutex);
	// for infinite task, just make sure bit 0 is set, kind of pointless to call this function
	if (theTask.nPulses == kINFINITETRAIN){
		theTask.doTask |= 1;
	}else{
		if ((theTask.doTask &~kMODANY) < (kMODDELAY - (nTasks + 1))){ 
			theTask.doTask +=nTasks;
		}
	}
	pthread_cond_signal(&theTask.taskVar);
	pthread_mutex_unlock( &theTask.taskMutex );
}

/* ****************************************************************************************************
/returns 0 if a thread is not currently doing a task, else returns number of tasks still left to do
Last Modified 2015/09/28 by Jamie Boyd -  initial verison */
int pulsedThread::isBusy(){
	pthread_mutex_lock (&theTask.taskMutex);
	int taskNum = theTask.doTask;
	pthread_mutex_unlock( &theTask.taskMutex);
	return taskNum;
}

/* *****************************************************************************************************
waits until a thread is no longer doing a task, then returns
Last modified:
2016/12/09 by Jamie Boyd - added paramater for timeout, and added return value for timed out vs not busy */
int pulsedThread::waitOnBusy(float waitSecs){
	struct timespec delaySleeper;
	configureSleeper (1.01 * kSLEEPTURNAROUND, &delaySleeper);
	unsigned int nWait = (waitSecs * 1e06)/(1.01 * kSLEEPTURNAROUND);
	int taskNum ;
	for (unsigned int ii =0; ii < nWait ; ii+=1){
		nanosleep (&delaySleeper, NULL);
		pthread_mutex_lock (&theTask.taskMutex);
		taskNum = theTask.doTask;
		pthread_mutex_unlock( &theTask.taskMutex);
		if (taskNum == 0){
			return 0;
		}
	}
	return 1;
}

/* ***************************************************************************************************
gets the lock on the task and sets doTask bit 0 to signal the thread to start train
Last Modified 2015/09/28 by Jamie Boyd */
void pulsedThread::startInfiniteTrain (void){
	if ((theTask.nPulses == kINFINITETRAIN) && (!(theTask.doTask & 1))){
		pthread_mutex_lock (&theTask.taskMutex);
		theTask.doTask |= 1;
		pthread_cond_signal(&theTask.taskVar);
		pthread_mutex_unlock( &theTask.taskMutex);
	}else{
#if beVerbose
		printf ("startInfiniteTrainError: theTask.nPulses = %d, theTask.doTask = %d.\n", theTask.nPulses , theTask.doTask);
#endif
	}
}

/* ****************************************************************************************************
sets doTask to 0 to signal the thread to stop train. Inifinite train is not in a position to pay attention
to condition variable but is continuously checking doTask
Last Modified 2015/09/28 by Jamie Boyd */
void pulsedThread::stopInfiniteTrain (){
	if ((theTask.nPulses == kINFINITETRAIN) && (theTask.doTask &~kMODANY)) {
		pthread_mutex_lock (&theTask.taskMutex);
		theTask.doTask -= 1;
		pthread_cond_signal(&theTask.taskVar);
		pthread_mutex_unlock( &theTask.taskMutex);
	}
}

/* ****************************************************************************************************
Changes the delay of each pulse by modifying data stored in the structure of the task
Last Modified:
2016/08/08 by Jamie Boyd
2016/12/13 by Jamie Boyd - better locking, delay can be 0 (but duration can't be 0) */
int pulsedThread::modDelay (unsigned int newDelayuSecs){
	
	int errCode = ticks2Times (newDelayuSecs, theTask.pulseDurUsecs, theTask.nPulses, theTask);
	if (errCode){
		return errCode;
	}
	theTask.pulseDelayUsecs = newDelayuSecs;
	pthread_mutex_lock (&theTask.taskMutex);
	theTask.doTask |= kMODDELAY;
	pthread_cond_signal(&theTask.taskVar);
	pthread_mutex_unlock( &theTask.taskMutex);
	return 0;
}

/* ****************************************************************************************************
Changes the duration of the pulse by modifying data stored in the structure of the task
Last Modified:
2016/08/09 by Jamie Boyd
2016/12/13 by Jamie Boyd - better locking */
int pulsedThread::modDur (unsigned int newDurUsecs){
	
	int errCode = ticks2Times (theTask.pulseDelayUsecs, newDurUsecs, theTask.nPulses, theTask);
	if (errCode){
		return errCode;
	}
	theTask.pulseDurUsecs = newDurUsecs;
	pthread_mutex_lock (&theTask.taskMutex);
	theTask.doTask |= kMODDUR;
	pthread_cond_signal(&theTask.taskVar);
	pthread_mutex_unlock( &theTask.taskMutex);
	return 0;
}

/* ****************************************************************************************************
Modifies nPulses directly, leaving frequency and duty cycle alone - no need to get lock , doTask not changed
changing nPulses can change mode (pulse=1, train >=2, infinite train =0)
To change from or to an infinite train, the task must be stopped. ticks2times checks for that
Last Modified:
2016/08/12 by Jamie Boyd - made param an int
2016/12/13 by Jamie Boyd - added guard for changing from or to infinite train
2016/12/14 by Jamie Boyd - using ticks2times */
int pulsedThread::modTrainLength (unsigned int newPulses){

	int errCode = ticks2Times (theTask.pulseDelayUsecs, theTask.pulseDurUsecs, newPulses, theTask);
	if (errCode){
		return errCode;
	}
	theTask.nPulses =newPulses;
	return 0;
}

/* ****************************************************************************************************
Changes the frequency and number of pulses (but not duty cycle or train time duration) of the train 
Only Modifies the structure of the task if it is a train and not a pulse, and if modding the frequency won't make it a pulse
Last Modified:
2016/09/10 by Jamie Boyd - initial version
2016/12/12 by Jamie Boyd - edited for clarity, also used & instead of + to set bits
2016/12/14 by Jamie Boyd - use new times2ticks function */
int pulsedThread::modFreq (float newFreq){

	int errVar = times2Ticks (newFreq, theTask.trainDutyCycle, theTask.trainDuration, theTask);
	if (errVar){
		return errVar;
	}
	theTask.trainFrequency = newFreq;
	pthread_mutex_lock (&theTask.taskMutex);
	theTask.doTask |= (kMODDELAY | kMODDUR);
	pthread_cond_signal(&theTask.taskVar);
	pthread_mutex_unlock( &theTask.taskMutex);
	return 0;
}

/* ****************************************************************************************************
Changes train duration by changing nPulses, leaving frequency and duty cycle alone
Last Modified 2016/12/12 by Jamie Boyd new times2ticks function */
int pulsedThread::modTrainDur (float newDur){
	
	int errVar = times2Ticks (theTask.trainFrequency, theTask.trainDutyCycle, newDur, theTask);
	if (errVar){
		return errVar;
	}
	theTask.trainDuration = newDur;
	return 0;
}

/* ****************************************************************************************************
Changes the duty cycle of the train by modifying delay and duration but not nPulses or frequency
Last Modified 2016/12/14 by Jamie Boyd - times vs ticks */
int pulsedThread::modDutyCycle (float newDutyCycle){
	
	int errVar = times2Ticks (theTask.trainFrequency, newDutyCycle, theTask.trainDuration, theTask);
	if (errVar){
		return errVar;
	}
	theTask.trainDutyCycle = newDutyCycle;
	pthread_mutex_lock (&theTask.taskMutex);
	theTask.doTask |= (kMODDELAY | kMODDUR);
	pthread_cond_signal(&theTask.taskVar);
	pthread_mutex_unlock( &theTask.taskMutex);
	return 0;
}
	
/* ***********************************************************************
Sets function that runs on low tick
last modified 2016/12/06 by Jamie Boyd  - initial version */
void pulsedThread::setLowFunc (void (*loFunc)(void *)){
	theTask.loFunc = loFunc;
}

/* ***********************************************************************
Sets function that runs on high tick
last modified 2016/12/06 by Jamie Boyd  - initial version */
void pulsedThread::setHighFunc (void (*hiFunc)(void *)){
	theTask.hiFunc = hiFunc;
}

/* ***********************************************************************
Sets function that runs at end of each pulse, or train of pulses 
last modified 2016/12/13 by Jamie Boyd  - initial version */
void pulsedThread::setEndFunc (void (*endFunc)(taskParams *)){
	theTask.endFunc = endFunc;
}

/* ***********************************************************************
removes the function that runs at end of each pulse, or train of pulses
last modified 2016/12/13 by Jamie Boyd  - initial version */
void pulsedThread::unSetEndFunc (void){
	theTask.endFunc = nullptr;
}

/* ***********************************************************************
Returns 1 if you have an endFunc set, else 0.  Does not check that
the endFunc is a valid function
last modified 2017/01/27 by Jamie Boyd  - initial version */
int pulsedThread::hasEndFunc  (void){
	if (theTask.endFunc == nullptr){
		return 0;
	}else{
		return 1;
	}
}

/* ****************************************************************************************************
Changes taskCustomData with supplied callback function and pointer to data
Last Modified:
2016/12/07 by Jamie Boyd - first version
2016/12/12 by Jamie Boyd - added option for locking vs non-locking version */
int pulsedThread::modCustom (int (*modFunc)(void *, taskParams * ), void * modData, int isLocking){	
	// if locking, we install callback function and data in the task, and request it to be run
	if (isLocking){
		pthread_mutex_lock (&theTask.taskMutex);
		theTask.modCustomFunc = modFunc;
		theTask.modCustomData = modData;
		theTask.doTask |= kMODCUSTOM;
		pthread_cond_signal(&theTask.taskVar);
		pthread_mutex_unlock( &theTask.taskMutex);
		return 0; // return value from inside thread has nowhere to go
	}else{
	// if not locking, we run the call back function directly 
		return modFunc (modData, &theTask);
	}
}

/* *******************************************************************************************************
Returns 1 if a puslsedThread has requested a custom data modification form the pthread, but it has
not been completed. If no request is pending, returns 0.
Last Modified:
2017/11/29 by Jamie Boyd - first version
*/
int pulsedThread::getModCustomStatus (void){
	if (theTask.doTask & kMODCUSTOM){
		return 1;
	}else{
		return 0;
	}
}

/* sets pointer to a function to delete customData when pulsedThread is killed */
void pulsedThread::setCustomDataDelFunc  (void (*delFunc)( void *)){
	this->delCustomDataFunc = delFunc;
}


unsigned int pulsedThread::getNpulses (void){
	return theTask.nPulses;
}

int pulsedThread::pulsedThread::getpulseDurUsecs (void){
	return theTask.pulseDurUsecs;
}

int pulsedThread::getpulseDelayUsecs (void){
	return theTask.pulseDelayUsecs;
}

// returns pointer to task custom data
void * pulsedThread::getCustomData (void){
	return theTask.taskCustomData;
}

// train duration in seconds
float pulsedThread::getTrainDuration (void){
	return theTask.trainDuration;
}

//  train frequency in Hz
float pulsedThread::getTrainFrequency (void){
	return theTask.trainFrequency;
}

// train duty cycle
float pulsedThread::getTrainDutyCycle (void){
	return theTask.trainDutyCycle;
}
	

/* ****************************************************************************************************
Destructor waits for task to be free, then cancels it
Last Modified:
2018/02/01 by Jamie Boyd - moved wait on busy so it only runs when needed. Also, nw aborts a train in progress after pulses is finished, or 100 seconds
2017/11/29 by jamie Boyd - added call to function pointer, delCustomDataFunc, for deletion of customData
2016/01/16 by Jamie Boyd - removed delete customData and modCustomData, as this should be deleted by maker of pulsed thread
2015/09/29 by Jamie Boyd - initial version */
pulsedThread::~pulsedThread(){
	// stop the task
	if (theTask.doTask){
		pthread_mutex_lock (&theTask.taskMutex);
		theTask.doTask =0;
		pthread_cond_signal(&theTask.taskVar);
		pthread_mutex_unlock( &theTask.taskMutex);
		this->waitOnBusy(100); // should be long enough to finish last pulse
	}
	pthread_mutex_lock (&theTask.taskMutex);
	pthread_cancel(theTask.taskThread);
	pthread_mutex_unlock (&theTask.taskMutex);
	pthread_mutex_destroy (&theTask.taskMutex);
	pthread_cond_destroy (&theTask.taskVar);
	// delete custom data?
	if (this->delCustomDataFunc != nullptr){
		this->delCustomDataFunc (theTask.taskCustomData);
	}
}
