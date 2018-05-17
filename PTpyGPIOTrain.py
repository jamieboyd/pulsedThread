#! /usr/bin/python
#-*-coding: utf-8 -*-

"""More testing code for ptPyFuncs - allows you to use pulsedThread C++ class with HiFunc/LoFunc and, optionally,
endFunc from a Python object. Most of the functions in ptPyFuncs are wrappers for functions
defined in pyPulsedThread.h. PT_Py_GPIO_train provides a simple example using ptPyFuncs
with RPi.GPIO to time trains of pulses output on Raspberry Pi GPIO pins
"""

import ptPyFuncs
from time import time, sleep
import RPi.GPIO as GPIO
from array import array as array
from math import cos, pi

class PT_Py_GPIO_train (object):
    INIT_PULSES =1
    INIT_FREQ=2
    ACC_MODE_SLEEPS = 0
    ACC_MODE_SLEEPS_AND_SPINS =1
    ACC_MODE_SLEEPS_AND_OR_SPINS =2
    END_FUNC_FREQ_MODE =0
    END_FUNC_PULSE_MODE =1

    def __init__ (self, initMode, pulseDelayOrTrainFreq, pulseDurationOrTrainDutyCycle, nPulsesOrTrainDuration, accuracy_level, p_pin):
        self.pin = p_pin
        GPIO.setup(self.pin, GPIO.OUT)
        if initMode == PT_Py_GPIO_train.INIT_FREQ:
            self.task_ptr = ptPyFuncs.initByFreq (self, float(pulseDelayOrTrainFreq), float (pulseDurationOrTrainDutyCycle), float (nPulsesOrTrainDuration), accuracy_level)
        else:
            self.task_ptr = ptPyFuncs.initByPulse (self, int (pulseDelayOrTrainFreq*1E6), int (pulseDurationOrTrainDutyCycle * 1E6), int(nPulsesOrTrainDuration), accuracy_level)
        self.period =0
        
    def HiFunc(self):
        GPIO.output (self.pin, GPIO.HIGH)
        #print ('LO')

    def LoFunc(self):
        GPIO.output (self.pin, GPIO.LOW)
        #print ('HI')
        
    def EndFunc (self, trainFreq, trainDutyCycle, trainLength, doTask):
        ptPyFuncs.modTrainDuty (self.task_ptr, self.periodArray[self.iArray])
        self.iArray +=1
        if self.iArray == self.period:
            self.iArray =0
        
    def TurnOnEndFunc (self, p_period):
        self.periodArray = array ('f', [0] * 1000)
        ptPyFuncs.cosDutyCycleArray(self.periodArray, 100, 0.6, 0.4)
        self.period = p_period
        self.iArray = 0
        #ptPyFuncs.setArrayEndFunc(self.task_ptr, self.periodArray, 0, 0)
        ptPyFuncs.setEndFuncObj(self.task_ptr, self, 1, 1)

    def TurnOffEndFunc (self, mode):
        ptPyFuncs.unsetEndFunc (self.task_ptr)

    def DoTrain (self):
        ptPyFuncs.doTask (self.task_ptr)

    def DoTrains (self, nTrains):
        ptPyFuncs.doTasks (self.task_ptr, nTrains)

    def IsBusy (self):
        return ptPyFuncs.isBusy (self.task_ptr)
    
    def WaitOnBusy (self, delaySecs):
        """Don't use pulsedThread.waitOnBusy because of the Global interpreter Lock.
        This Python function would have the GIL and be waiting on C module ptPyFuncs
        which was trying to get the GIL so it could call a Python endfunction"""
        endTime = time() + delaySecs
        while ptPyFuncs.isBusy (self.task_ptr) and time() < endTime:
            sleep (0.05)
        return ptPyFuncs.isBusy (self.task_ptr) 

    def Set_level (self, level):
        return ptPyFuncs.setlevel (self.task_ptr, level)

if __name__ == '__main__':
    
    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)
    try:
        print ("ACC_MODE_SLEEPS")
        train1 = PT_Py_GPIO_train (PT_Py_GPIO_train.INIT_FREQ, 100, 0.01, 0.1, PT_Py_GPIO_train.ACC_MODE_SLEEPS, 23)
    except Exception as e:
        print (e);
    else:
        train1.TurnOnEndFunc (100)
        train1.DoTrains (200)
        train1.WaitOnBusy (100)
        del (train1)
        print ("ACC_MODE_SLEEPS_AND_SPINS")
    try:
        train2 = PT_Py_GPIO_train (PT_Py_GPIO_train.INIT_FREQ, 200, 0.01, 0.1, PT_Py_GPIO_train.ACC_MODE_SLEEPS_AND_SPINS, 23)
    except Exception as e:
        print (e);
    else:
        train2.TurnOnEndFunc (100)
        train2.DoTrains (200)
        train2.WaitOnBusy (100)
        del (train2)

    try:
        print ("ACC_MODE_SLEEPS_AND_OR_SPINS")
        train3 = PT_Py_GPIO_train (PT_Py_GPIO_train.INIT_FREQ, 10000, 0.01, 0.1, PT_Py_GPIO_train.ACC_MODE_SLEEPS_AND_OR_SPINS, 23)
    except Exception as e:
        print (e);
    else:
        train3.TurnOnEndFunc (100)
        train3.DoTrains (200)
        train3.WaitOnBusy (100)
        del (train3)
        print ("Trains finished")
