#! /usr/bin/python
#-*-coding: utf-8 -*-

"""
testing code for ptPyFuncs - allows you to use pulsedThread C++ class with HiFunc/LoFunc and, optionally,
endFunc from a Python object. Most of the functions in ptPyFuncs are wrappers for functions
defined in pyPulsedThread.h. PT_Py_Greeter provides a simple example of a Python Object interface
using ptPyFuncs to time trains of pulses.

The PT_Py_Greeter class field task_ptr is a pyCapsule that points to a pulsedThread object on the C++
side of things. All the calls to the C++ code is through an object's task_ptr, which is created in the
PT_Py_Greeter __init__ method.

The PT_Py_Greeter HiFunc and LoFunc simply print hellos and good byes.

The __main__ function makes a PT_Py_Greeter and sets it going, then does some calculations in a loop,
printing results as it goes, to show the independence of the threaded PT_Py_Greeter from the main code.
The class field PSEUDO_MUTEX is used for preventing print statements from different places in the code
from executing at the same time, which leads to garbled output. Unlike a real mutex, execution of the
thread is not halted while waiting on the PSEUDO_MUTEX, hence the loops with calls to sleep to allow the
other threads of execution to continue while waiting for the mutex to be free. Also, read/write to the
PSEUDO_MUTEX is not atomic; one thread may read PSEUDO_MUTEX as 0, and set it to 1, but in the interval
between reading and writing to PSEUDO_MUTEX,another thread may have read PSEUDO_MUTEX as 0 and both
threads think they have the mutex.
"""
import pulsedThread
import ptPyFuncs
from time import sleep as sleep
from time import time as time

class PT_Py_Greeter (object):

    INIT_PULSES =1
    INIT_FREQ=2
    ACC_MODE_SLEEPS = 0
    ACC_MODE_SLEEPS_AND_SPINS =1
    ACC_MODE_SLEEPS_AND_OR_SPINS =2
    END_FUNC_FREQ_MODE =0
    END_FUNC_PULSE_MODE =1
    PSEUDO_MUTEX =0
    
    def __init__ (self, initMode, pulseDelayOrTrainFreq, pulseDurationOrTrainDutyCycle, nPulsesOrTrainDuration, accuracy_level, p_name):
        self.name = p_name
        self.nTimes = 0
        if initMode == PT_Py_Greeter.INIT_FREQ:
            self.task_ptr = ptPyFuncs.initByFreq (self, float(pulseDelayOrTrainFreq), float (pulseDurationOrTrainDutyCycle), float (nPulsesOrTrainDuration), accuracy_level)
        else:
            self.task_ptr = ptPyFuncs.initByPulse (self, int (pulseDelayOrTrainFreq*1E6), int (pulseDurationOrTrainDutyCycle * 1E6), int(nPulsesOrTrainDuration), accuracy_level)

    def HiFunc(self):
        while PT_Py_Greeter.PSEUDO_MUTEX== 1:
            sleep (0.01)
        PT_Py_Greeter.PSEUDO_MUTEX =1
        if self.nTimes == 0:
            print ("Hello from ", self.name)
        else:
            print ("Hello for the ", self.nTimes + 1, "th time from ", self.name)
        PT_Py_Greeter.PSEUDO_MUTEX  =0


    def LoFunc(self):
        while PT_Py_Greeter.PSEUDO_MUTEX == 1:
            sleep (0.01)
        PT_Py_Greeter.PSEUDO_MUTEX  =1
        if self.nTimes == 0:
            print ("Gooodbye from ", self.name)
        else:
            print ("Goodbye for the ", self.nTimes + 1, "th time from ", self.name)
        PT_Py_Greeter.PSEUDO_MUTEX  =0
        self.nTimes +=1
        
        
    def EndFunc (self, pulseDelayUsecs, pulseDurUsecs, nPulses, doTask):
        while PT_Py_Greeter.PSEUDO_MUTEX  == 1:
            sleep (0.01)
        PT_Py_Greeter.PSEUDO_MUTEX =1
        print ("At the end of this train, do Task = ", doTask)
        PT_Py_Greeter.PSEUDO_MUTEX = 0
        
    def TurnOnEndFunc (self, mode):
        pulsedThread.setEndFuntionObject(self.task_ptr, self, mode)

    def TurnOffEndFunc (self, mode):
        pulsedThread.unsetEndFunc (self.task_ptr)
        
    def greet(self):
        pulsedThread.doTask(self.task_ptr)

    def greet_many (self, n_trains):
        pulsedThread.doTasks(self.task_ptr, n_trains)

    def get_goodbye_time (self):
        return pulsedThread.getPulseDelay(self.task_ptr)

    def get_hello_time (self):
        return pulsedThread.getPulseDuration (self.task_ptr)

    def get_num_greets (self):
        return pulsedThread.getPulseNumber (self.task_ptr)

    def set_num_greets (self, numGreets):
        pulsedThread.modTrainLength (self.task_ptr, numGreets)

    def set_goodbye_time (self, goodbyeSecs):
       pulsedThread.modDelay (self.task_ptr, goodbyeSecs)

    def set_hello_time (self, helloSecs):
        pulsedThread.modDur (self.task_ptr, helloSecs)
        
    def is_greeting (self):
        return pulsedThread.isBusy (self.task_ptr)
    
    def wait_greeting (self, delaySecs):
        """There is no ptPyFuncs.waitOnBusy because of the Global interpreter Lock.
        This Python function would have the GIL and be waiting on C module ptPyFuncs
        which was trying to get the GIL so it could call a Python function"""
        endTime = time() + delaySecs
        while ptPyFuncs.isBusy (self.task_ptr) and time() < endTime:
            sleep (0.05)
        return ptPyFuncs.isBusy (self.task_ptr) 


if __name__ == '__main__':
    pseudo_mutex =0
    pyGreeter = PT_Py_Greeter (PT_Py_Greeter.INIT_FREQ, 2, 0.5, 3, PT_Py_Greeter.ACC_MODE_SLEEPS, "Python functions run from C++")
    print ("hello time is ",  pyGreeter.get_hello_time(), 'seconds')
    print ("goodbye time is ",  pyGreeter.get_goodbye_time(), 'seconds')
    pyGreeter.TurnOnEndFunc (PT_Py_Greeter.END_FUNC_PULSE_MODE)
    pyGreeter.greet_many (3)
    result = 0
    while pyGreeter.is_greeting ():
        for i in range (1,20000):
            result += (i*i/(i + 1))
        while PT_Py_Greeter.PSEUDO_MUTEX == 1:
             sleep (0.1)
        PT_Py_Greeter.PSEUDO_MUTEX  =1
        print ('result = ', result)
        PT_Py_Greeter.PSEUDO_MUTEX =0
    print ('Greeter is finished now. Final result was', result)
    del (pyGreeter)
