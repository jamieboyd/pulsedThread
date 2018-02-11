#ifndef PULSEDTHREAD_H
#define PULSEDTHREAD_H

#include <pthread.h>
#include <sys/time.h>
#include <math.h>
#include <stdlib.h>
#include <cstddef>
#include <cstdint>

/* *********************Class to make and signal a task that does one of:************************************************
    1) a single timed  pulse, recallable, for solenoids, e.g.
    2) train of pulses as for buzzers or LEDS, recallable
    3) infinite train with start/stop
  
Mixes C-style functions and structures with C++ to use pthreads with convenience of classes

Last Modified:
2018/02/05 by Jamie Boyd - made a separate pointer in taskParams for endFuncData and added endFuncs for frequency and duty cycle
2018/01/31 by Jamie Boyd - tidied up a bit, added more comments
2017/12/06 by Jamie Boyd - added code for more control of custom data, including function pointer for custom delete function  
2016/12/21 by Jamie Boyd renamed class to pulsedThread as GPIO stuff was put in separate file
2016/12/14 by Jamie Boyd added endFunc, a function that runs after every task with full taskParams
2016/12/13 by Jamie Boyd improving locking
2016/12/07 by Jamie Boyd using function pointers to make threads more modular, extensible */


 /* ***define beVerbose to non-zero to print out messages that may be useful for debugging or understanding how the code runs ****/
 #define beVerbose 0
 #if beVerbose
 #include <stdio.h>
#endif
/* *************************************constants for timing methods: ***************************************************************/
const int ACC_MODE_SLEEPS = 0;				// thread sleeps for duration of period
const int ACC_MODE_SLEEPS_AND_SPINS =1;		//thread sleeps for period - kSLEEPTURNAROUND microseconds, then spins for remaining time
const int ACC_MODE_SLEEPS_AND_OR_SPINS =2;	//sleep time is re-calculated for each duration, sleep is countermanded if thread is running late
const int kSLEEPTURNAROUND= 200;			//how long, in microseconds, we aim to spin for the end of pulse timing in accuracy levels 1 and 2

/* ***********************************constants for different task modes ************************************************************************/
const int kPULSE= 1; // waits for delay, calls hiFunc, waits for duration, calls low func
const int kTRAIN= 2; // for nPulses, calls hiFunc, waits for duration, if Delay > 0, calls low func and waits for delay
const int kINFINITETRAIN = 0; // calls hiFunc, waits for duration, if Delay > 0, calls low func and waits for delay, repeats


/* *****************************************constants for task modification signals *******************************************************************
The higher order bits of doTask in taskParams struct are reserved for flags, signalling that the task has been modified and that the thread should
wake up and change timing paramaters.  Even with reserved bits, the number of tasks that can be requested can still be more
than 500 million (2^29 -1 =536,870,911 which "should be enough for anybody" */
const unsigned int kMODDELAY = 536870912; 		//2^29
const unsigned int kMODDUR = 1073741824;		//2^30
const unsigned int kMODCUSTOM = 2147483648;	//2^31
const unsigned int kMODANY = 3221225530;		//2^29 + 2^30 + 2^31

/* ***************this C-style struct contains all the relevant thread variables and task variables, and is passed to the thread function *********
last modified:
2018/02/05 by Jamie Boyd - added separate pointer for endFunc data as separate from taskData */
struct taskParams {
	int accLevel; // sleeps, sleeps and spins, sleeps and/or spins.
	int doTask; // incremented when tasks are requested, decremented when tasks are done
	/* ****************************raw pulse durations and number of pulses, in microseconds *******************************************/
	unsigned int pulseDelayUsecs; // duration of low time in microseconds, can be 0, in which case loFunc is never called for a train or infinite train
	unsigned int pulseDurUsecs; // duration of high time in microseconds, must be > 0
	unsigned int nPulses; // number of pulses in a train, 0 for infinite train, 1 for a single pulse
	/* *********** train durations, train fequencies, and train duty cycles. Same information as above ************************************/
	float trainDuration; // duration of train, in seconds, or 0 for infinite train
	float trainFrequency; // frequency in Hz, i.e., pulses/second
	float trainDutyCycle; // pulseDurUsecs/(pulseDurUsecs + pulseDelayUsecs)
	/* *****************************Hi and Lo functions, and pointer to their custom data, ********************************/
	void (*loFunc)(void *) ; // function to run for low part of pulse, gets pointer to taskData
	void (*hiFunc)(void *); // function to run for high part of pulse, gets pointer to taskData
	void * taskData; // pointer to custom taskData for hi and lo functions
	/* *************************** EndFunction and pointer to its custom data *******************************************/
	void (*endFunc)(taskParams *); // runs at end of train, or end of each pulse for infinite train or single pulse, gets pointer to the whole task
	void * endFuncData; // pointer for custom data for end functions
	/* *************** function to mod custom data and pointer to data to use with mod function ***********************/
	int (*modCustomFunc)(void *, taskParams *); // runs when kMODCUSTOM is set in doTask
	void * modCustomData; // needs to be initialized before use with modCustomFunc
	/* ************************************* pthread variables *************************************************************/
	pthread_t taskThread;
	pthread_mutex_t taskMutex ;
	pthread_cond_t taskVar;
};

/* ******************* A Custom struct for endFunc Data using an array **************************
for the two provided endFuncs that change frequency and dutyCycle for trains */
typedef struct pulsedThreadArrayStruct{
	float * arrayData;		// pointer to an array of floats
	unsigned int startPos; // where to start in the array when out putting data
	unsigned int endPos;		// where to end in the array
	unsigned int arrayPos;	// current position in array, as it is iterated through
}pulsedThreadArrayStruct, *pulsedThreadArrayStructPtr;


/* *************************** Function Declarations for non-class Functions used by pthread **********************************/
int pulsedThreadSetUpArrayCallback (void * modData, taskParams * theTask);
void pulsedThreadFreqFromArrayEndFunc (taskParams * theTask);
void pulsedThreadDutyCycleFromArrayEndFunc (taskParams * theTask);
void pulsedThreadArrayStructCustomDel(void * taskData);


/* **************** Non-Class Utility Functions Used by Thread that we want Inlined for speed yet available for subclasses ********************
******************************************************************************************************************************************

 ******************* Configure timespecs and timevals for thread timing and to do the waiting for acc levels 1 and 2 **************
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


/* ************************************** Utility functions to convert between pulse timing and train frequency/duration *********************************
 **** Converts from pulse-based info (pulseDelay, pulseDuation, number of pulses) to frequency-based info (trainDuration, frequency, dutyCycle) *******/
inline int ticks2Times (unsigned int pulseDelay, unsigned int pulseDuration, unsigned int nPulses, taskParams &theTask){
#if beVerbose
	printf ("ticks2Times requested params: delay = %d, duration = %d, nPulses = %d\n", pulseDelay, pulseDuration, nPulses);
#endif
	if (pulseDuration == 0){
#if beVerbose
		printf ("ticks2Times requested param error: delay = %d, duration = %d, nPulses = %d\n", pulseDelay, pulseDuration, nPulses);
#endif
		return 1;
	}

	if (((theTask.nPulses == kINFINITETRAIN) && (nPulses != kINFINITETRAIN)) && (theTask.doTask & 1)){
#if beVerbose
		printf ("ticks2Times error, infinite train with task not stopped: current length = %d and new length = %d\n", theTask.nPulses, nPulses);
#endif
		return 1;
	}
	float pulseTime = float(pulseDelay + pulseDuration)/1e06;
	theTask.trainFrequency = 1/pulseTime;
	theTask.trainDuration = pulseTime * nPulses;
	theTask.trainDutyCycle = ((float)pulseDuration)/((float)(pulseDelay + pulseDuration));
	return 0;
}

/* ** Converts from frequency-based info (trainDuration, frequency, dutyCycle) to pulse-based info (pulseDelay, pulseDuation, number of pulses) **/
inline int times2Ticks (float frequency, float dutyCycle, float trainDuration, taskParams &theTask){
#if beVerbose
	printf ("times2Ticks requested params:  frequency = %.2f, dutyCycle = %.2f, trainDuration = %.2f\n", frequency, dutyCycle, trainDuration);
#endif
	if ((trainDuration < 0) || (dutyCycle <=0) || (dutyCycle > 1) || (frequency < 0)){
#if beVerbose
		printf ("times2Ticks requested param error:  frequency = %.2f, dutyCycle = %.2f, trainDuration = %.2f\n", frequency, dutyCycle, trainDuration);
#endif
		return 1;
	}
	float pulseMicrosecs = 1e06 / frequency;
	unsigned int newDelay= round (pulseMicrosecs * (1 - dutyCycle));
	unsigned int newDur = round (pulseMicrosecs * dutyCycle);
	unsigned int newnPulses = round ((trainDuration * 1e06) / pulseMicrosecs);
	/*
	if (((theTask.nPulses == kINFINITETRAIN) && (newnPulses != kINFINITETRAIN)) && (theTask.doTask & 1)){
#if beVerbose
		printf ("times2Ticks error, infinite train with task not stopped: current length = %d and new length = %d\n", theTask.nPulses, newnPulses);
#endif
		return 1;
	}
	*/
	theTask.pulseDelayUsecs = newDelay;
	theTask.pulseDurUsecs =  newDur;
	theTask.nPulses = newnPulses;
	return 0;
}

/* ***************************************Declaration of the pulsedThread class *******************************************************************************/
class pulsedThread{
	public:
		/* Constructors 
		errCode is a reference variable that returns 1 if input was not ok, else 0. Should really throw an exception....
		accLevel is 0 to trust nanosleep for the timing - may not be as accurate, but less processor intenisve, good for up to a couple hundred Hz,
		accLevel is 1 to keep a timer going to track elapsed time, and to cycle on current time for short intervals. processor intensive, but more accurate  */
		pulsedThread (unsigned int, unsigned int, unsigned int, void *  , int (*)(void *, void *  &), void (*)(void *), void (*)(void *), int , int &);
		pulsedThread  (float, float, float, void *, int (*)(void *, void * &), void (*)(void *), void (*)(void *), int , int &);
		virtual ~pulsedThread();
		/* ********************* Requesting a task and checking if we are doinga task ***********************************************************/
		void DoTask (void); // requests that the thread perform its task once, as currently configured, if not an infinite train, or will start an infinite train
		void DoTasks (unsigned int nTasks); // requests that the thread perform its task nTasks times, as currently configured, or
        void UnDoTasks (void); // zeros requested tasks in doTask. Thread stops after current task
        int isBusy(); // checks if a task is busy, returns how many tasks are left to do
		int waitOnBusy(float timeOut); // doesn't return until a thread is no longer busy
		// for infinite trains
		void startInfiniteTrain(void);  // starts an infinite train
		void stopInfiniteTrain (void); // stops an infinite train
		/* *********************************** Modifying  and Checking Timing by Pulse Time  ****************************************************************/
		int modDelay (unsigned int newDelay); // sets delay time, before pulse, in microseconds. 0 means no delay
		int modDur (unsigned int newDur); // sets pulse duration, in microseconds,
		int modTrainLength (unsigned int newTicks); // sets number of pulses in a train. if set to 0 or 1, switches mode to infinite train or single pulse
		unsigned int getNpulses (void); // will be 0 for infiniteTrain
		int getpulseDurUsecs (void); // in microseconds
		int getpulseDelayUsecs (void); // in microseconds, can be 0
		/* *********************************** Modifying and Checking Timing by Frequency and Duty Cycle  *******************************/
		int modTrainDur (float newDur); // changes duration of pulse train, in seconds (if not an infinite train)
		int modFreq (float newFreq); // changes frequency (Hz) of train. Duration and duty cycle will not be changed
		int modDutyCycle (float newDutyCycle); //changes duty cycle (as for PWM), frequency and duration unchanged
		float getTrainDuration (void); // train duration in seconds
		float getTrainFrequency (void); //  train frequency in Hz
		float getTrainDutyCycle (void); // duty cycle, dur/(dur + delay)
		/* ********** Modifying custom data (taskData or endFunc data) with provided modifier data and modifier function ***************/
		int modCustom (int (*modFunc)(void *, taskParams *), void * modData, int isLocking); // for either taskData or endFunc data
		int getModCustomStatus (void); // returns 1 if waiting for the pthread to do a requested modification for either taskData or endFunc data
		float * cosineDutyCycleArray  (unsigned int arraySize, unsigned int period, float offset, float scaling); //Utility function to return an array containing a cosine wave useful for pulsedThreadDutyCycleFromArrayEndFunc 
		int setUpEndFuncArray (float * newData, unsigned int nData, int isLocking);
		void getTaskMutex (void);
		void giveUpTaskMutex (void);
		void * getTaskData (void); // returns a pointer to the custom data for the task
		void * getEndFuncData (void); // returns a pointer to the endFunc data for the task
		/* ************* checking task data and Hi and Lo functions ******************************************************************/
		void setLowFunc (void (*loFunc)(void *)); // sets the function that is called on low part of cycle
		void setHighFunc (void (*hiFunc)(void *)); // sets the function that is called on high part of cycle
		void setTaskDataDelFunc  (void (*delFunc)( void *)); // sets a function that will be run when a pulsedThread is about to be killed
		/* ********************************* setting and unsetting endFunction **********************************************/
		void setEndFunc (void (*endFunc)(taskParams *)); // sets the function that is called after each pulse, or after each train,gets pointer to the task
		void unSetEndFunc (void); //removes end func, replaces with nullptr
		int hasEndFunc  (void); // returns 1 if an endFunc is installed, else 0
		void setEndFuncDataDelFunc  (void (*delFunc)( void *)); // sets a function that will be run when a pulsedThread is about to be killed
	protected:
		/* *******************************taskParams structure ***********************************************************************************/
		struct taskParams theTask;  // thread, mutex, condition variable, and task variables are all in theTask 
		/* ********************************* function pointers for destructor to run ******************************************************/
		void (*delTaskDataFunc)( void *); // deletes custom Task Data used by Hifunc and LoFunc
		void (*delEndFuncDataFunc) (void *); // deletes custom data used by endFunc
};

#endif // PULSEDTHREAD_H
