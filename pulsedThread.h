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
2018/01/31 by Jamie Boyd - tidied up a bit, added more comments
2017/12/06 by Jamie Boyd - added code for more control of custom data, including function pointer for custom delete function  
2016/12/21 by Jamie Boyd renamed class to pulsedThread as GPIO stuff was put in separate file
2016/12/14 by Jamie Boyd added endFunc, a function that runs after every task with full taskParams
2016/12/13 by Jamie Boyd improving locking
2016/12/07 by Jamie Boyd using function pointers to make threads more modular, extensible */


 /* ***define beVerbose to non-zero to print out messages that may be useful for debugging or understanding how the code runs ****/
 #define beVerbose 1
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
than 16 million (2^24=16777216), which "should be enough for anybody"
This leaves bits 28,29,30 and 31 for your own purposes, like signaling endFunction*/
const unsigned int kMODDELAY = 33554432; //2^25
const unsigned int kMODDUR = 67108864; //2^26
const unsigned int kMODCUSTOM = 134217728;//2^27
const unsigned int kMODANY = 234881024;//2^27 + 2^26 + 2^25

/* ***************this C-style struct contains all the relevant thread variables and task variables, and is passed to the thread function **********/
struct taskParams {
	int accLevel; // sleeps, sleeps and spins, etc.
	int doTask; // incremented when tasks are requested, decremented when tasks are done
	// raw pulse durations and number of pulses, in microseconds
	unsigned int pulseDelayUsecs; // duration of low time in microseconds, can be 0, in which case loFunc is never called for a train or infinite train
	unsigned int pulseDurUsecs; // duration of high time in microseconds, must be > 0
	unsigned int nPulses; // number of pulses in a train, 0 for infinite train, 1 for a single pulse
	// requested train durations, train fequencies, and train duty cycles. Same information as above
	float trainDuration; // duration of train, in seconds, or 0 for infinite train
	float trainFrequency; // frequency in Hz, i.e., pulses/second
	float trainDutyCycle; // pulseDurUsecs/(pulseDurUsecs + pulseDelayUsecs)
	void *  taskCustomData; // pointer to custom taskData
	void (*loFunc)(void *) ; // function to run for low part of pulse, gets pointer to taskData
	void (*hiFunc)(void * );// function to run for high part of pulse, gets pointer to taskData
	void (*endFunc)(taskParams *); // runs at end of train, or end of each pulse for infinite train or single pulse, gets pointer to the whole task
	int (*modCustomFunc)(void *, taskParams *);//function to mod custom task data, taskes pointer to provided custom data, and to the whole Task, runs when kMODCUSTOM is set in doTask
	void * modCustomData; // needs to be inited before use with modCustomFunc
	pthread_t taskThread;
	pthread_mutex_t taskMutex ;
	pthread_cond_t taskVar;
};

/* **********************Utility functions we want inlined for speed AND available as part of the library must be in the header ************************

***** Converts from pulse-based info (pulseDelay, pulseDuation, number of pulses) to frequency-based info (trainDuration, frequency, dutyCycle) *******/
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
	
	if (((theTask.nPulses == kINFINITETRAIN) && (newnPulses != kINFINITETRAIN)) && (theTask.doTask & 1)){
#if beVerbose
		printf ("times2Ticks error, infinite train with task not stopped: current length = %d and new length = %d\n", theTask.nPulses, newnPulses);
#endif
		return 1;
	}
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
		accLevel is 1 to keep a timer going to track elapsed time, and to cycle on current time for short intervals. processor intensive, but more accurate
		*/
		pulsedThread (unsigned int , unsigned int , unsigned int , void *  , int (*)(void *, void *  &), void (*)(void *), void (*)(void *), int , int &);
		pulsedThread  (float , float , float , void *  , int (*)(void *, void * &), void (*)(void *), void (*)(void *), int , int &);
		virtual ~pulsedThread();
		void DoTask (void); // requests that the thread perform its task once, as currently configured, if not an infinite train, or will start an infinite train
		void DoTasks (unsigned int nTasks); // requests that the thread perform its task nTasks times, as currently configured, or
		int isBusy(); // checks if a task is busy, returns how many tasks are left to do
		int waitOnBusy(float timeOut); // doesn't return until a thread is no longer busy
		// for infinite trains
		void startInfiniteTrain(void);  // starts an infinite train
		void stopInfiniteTrain (void); // stops an infinite train
		// modifiers based on individual pulses
		int modDelay (unsigned int newDelay); // sets delay time, before pulse, in microseconds. 0 means no delay
		int modDur (unsigned int newDur); // sets pulse duration, in microseconds,
		int modTrainLength (unsigned int newTicks); // sets number of pulses in a train. if set to 0 or 1, switches mode to infinite train or single pulse
		// modifiers based on timing
		int modTrainDur (float newDur); // changes duration of pulse train, in seconds (if not an infinite train)
		int modFreq (float newFreq); // changes frequency (Hz) of train. Duration and duty cycle will not be changed
		int modDutyCycle (float newDutyCycle); //changes duty cycle (as for PWM), frequency and duration unchanged
		// custom modifier for custom data, you need to provide the modifier data and the modifier function
		int modCustom (int (*modFunc)(void *, taskParams *), void * modData, int isLocking);
		// returns 1 if waiting for the pthread to do a requested modification
		int getModCustomStatus (void);
		// gets a pointer to custom data, allows you to examine and modify custom data directly - don't modifiy when thread is running, use modCustom for that
		void setCustomDataDelFunc  (void (*delFunc)( void *)); // sets a function that will be run when a pulsedThread is about to be killed
		void * getCustomData (void); // returns a pointer to the custom data. 
		// setters for low, high, and end functions
		void setLowFunc (void (*loFunc)(void *)); // sets the function that is called on low part of cycle
		void setHighFunc (void (*hiFunc)(void *)); // sets the function that is called on high part of cycle
		void setEndFunc (void (*endFunc)(taskParams *)); // sets the function that is called after each pulse, or after each train,gets pointer to the task
		void unSetEndFunc (void); //removes end func, replaces with nullptr
		int hasEndFunc  (void); // returns 1 if an endFunc is installed, else 0
		// getters for timing params in pulses
		unsigned int getNpulses (void); // will be 0 for infiniteTrain
		int getpulseDurUsecs (void); // in microseconds
		int getpulseDelayUsecs (void); // in microseconds, can be 0
		// getters for train based timing params
		float getTrainDuration (void); // train duration in seconds
		float getTrainFrequency (void); //  train frequency in Hz
		float getTrainDutyCycle (void); // duty cycle, dur/(dur + delay)
		
	protected:
		// thread variables - thread, mutex, condition variable, and task variables are all in the taskParams structure
		struct taskParams theTask;
		// function pointer for  function to run to delete taskCustom data, gets passed pointer to custom data. never used by pthread so not in taskParams
		void (*delCustomDataFunc)( void *); 
};

#endif // PULSEDTHREAD_H
