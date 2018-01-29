#! /usr/bin/python
#-*-coding: utf-8 -*-

import ptPyFuncs

class PT_Py_Greeter (object):

    INIT_PULSES =1
    INIT_FREQ=2
    ACC_MODE_SLEEPS = 0
    ACC_MODE_SLEEPS_AND_SPINS =1
    ACC_MODE_SLEEPS_AND_OR_SPINS =2
    END_FUNC_FREQ_MODE =0
    END_FUNC_PULSE_MODE =1
    
    def __init__ (self, initMode, pulseDelayOrTrainFreq, pulseDurationOrTrainDutyCycle, nPulsesOrTrainDuration, accuracy_level, p_name):
        self.name = p_name
        self.nTimes = 0
        if initMode == PT_Py_Greeter.INIT_FREQ:
            self.task_ptr = ptPyFuncs.initByFreq (self, float (pulseDelayOrTrainFreq), float (pulseDurationOrTrainDutyCycle), float(nPulsesOrTrainDuration), accuracy_level)
        else:
            self.task_ptr = ptPyFuncs.initByPulse (self, int (pulseDelayOrTrainFreq*1E6), int (pulseDurationOrTrainDutyCycle * 1E6), int(nPulsesOrTrainDuration), accuracy_level)


    def HiFunc(self):
        if self.nTimes == 0:
            print ("Hello from ", self.name)
        else:
            print ("Hello for the ", self.nTimes + 1, "th time from ", self.name)


    def LoFunc(self):
        if self.nTimes == 0:
            print ("Gooodbye from ", self.name)
        else:
            print ("Goodbye for the ", self.nTimes + 1, "th time from ", self.name)
        self.nTimes +=1

    def EndFunc (self, pulseDelayUsecs, pulseDurUsecs, nPulses, doTask):
        print ("At the end of this train, do Task = ", doTask)

    def TurnOnEndFunc (self, mode):
        ptPyFuncs.setEndFunc(self.task_ptr, mode)
        
    def greet(self):
        ptPyFuncs.doTask(self.task_ptr)

    def greet_many (self, n_trains):
        ptPyFuncs.doTasks(self.task_ptr, n_trains)

    def get_goodbye_time (self):
        return ptPyFuncs.getPulseDelay(self.task_ptr)

    def get_hello_time (self):
        return ptPyFuncs.getPulseDuration (self.task_ptr)

    def get_num_greets (self):
        return ptPyFuncs.getPulseNumber (self.task_ptr)

    def set_num_greets (self, numGreets):
        ptPyFuncs.modTrainLength (self.task_ptr, numGreets)

    def set_goodbye_time (self, goodbyeSecs):
        ptPyFuncs.modDelay (self.task_ptr, goodbyeSecs)

    def set_hello_time (self, helloSecs):
        ptPyFuncs.modDur (self.task_ptr, helloSecs)
        
    def is_greeting (self):
        return ptPyFuncs.isBusy (self.task_ptr)
    
    def wait_greeting (self, delaySecs):
        return ptPyFuncs.waitOnBusy (self.task_ptr, delaySecs)  

if __name__ == '__main__':
    pyGreeter = PT_Py_Greeter (PT_Py_Greeter.INIT_PULSES, 1, 1.75, 3, PT_Py_Greeter.ACC_MODE_SLEEPS, "Python functions run from C++")
    print ("hello time is ",  pyGreeter.get_hello_time(), 'seconds')
    print ("goodbye time is ",  pyGreeter.get_goodbye_time(), 'seconds')
    pyGreeter.TurnOnEndFunc (PT_Py_Greeter.END_FUNC_PULSE_MODE)
    pyGreeter.greet_many (3)
     
