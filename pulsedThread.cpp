#include "pulsedThread.h"

/* ************** the thread function needs to be a C-style function, not a class method ********************************************************
****************************************************************************************************************************************************
Last Modified:
2018/05/23 by Jamie Boyd - changed infinite train while test to while (theTask->doTask & 1), so will stop with an endFunc installed
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
				theTask->modCustomFunc (theTask->modCustomData, theTask);
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
		 // initalize spinEndTime to current time once for 
		if (theTask->accLevel == ACC_MODE_SLEEPS_AND_OR_SPINS){
			gettimeofday (&spinEndTime, NULL);
		}
		// do the task(s) as per nPulses
		switch (theTask->nPulses){
			case kPULSE:
				if (theTask->pulseDelayUsecs > 0){
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS){
						nanosleep (&delaySleeper, NULL);
					}else{
						
						if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
							gettimeofday (&spinEndTime, NULL);
							timeradd (&spinEndTime, &pulseDelayUsecs, &spinEndTime);
							WAITINLINE1 (delaySleeps, &delaySleeper, &spinEndTime);
						}else{
							timeradd (&spinEndTime, &pulseDelayUsecs, &spinEndTime);
							WAITINLINE2 (delaySleeps, &turnaroundTime, &spinEndTime);
						}
					}
				}
				theTask->hiFunc(theTask->taskData);
				if (theTask ->accLevel == 0){
					nanosleep (&durSleeper, NULL);
				}else{
					
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
						gettimeofday (&spinEndTime, NULL);
						timeradd (&spinEndTime, &pulseDurUsecs, &spinEndTime);
						WAITINLINE1 (durSleeps, &durSleeper, &spinEndTime);
					}else{
						timeradd (&spinEndTime, &pulseDurUsecs, &spinEndTime);
						WAITINLINE2 (durSleeps, &turnaroundTime, &spinEndTime);
					}
				}
				theTask->loFunc(theTask->taskData);
				if (theTask->endFunc != nullptr){
					theTask->endFunc (theTask->endFuncData, theTask);
				}
			break;
                
		case kINFINITETRAIN: //an infinite train - don't decrement the queue, and check for delay, duration mods without breaking
			while (theTask->doTask & 1){
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
					theTask->hiFunc(theTask->taskData);
				}
				if (theTask ->accLevel == 0){
					nanosleep (&durSleeper, NULL);
				}else{
					
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
						gettimeofday (&spinEndTime, NULL);
						timeradd (&spinEndTime, &pulseDurUsecs, &spinEndTime);
						WAITINLINE1 (durSleeps, &durSleeper, &spinEndTime);
					}else{
						timeradd (&spinEndTime, &pulseDurUsecs, &spinEndTime);
						WAITINLINE2 (durSleeps, &turnaroundTime, &spinEndTime);
					}
				}
				if (theTask->pulseDelayUsecs > 0){
					if (theTask->loFunc != nullptr){
						theTask->loFunc(theTask->taskData);
					}
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS){
						nanosleep (&delaySleeper, NULL);
					}else{
						
						if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
							gettimeofday (&spinEndTime, NULL);
							timeradd (&spinEndTime, &pulseDelayUsecs, &spinEndTime);
							WAITINLINE1 (delaySleeps, &delaySleeper, &spinEndTime);
						}else{
							timeradd (&spinEndTime, &pulseDelayUsecs, &spinEndTime);
							WAITINLINE2 (delaySleeps, &turnaroundTime, &spinEndTime);
						}
					}
				}
				if (theTask->endFunc != nullptr){
					theTask->endFunc (theTask->endFuncData, theTask);
				}
			}
			break;
			
		default: // kTRAIN
#if beVerbose
			printf ("A train was called with doTask = %d and nPulses = %d.\n", theTask->doTask, theTask->nPulses);
#endif
			for (unsigned int iTick=0; iTick < theTask->nPulses; iTick++){
				if (theTask->pulseDurUsecs > 0) {
					theTask->hiFunc(theTask->taskData);
					if (theTask ->accLevel == ACC_MODE_SLEEPS){
						nanosleep (&durSleeper, NULL);
					}else{
						if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
							gettimeofday (&spinEndTime, NULL);
							timeradd (&spinEndTime, &pulseDurUsecs, &spinEndTime);
							WAITINLINE1 (durSleeps, &durSleeper, &spinEndTime);
						}else{
							timeradd (&spinEndTime, &pulseDurUsecs, &spinEndTime);
							WAITINLINE2 (durSleeps, &turnaroundTime, &spinEndTime);
						}
					}
				}
				if (theTask->pulseDelayUsecs > 0){
					theTask->loFunc(theTask->taskData);
					if  (theTask ->accLevel ==ACC_MODE_SLEEPS){
						nanosleep (&delaySleeper, NULL);
					}else{
						if  (theTask ->accLevel ==ACC_MODE_SLEEPS_AND_SPINS){
							gettimeofday (&spinEndTime, NULL);
							timeradd (&spinEndTime, &pulseDelayUsecs, &spinEndTime);
							WAITINLINE1 (delaySleeps, &delaySleeper, &spinEndTime);
						}else{
							timeradd (&spinEndTime, &pulseDelayUsecs, &spinEndTime);
							WAITINLINE2 (delaySleeps, &turnaroundTime, &spinEndTime);
						}
					}
				}
			}
			if (theTask->endFunc != nullptr){
				theTask->endFunc (theTask->endFuncData, theTask);
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
		theTask.endFunc = nullptr;
		theTask.endFuncData = nullptr;
		delTaskDataFunc = nullptr; //this function pointer is initialised null , as we don't always have a function
		delEndFuncDataFunc = nullptr;
		theTask.accLevel =gAccLevel;
		// start doTask at 0
		theTask.doTask =0;
		// initialize the task with passed in init func
		errCode = 0;
		if (initFunc == nullptr){
#if beVerbose
			printf ("pulsedThread constructor initFunc == nullptr; taskData initialized with copy of initData pointer.\n");
#endif
			theTask.taskData = initData;
		}else{
#if beVerbose
			printf ("pulsedThread constructor is running the provided initFunc\n");
#endif
			errCode =initFunc(initData , theTask.taskData);
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
		theTask.endFunc = nullptr;
		theTask.accLevel =gAccLevel;
		delTaskDataFunc = nullptr;
		delEndFuncDataFunc = nullptr;
		// start doTask at 0
		theTask.doTask =0;
		// initialize the task with passed in init func, or just set a pointer to init data if no initFunc
		errCode = 0;
		if (initFunc == nullptr){
#if beVerbose
			printf ("pulsedThread constructor initFunc == nullptr; taskData initialized with copy of initData pointer.\n");
#endif
			theTask.taskData = initData;
		}else{
#if beVerbose
			printf ("pulsedThread constructor is running the provided initFunc\n");
#endif
			errCode =initFunc(initData , theTask.taskData);
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


/* ************ countermands currently requested tasks ***************************************
 The thread will finish the current pulse or train of pulses then stop
 Last Modified:
2018/03/13 by jamie Boyd - deleted some fancy schmancy stuff to work around high order bits - because it wasn't working, for some reason
2018/02/14 by Jamie Boyd - made sure one task was set for thread to unset, else doTask might go negative
 2018/02/09 by Jamie Boyd - initial Version */
void pulsedThread::UnDoTasks (void){
    if (theTask.doTask > 1) { // no need to do anything unless 1 or more tasks still left to do
        pthread_mutex_lock (&theTask.taskMutex);
	theTask.doTask =1;
        pthread_cond_signal(&theTask.taskVar);
        pthread_mutex_unlock( &theTask.taskMutex);
    }
}


/* ****************************************************************************************************
// gets the lock on the task and increments doTask to signal the thread to do task it is configured to do nTasks times
//Last Modified:
2018/11/05 by Jmie Boyd - added bounds checking
2018/10/25 by Jamie Boyd - initial version */
void pulsedThread::DoOrUndoTasks(int nTasks){
	// for infinite task, this function is a NOP
	if (theTask.nPulses != kINFINITETRAIN){
		pthread_mutex_lock (&theTask.taskMutex);
		// scrunch resulting value of doTask between 0 and kMODDELAY (first of signal bits)
		unsigned int currentTasks = (theTask.doTask &~kMODANY);
		if ((int)(currentTasks + nTasks) < 0){
			theTask.doTask &= 1;
		}else{
			if ((int)currentTasks + nTasks >= kMODDELAY){
				theTask.doTask = kMODDELAY -1;
			}else{
				theTask.doTask +=nTasks;
			}
		}
		pthread_cond_signal(&theTask.taskVar);
		pthread_mutex_unlock( &theTask.taskMutex );
	}
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
2018/05/23 by Jamie Boyd - changed timing from simple loop of nSleep to calculating end time and using timercmp
2016/12/09 by Jamie Boyd - added paramater for timeout, and added return value for timed out vs not busy */
int pulsedThread::waitOnBusy(float waitSecs){
	// make timers, one for current time and one for current time + wait seconds
	struct timeval currentTime;
	struct timeval endTime;
	struct timeval waitTime;
	configureTimer ((waitSecs * 1E06), &waitTime);
	gettimeofday (&endTime, NULL);
	timeradd (&endTime, &waitTime, &endTime);
	// make a timSpec sleeper for sleepTurnaround, typically 200 mSec
	struct timespec Sleeper;
	configureSleeper (kSLEEPTURNAROUND, &Sleeper);
	// check if tasks are done, sleep, check if time is up, repeat
	int taskNum;
	do{
		pthread_mutex_lock (&theTask.taskMutex);
		taskNum = theTask.doTask;
		pthread_mutex_unlock( &theTask.taskMutex);
		if (taskNum ==0){
			break;
		}
		nanosleep (&Sleeper, NULL);
		gettimeofday (&currentTime, NULL);
	}while (timercmp (&currentTime, &endTime, <));
	return taskNum;
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
Last Modified:
2018/05/22 by Jamie Boyd - deleted the higher order bit cheking, it was removed from threadFunc anways 
2015/09/28 by Jamie Boyd - initial version*/
void pulsedThread::stopInfiniteTrain (){
	if ((theTask.nPulses == kINFINITETRAIN) && (theTask.doTask & 1)){
		pthread_mutex_lock (&theTask.taskMutex);
		theTask.doTask =0;
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
void pulsedThread::setEndFunc (void (*endFunc)(void *, taskParams *)){
	theTask.endFunc = endFunc;
}

/* ************************** Setting up modifying End Functions for selecting train frequency/duty from an array ******************

Sets Endfunction that runs at end of each pulse, or train of pulses 
last modified 2016/12/13 by Jamie Boyd  - initial version */
void pulsedThread::chooseArrayEndFunc (int endFuncMode){
	if (endFuncMode == 0){
		theTask.endFunc = &pulsedThreadFreqFromArrayEndFunc;
	}else {
		if (endFuncMode == 1){
			theTask.endFunc = &pulsedThreadDutyCycleFromArrayEndFunc;
		}
	}
}

/* ********************************************************Set Up ******************************************************
sets up/change the array for pulsedThread using the pulsedThreadsetArrayCallback function.
last modified:
2018/02/02 by Jamie Boyd - initial version */
int pulsedThread::setUpEndFuncArray (float * newData, unsigned int nData, int isLocking){
	// fill an array struct from passed-in data
	pulsedThreadArrayStructPtr setUpStruct = new pulsedThreadArrayStruct;
	setUpStruct->arrayData = newData;
	setUpStruct->endPos = nData;
	setUpStruct->startPos =0;
	setUpStruct->arrayPos =0;
	int errVar = modCustom (&pulsedThreadSetUpArrayCallback, (void *) setUpStruct, isLocking);
	if (errVar ==0){
		// set up the custom delete function specific to the array callback
		setEndFuncDataDelFunc (&pulsedThreadArrayStructCustomDel);
	}
	return errVar;	
}


/* *************************************** Set Array Position ************************************
Changes the position the endFunc is currently outputting. Will start iterating from here
Last Modified:
2018/02/21 by Jamie boyd - initial version */
int  pulsedThread::setEndFuncArrayPos (unsigned int arrayPosP, int isLocking){
	// sanity check that endFunc is one of the array endFunctions
	if ((theTask.endFunc == &pulsedThreadFreqFromArrayEndFunc) || (theTask.endFunc == &pulsedThreadDutyCycleFromArrayEndFunc)){
		// make an array mod struct
		pulsedThreadArrayModStructPtr modStruct = new pulsedThreadArrayModStruct;
		modStruct ->modBits = 4;
		modStruct->arrayPos =arrayPosP;
		return modCustom (&pulsedThreadSetArrayLimitsCallback, (void *) modStruct, isLocking);
	}else{
		return 1;
	}
	return 0;
}

/* *************************************** Set Array Limits ************************************
Changes the start and enposition through which the endFunc will  iterate
Last Modified:
2018/02/21 by Jamie boyd - initial version */
int  pulsedThread::setEndFuncArrayLimits (unsigned int startPosP, unsigned int endPosP, int isLocking){
	// sanity check that endFunc is one of the array endFunctions
	if ((theTask.endFunc == &pulsedThreadFreqFromArrayEndFunc) || (theTask.endFunc == &pulsedThreadDutyCycleFromArrayEndFunc)){
		// make an array mod struct
		pulsedThreadArrayModStructPtr modStruct = new pulsedThreadArrayModStruct;
		modStruct ->modBits = 3;
		modStruct-> startPos = startPosP;
		modStruct -> endPos = endPosP;
		return modCustom (&pulsedThreadSetArrayLimitsCallback, (void *) modStruct, isLocking);
	}else{
		return 1;
	}
	return 0;
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

/* ***************************** utility function to fill a passed-in array with a cosine function**********************************
Useful when setting duty cycle from an endfunc. Computed values must be between 0 and 1. offset = 0.5 and scaling = 0.5 will
fill the full range from 0 to 1. period sets the repeat period for the cosine in points. If points = array size, you get 1 period
of the cosine function. There is no parameter for phase, yet.  May add other standard waveforms like sawtooths and ramps
last Modified:
2018/02/21 by Jamie Boyd */
int pulsedThread::cosineDutyCycleArray  (float * arrayData, unsigned int arraySize, unsigned int period, float offset, float scaling){
	const double phi = 6.2831853071794;
	if (((offset - scaling ) < 0) || ((offset + scaling) > 1)){
#if beVerbose
		printf ("adjust offset and scaling so cosine is bounded by 0 and 1/n");
#endif
		return 1;
	}
	for (unsigned int ii=0; ii< arraySize; ii +=1){
		arrayData [ii] = offset -  scaling * cos (phi * (double)(ii % period)/period);
	}
	return 0;
}

/* ****************************************************************************************************
Changes taskData with supplied callback function and pointer to data
Last Modified:
2016/12/12 by Jamie Boyd - added option for locking vs non-locking version 
2016/12/07 by Jamie Boyd - first version */
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

/* ****************** Mutex access if you want to change taskData directly ******************************
you need to have kept a pointer to the taskData, or endFunc data, if you want to do this

 *****************Gets the Mutex ****************************************
Last Modified:
2018/02/08 by Jamie boyd - initial Version */
void pulsedThread::getTaskMutex (void){
	pthread_mutex_lock (&theTask.taskMutex);
}

/* *****************Gives up the Mutex ****************************************
Last Modified:
2018/02/08 by Jamie boyd - initial Version */
void pulsedThread::giveUpTaskMutex (void){
	pthread_mutex_unlock (&theTask.taskMutex);
}


/* *******************************************************************************************************
Returns 1 if a pulsedThread has requested a custom data modification from the pthread, but it has
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
void pulsedThread::setTaskDataDelFunc  (void (*delFunc)( void *)){
	delTaskDataFunc = delFunc;
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


// returns a pointer to the whole taskParams structure
taskParams * pulsedThread::getTask (void){
	return &theTask;
}


// returns pointer to task custom data
void * pulsedThread::getTaskData (void){
	return theTask.taskData;
}

// returns pointer to task endFunc data
void * pulsedThread::getEndFuncData (void){
	return theTask.endFuncData;
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

/* sets pointer to a function to delete customData when pulsedThread is killed */
void pulsedThread::setEndFuncDataDelFunc  (void (*delFunc)( void *)){
	delEndFuncDataFunc = delFunc;
}



/* ****************************************************************************************************
Destructor waits for task to be free, then cancels it
Last Modified:
2018/05/26 by Jamie Boyd - waits for current pulse or train to finish
2018/02/01 by Jamie Boyd - moved wait on busy so it only runs when needed. Also, nw aborts a train in progress after pulses is finished, or 100 seconds
2017/11/29 by jamie Boyd - added call to function pointer, delCustomDataFunc, for deletion of customData
2016/01/16 by Jamie Boyd - removed delete customData and modCustomData, as this should be deleted by maker of pulsed thread
2015/09/29 by Jamie Boyd - initial version */
pulsedThread::~pulsedThread(){
	// stop the task
	if (theTask.nPulses == kINFINITETRAIN){
		if (theTask.doTask & 1){
			pthread_mutex_lock (&theTask.taskMutex);
			theTask.doTask =0;
			pthread_cond_signal(&theTask.taskVar);
			pthread_mutex_unlock( &theTask.taskMutex);
		}	
	}else{
		if (theTask.doTask > 1) { // no need to do anything unless 1 or more tasks still left to do
			pthread_mutex_lock (&theTask.taskMutex);
			theTask.doTask =1;
			pthread_cond_signal(&theTask.taskVar);
			pthread_mutex_unlock( &theTask.taskMutex);
		}
	}
	pthread_mutex_lock (&theTask.taskMutex);
	pthread_cancel(theTask.taskThread);
	pthread_mutex_unlock (&theTask.taskMutex);
	pthread_mutex_destroy (&theTask.taskMutex);
	pthread_cond_destroy (&theTask.taskVar);
	// delete task custom data?
	if (delTaskDataFunc != nullptr){
		delTaskDataFunc (theTask.taskData);
	}
	// delete endFunction custom data ?
	if (delEndFuncDataFunc != nullptr){
		delEndFuncDataFunc (theTask.endFuncData);
	}
}

 /* *************************************CallBacks for EndFunctions using an Array of values  *************
Functions for using an array of values for frequency or duty cycle with an endFunc to cycle through the array using pulsedThreadArrayStruct
EndFuncData is pulsedThreadArrayStructPtr includes
float * arrayData
unsigned int startPos
unsigned int endPos
unsigned int arrayPos 

***************************** EndFunc dataMod Callback**********************************
 Sets up the array of data used to output new values for frequency, duty cycle
Use with pulsedThread::modCustom
last modified:
2018/02/05 by Jamie Boyd - updated for separate pointer for endFunc Data
2017/03/02 by Jamie Boyd - initial version */
int pulsedThreadSetUpArrayCallback (void * modData, taskParams * theTask){
	// cast modData to a pulsedThreadArrayStructPtr
	pulsedThreadArrayStructPtr modDataP = (pulsedThreadArrayStructPtr)modData;
	// make a new pulsedThreadArrayStruct , and point endFuncDataPtr at it
	pulsedThreadArrayStructPtr endFuncDataPtr = new pulsedThreadArrayStruct;
	theTask->endFuncData = endFuncDataPtr;
	// copy over the data from modData
	endFuncDataPtr -> arrayData = modDataP->arrayData; // Don't copy data, just pointer to the data. So calling function must not delete arrayDat while the thread is active
	endFuncDataPtr -> startPos = modDataP->startPos; // position in the array to output
	endFuncDataPtr -> endPos = modDataP->endPos; // number of points in the array
	endFuncDataPtr -> arrayPos = modDataP->arrayPos; // position in the array to output
	// delete modData
	delete ((pulsedThreadArrayStructPtr)modData);
	return 0;
} 

/* ***************** Array position modification Callback **************************************************
Sets starting, ending, and/or currrent position within array, scrunching current position to new array subset
does NOT check if endArray position is actually within bounds of the array 
Last modified:
2018/02/21 by jamie Boys - initial version */
int pulsedThreadSetArrayLimitsCallback (void * modData, taskParams * theTask){
	// cast modData to a pulsedThreadArrayStructPtr
	pulsedThreadArrayModStructPtr modDataPtr = (pulsedThreadArrayModStructPtr)modData;
	// cast endFuncDataPtr to pulsedThreadArrayStructPtr
	pulsedThreadArrayStructPtr endFuncDataPtr = (pulsedThreadArrayStructPtr)theTask->endFuncData;
	// copy over the data from modData
	if ((modDataPtr->modBits) & 1){
		endFuncDataPtr -> startPos = modDataPtr->startPos; // starting position in the array
	}
	if ((modDataPtr->modBits) & 2){
		endFuncDataPtr -> endPos = modDataPtr->endPos; //  ending position in the array
	}
	if ((modDataPtr->modBits) & 4){
		endFuncDataPtr -> arrayPos = modDataPtr->arrayPos; //  current position in the array
	}
	// scrunch arrayPos to start,end
	if ((endFuncDataPtr -> arrayPos < endFuncDataPtr -> startPos) || (endFuncDataPtr -> arrayPos > endFuncDataPtr -> endPos)){
		endFuncDataPtr -> arrayPos = endFuncDataPtr -> startPos;
	}
#if beVerbose
	printf ("array start position changed to %i\n", endFuncDataPtr-> startPos);
	printf ("array end position changed to %i\n", endFuncDataPtr-> endPos);
#endif
	// delete modData
	delete ((pulsedThreadArrayModStructPtr)modData);
	return 0;
} 

/* ************************ EndFunc sets Train Frequency from Array in endFunc data ***************************************
last Modified:
2018/02/05 by Jamie Boyd - updated for separate pointer for endFunc Data */
void pulsedThreadFreqFromArrayEndFunc (void * endFuncData, taskParams * theTask){
	// cast endFunc data pointer to pulsedThreadArrayStructPtr
	pulsedThreadArrayStructPtr ArrayStructPtr  = (pulsedThreadArrayStructPtr)endFuncData;
	// move to next point in array and warap to start if needed
	ArrayStructPtr->arrayPos +=1;
	if(ArrayStructPtr->arrayPos == ArrayStructPtr->endPos){
		ArrayStructPtr->arrayPos =ArrayStructPtr->startPos;
	}
	// use times2Ticks to update taskParams with new timing values
	times2Ticks (ArrayStructPtr->arrayData[ArrayStructPtr->arrayPos], theTask->trainDutyCycle, theTask->trainDuration, *theTask);
	theTask->trainFrequency = ArrayStructPtr->arrayData[ArrayStructPtr->arrayPos];
	// set high order signal bits of doTask that pulse duration and delay have changed
	theTask->doTask |= (kMODDUR | kMODDELAY);
}

/* ************************ EndFunc sets Train Duty Cycle from Array in endFunc data ***************************************
last Modified:
2018/02/05 by Jamie Boyd - updated for separate pointer for endFunc Data */
void pulsedThreadDutyCycleFromArrayEndFunc (void * endFuncData, taskParams * theTask){
	// cast endFunc data pointer to pulsedThreadArrayStructPtr
	pulsedThreadArrayStructPtr ArrayStructPtr  = (pulsedThreadArrayStructPtr)endFuncData;
	// move to next point in array and warap to start if needed
	ArrayStructPtr->arrayPos +=1;
	if(ArrayStructPtr->arrayPos == ArrayStructPtr->endPos){
		ArrayStructPtr->arrayPos =ArrayStructPtr->startPos;
	}
	// use times2Ticks to update taskParams with new timing values
	times2Ticks (theTask->trainFrequency, ArrayStructPtr->arrayData[ArrayStructPtr->arrayPos], theTask->trainDuration, *theTask);
	theTask->trainDutyCycle = ArrayStructPtr->arrayData[ArrayStructPtr->arrayPos];
	theTask->doTask |= (kMODDUR | kMODDELAY);
}

 /* **************************************************
 * delete function for endFunc data ONLY for a task using pulsedThreadArrayStruct,
 *  called when pulsedThread is killed if you explicitly install it with
 *  pulsedThread::setendFuncDataDelFunc
 * last modified:

 * 2017/12/06 by Jamie Boyd - initial version */
 void pulsedThreadArrayStructCustomDel (void * endFuncData){
	 if (endFuncData != nullptr){
		 pulsedThreadArrayStructPtr ArrayStructPtr = (pulsedThreadArrayStructPtr) endFuncData;
		 delete (ArrayStructPtr);
	 }
 }
