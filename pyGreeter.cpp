#include <pyPulsedThread.h>
#include "Greeter.h"

/************This code describes a simple C++-Python module that uses the pulsedThread library**************

To use pulsedThread C++ library in a Python module, we use the wrapper functions provided in pyPulsedThread.h
The functions in pyPulsedThread.h are designed to work with a pre-made pulsedThread object on the C++ side
of things and a PyCapsule object containing a pointer to that pulsedThread object on the Python side of things. 

As in using pulsedThread directly from C++, we must supply at the minimum C++ functions for initialization of
the taskCustom data and for the HI and LO events. To use pyPulsedThread.h in a Python C++ module, we must also 
provide a Python C++ module function that makes a new pulsedThread using those intialization and HI and Lo 
functions and returns to Python a PyCapsule that wraps a pointer to the pulsedThread object. 

For the C++ side of things, we include the header file Greeter.h and use the same  functions, ptTest_Init, ptTest_Hi, 
and ptTest_Lo, that we used in  Greeter.cpp . So to make the Python C++ module version, we just need to provide
a function to make a pulsedThread object and return a pyCapsule containing a pointer to it. 
The pulsedThread library, the functions in pyPulsedThread.h, and in Greeter.h/cpp plus the initialization
function in this file are compiled together with the provided Python install script, setup.py.
*******************************************************************************************************************************/



/******************************C++ Code that is called from Python - forming the interface to the Python Module*****************
This is where you put functions to create and interact with the C++ pulsedThread object, and also use function from Python.h to 
create, return to Python, and interact with Python objects. All we need to do is create a pulsedThread and  we can
use the function in pyPulsedThread.h to run it */
/* makes a pulsedThread object and returns to Python a pyCapsule containing a pointer to the pulsedThread*/
static PyObject* pulsed_C_Greeter (PyObject *self, PyObject *args) {
	// parameters passed to this function from Python are a string for the name, and integer for timing mode
	const char * localName;
	int accLevel;
	// parse the arguments and copy into local variables
	if (!PyArg_ParseTuple(args,"is",  &accLevel, &localName)) {
		return NULL;
	}
	// make a ptTestStruct to use for iniitialization, with name from Python 
	ptTestStruct initStruct;
	int iPos;
	for (iPos=0; iPos < 255 && localName[iPos] != 0; iPos+=1){
		initStruct.name[iPos] = localName[iPos] ;
	}
	initStruct.name[iPos] =0;
	
	// make a pulsed thread with our pttestStruct and our Hi and Lo functions. USe microsecond delay, microsecond duration, and number of pulses method
	int errCode =0;
	pulsedThread * threadObj = new pulsedThread ((unsigned int)50000, (unsigned int)50000, (unsigned int)10, (void * volatile) &initStruct, &ptTest_Init, &ptTest_Lo, &ptTest_Hi, accLevel, errCode);
	if (errCode){
		return NULL;
	}else{
		return PyCapsule_New (static_cast <void *>(threadObj), "pulsedThread", pulsedThread_del);
	}
  }
  
  
  /* Module method table -only pulsed_C_Greeter is defined here, the first 20 functions are defined in pyPulsedThread.h*/
static PyMethodDef ptGreeterMethods[] = {
  //Setting times and requesting tasks 
	
	{"isBusy", pulsedThread_isBusy, METH_O, "returns number of tasks a thread has left to do, 0 means finished all tasks"},
	{"waitOnBusy", pulsedThread_waitOnBusy, METH_VARARGS, "Returns when a thread is no longer busy, or after timeOut secs"},
	{"doTask", pulsedThread_doTask, METH_O, "Tells the pulsedThread object to do whatever task it was configured for"},
	{"doTasks", pulsedThread_doTasks, METH_VARARGS, "Tells the pulsedThread object to do whatever task it was configured for multiple times"},
	{"startTrain", pulsedThread_startTrain, METH_O, "Tells a pulsedThread object configured as an infinite train to start"},
	{"stopTrain", pulsedThread_stopTrain, METH_O, "Tells a pulsedThread object configured as an infinite train to stop"},
	{"modDelay", pulsedThread_modDelay, METH_VARARGS, "changes the delay period of a pulse or LOW period of a train"},
	{"modDur", pulsedThread_modDur, METH_VARARGS, "changes the delay period of a pulse or HIGH period of a train"},
	{"modTrainLength", pulsedThread_modTrainLength, METH_VARARGS, "changes the number of pulses of a train"},
	{"modTrainDur", pulsedThread_modTrainDur, METH_VARARGS, "changes the total duration of a train"},
	{"modTrainFreq", pulsedThread_modFreq, METH_VARARGS, "changes the frequency of a train"},
	{"modTrainDuty", pulsedThread_modDutyCycle, METH_VARARGS, "changes the duty cycle of a train"},
	{"getPulseDelay", pulsedThread_getPulseDelay, METH_O, "returns pulse delay, in seconds"},
	{"getPulseDuration", pulsedThread_getPulseDuration, METH_O, "returns pulse duration, in seconds"},
	{"getPulseNumber", pulsedThread_getPulseNumber, METH_O, "returns number of pulses in a train, 1 for a single pulse, or 0 for an infinite train"},
	{"getTrainDuration", pulsedThread_getTrainDuration, METH_O, "returns duration of a train, in seconds"},
	{"getTrainFrequency", pulsedThread_getTrainFrequency, METH_O, "returns frequency of a train, in Hz"},
	{"getTrainDutyCycle", pulsedThread_getTrainDutyCycle, METH_O, "returns duty of a train, between 0 and 1"},
	{"unsetEndFunc", pulsedThread_UnSetEndFunc, METH_O, "un-sets any end function set for this pulsed thread"},
	{"hasEndFunc", pulsedThread_hasEndFunc, METH_O, "Returns the endFunc status (installed or not installed) for this pulsed thread"},

	{"pulsed_C_Greeter", pulsed_C_Greeter, METH_VARARGS, "Creates and configures new pulsedThread object to say hello and goodbye on the C side of things"},
	{ NULL, NULL, 0, NULL}
  };
  
  /* Module structure */
  static struct PyModuleDef ptGreetermodule = {
    PyModuleDef_HEAD_INIT,
    "ptGreeter",           /* name of module */
    "An external module for demonstrating use of pulsedThreads library with a Python C-module",  /* Doc string (may be NULL) */
    -1,                 /* Size of per-interpreter state or -1 */
    ptGreeterMethods       /* Method table */
  };

  /* Module initialization function */
  PyMODINIT_FUNC
  PyInit_ptGreeter(void) {
    return PyModule_Create(&ptGreetermodule);
  } 
 