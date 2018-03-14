#! /usr/bin/python
#-*-coding: utf-8 -*-

import ptGreeter

"""
    PTGreeter demonstrates making a wrapper providing a "Pythonic" object interface for an
    external module based on the C++ pulsedThread library , in this case ptGreeter, which uses a
    pulsedThread to print messages to the screen in a independently timed fashion.
    
"""

class PT_Greeter (object):
    """
    PTGreeter defines constants for accuracy level for timing:
    ACC_MODE_SLEEPS relies solely on sleeping for timing, which may not be accurate beyond the ms scale
    ACC_MODE_SLEEPS_AND_SPINS sleeps for first part of a time period, but wakes early and spins in a tight loop
    until the the end of the period, checking each time through the loop against calculated end time.
    It is more processor intensive, but much more accurate. How soon before the end of the period the thread
    wakes is set by the constant kSLEEPTURNAROUND in gpioThread.h. You need to re-run setup if it is changed.
    ACC_MODE_SLEEPS_AND_OR_SPINS checks the time before sleeping, so if thread is delayed for some reason, the sleeping is
    countermanded for that time period, and the thread goes straight to spinning, or even skips spinning entirely
    """
    ACC_MODE_SLEEPS = 0
    ACC_MODE_SLEEPS_AND_SPINS =1
    ACC_MODE_SLEEPS_AND_OR_SPINS =2

    def __init__ (self, accuracy_level, message):
        self.task_ptr = ptGreeter.pulsed_C_Greeter(accuracy_level,message)

    def greet(self):
        ptGreeter.doTask(self.task_ptr)

    def greet_many (self, n_trains):
        ptGreeter.doTasks(self.task_ptr, nTrains)

    def get_goodbye_time (self):
        return ptGreeter.getPulseDelay(self.task_ptr)

    def get_hello_time (self):
        return ptGreeter.getPulseDuration (self.task_ptr)

    def get_num_greets (self):
        return ptGreeter.getPulseNumber (self.task_ptr)

    def set_num_greets (self, numGreets):
        ptGreeter.modTrainLength (self.task_ptr, numGreets)

    def set_goodbye_time (self, goodbyeSecs):
        ptGreeter.modDelay (self.task_ptr, goodbyeSecs)

    def set_hello_time (self, helloSecs):
        ptGreeter.modDur (self.task_ptr, helloSecs)
        
    def is_greeting (self):
        return ptGreeter.isBusy (self.task_ptr)
    
    def wait_greeting (self, delaySecs):
        return ptGreeter.waitOnBusy (self.task_ptr, delaySecs)  
 
if __name__ == '__main__':
    try:
        myGreeter =  PT_Greeter (PT_Greeter.ACC_MODE_SLEEPS_AND_SPINS, "C++ python interfacing")
    except Exception as e:
        print (e)
        quit()
    #print ("Default number of greetings = ", myGreeter.get_num_greets ())
    #print ("Default hello time is ",  myGreeter.get_hello_time(), ' seconds')
    print ("Default goodbye time is ",  myGreeter.get_goodbye_time(), ' seconds')
    myGreeter.set_hello_time (0.25)
    myGreeter.set_goodbye_time (1)
    myOtherGreeter =  PT_Greeter (PT_Greeter.ACC_MODE_SLEEPS, "the other greeter")
    myOtherGreeter.set_hello_time (1)
    myOtherGreeter.set_goodbye_time (2)
    myOtherGreeter.set_num_greets (5)
    myGreeter.set_num_greets (11)
    myGreeter.greet()
    myOtherGreeter.greet()

    result =0
    while (myGreeter.is_greeting () > 0 and myOtherGreeter.is_greeting () > 0):
        for i in range (1,10000):
            result += (i*i/(i + 1))
        #print ('result = ', result)
        
                        
    print ('Greeters are finished now. Final result was', result)
        
