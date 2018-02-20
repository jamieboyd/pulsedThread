#include <pyPulsedThread.h>
/**********************************************************************************************************************************
These 2 functions make a pulsedThread object that runs Python callbacks for the high and low functions from a Python object 
Because the only data is a single pointer to a Python object, no need for an initFunc */
	

// Makes a pulsedThread that runs a Python object's HiFunc and LoFunc, initialized with arguments for pulse delay/duration and number of pulses
static PyObject* pulsedThreadPy_p (PyObject *self, PyObject *args) {
	// parse arguments from Python
	PyObject *PyObjPtr;		// first argument is a Python object that better have a HiFunc and a LoFunc
	unsigned int lowTicks ;			// low time in microseconds
	unsigned int highTicks ;		// high time in microseconds
	unsigned int nPulses;
	int accLevel;			// accuracy level, 0,1,2 as usual
	if (!PyArg_ParseTuple(args,"Oiiii", &PyObjPtr, &lowTicks, &highTicks, &nPulses, &accLevel)) {
		PyErr_SetString (PyExc_RuntimeError, "Could not parse input for Python object pointer, low ticks, high ticks, number of pulses, and timing method.");
		return NULL;
	}
	// make the pulsed thread with PyObjPtr as the Init Data, no INIT function (just copy over the PyObjPtr to taskData), and pulsedThread_RunPythonLoFunc and pulsedThread_RunPythonLoFunc as functions to run
	int errCode =0;
	pulsedThread * threadObj = new pulsedThread ((unsigned int)lowTicks, (unsigned int) highTicks, (unsigned int) nPulses, (void *) PyObjPtr, nullptr, &pulsedThread_RunPythonLoFunc, &pulsedThread_RunPythonHiFunc, accLevel, errCode);
	if (errCode){
		PyErr_SetString (PyExc_RuntimeError, "Could not make a new pulsedThread object.");
		return NULL;
	}
	return PyCapsule_New (static_cast <void *>(threadObj), "pulsedThread", pulsedThread_del);
}

// pulsedThread ( , int (*)(void *, void *  &), void (*)(void *), void (*)(void *), int , int &)

// Makes a pulsedThread that runs a Python object's HiFunc and LoFunc, with arguments for frequency, duty-cycle, and train duration
static PyObject* pulsedThreadPy_f (PyObject *self, PyObject *args) {
	// parse arguments from Python
	PyObject *PyObjPtr;		// first argument is a Python object that better have a HiFunc and a LoFunc
	float frequency;		// frequency in HZ
	float dutyCycle;		// Duty-cycle (0-1)
	float trainDur;			//train duration in seconds
	int accLevel;			// accuracy level, 0,1,2 as usual
	if (!PyArg_ParseTuple(args,"Offfi", &PyObjPtr, &frequency, &dutyCycle, &trainDur, &accLevel)) {
		PyErr_SetString (PyExc_RuntimeError, "Could not parse input for Python object pointer, frequency, duty cycle, train duration, and timing method.");
		return NULL;
	}
	// make the pulsed thread with PyObjPtr as the Init Data, no INIT function (just copy over the PyObjPtr to taskCustomData), and pulsedThread_RunPythonLoFunc and pulsedThread_RunPythonLoFunc as functions to run
	int errCode =0;
	pulsedThread * threadObj = new pulsedThread (frequency, dutyCycle, trainDur, (void *) PyObjPtr, nullptr, &pulsedThread_RunPythonLoFunc, &pulsedThread_RunPythonHiFunc, accLevel, errCode);
	if (errCode){
		PyRun_SimpleString ("print (' error making pulsedThread object')");
		return NULL;
	}
	return PyCapsule_New (static_cast <void *>(threadObj), "pulsedThread", pulsedThread_del);
}

  /* Module method table - the first 25 methods are defined in pyPulsedThread.h*/
static PyMethodDef ptPyFuncsMethods[]= {	
	{"isBusy", pulsedThread_isBusy, METH_O, "(PyCapsule) returns number of tasks a thread has left to do, 0 means finished all tasks"},
	//{"waitOnBusy", pulsedThread_waitOnBusy, METH_VARARGS, " (PyCapsule, timeOutSecs) Returns when a thread is no longer busy, or after timeOutSecs"},
	{"doTask", pulsedThread_doTask, METH_O, "(PyCapsule) Tells the pulsedThread object to do whatever task it was configured for"},
	{"doTasks", pulsedThread_doTasks, METH_VARARGS, "(PyCapsule, nTasks) Tells the pulsedThread object to do whatever task it was configured for nTasks times"},
	{"unDoTasks", pulsedThread_unDoTasks, METH_O, "(PyCapsule) Tells the pulsedThread object to stop doing however many task it was asked to do"},
	{"startTrain", pulsedThread_startTrain, METH_O, "(PyCapsule) Tells a pulsedThread object configured as an infinite train to start"},
	{"stopTrain", pulsedThread_stopTrain, METH_O, "(PyCapsule) Tells a pulsedThread object configured as an infinite train to stop"},
	{"modDelay", pulsedThread_modDelay, METH_VARARGS, "(PyCapsule, newDelaySecs) changes the delay period of a pulse or LOW period of a train"},
	{"modDur", pulsedThread_modDur, METH_VARARGS, "(PyCapsule, newDurationSecs) changes the delay period of a pulse or HIGH period of a train"},
	{"modTrainLength", pulsedThread_modTrainLength, METH_VARARGS, "(PyCapsule, newTrainLength) changes the number of pulses of a train"},
	{"modTrainDur", pulsedThread_modTrainDur, METH_VARARGS, "(PyCapsule, newTrainDurSecs) changes the total duration of a train"},
	{"modTrainFreq", pulsedThread_modFreq, METH_VARARGS, "(PyCapsule, newTrainFequency) changes the frequency of a train"},
	{"modTrainDuty", pulsedThread_modDutyCycle, METH_VARARGS, "(PyCapsule, newTrainDutyCycle) changes the duty cycle of a train"},
	{"getPulseDelay", pulsedThread_getPulseDelay, METH_O, "(PyCapsule) returns pulse delay, in seconds"},
	{"getPulseDuration", pulsedThread_getPulseDuration, METH_O, "(PyCapsule) returns pulse duration, in seconds"},
	{"getPulseNumber", pulsedThread_getPulseNumber, METH_O, "(PyCapsule) returns number of pulses in a train, 1 for a single pulse, or 0 for an infinite train"},
	{"getTrainDuration", pulsedThread_getTrainDuration, METH_O, "(PyCapsule) returns duration of a train, in seconds"},
	{"getTrainFrequency", pulsedThread_getTrainFrequency, METH_O, "(PyCapsule) returns frequency of a train, in Hz"},
	{"getTrainDutyCycle", pulsedThread_getTrainDutyCycle, METH_O, "(PyCapsule) returns duty cycle of a train, between 0 and 1"},
	{"unsetEndFunc", pulsedThread_UnSetEndFunc, METH_O, "(PyCapsule) un-sets any end function set for this pulsed thread"},
	{"hasEndFunc", pulsedThread_hasEndFunc, METH_O, "(PyCapsule) Returns the endFunc status (installed or not installed) for this pulsed thread"},
	{"setEndFuncObj", pulsedThread_SetPythonEndFuncObj, METH_VARARGS, "(PyCapsule, PythonObj, int dataMode) sets a Python object to provide endFunction for pulsedThread"},
	{"setTaskFuncObj", pulsedThread_SetPythonTaskObj, METH_VARARGS, "(PyCapsule, PythonObj) sets a Python object to provide LoFunc and HiFunc for pulsedThread"},
	{"setArrayEndFunc", pulsedThread_setArrayFunc, METH_VARARGS, "(PyCapsule, Python float array, endFuncType, isLocking) sets pulsedThread endFunc to set frequency (type 0) or duty cycle (type 1) from a Python float array"},
	{"cosDutyCycleArray", pulsedThread_cosineDutyCycleArray, METH_VARARGS, "(Python float array, pointsPerCycle, offset, scaling) fills passed-in array with cosine values of given period, with applied scaling and offset expected to range between 0 and 1"},
	
	{"initByPulse", pulsedThreadPy_p, METH_VARARGS, "Returns a new pulsedThread object that calls your objects HiFunc and LoFunc methods"},
	{"initByFreq", pulsedThreadPy_f, METH_VARARGS, "Returns a new pulsedThread object that calls your objects HiFunc and LoFunc methods"},
	{ NULL, NULL, 0, NULL}
  };
  
  /* Module structure */
  static struct PyModuleDef ptPyFuncsModule= {
    PyModuleDef_HEAD_INIT,
    "ptPyFuncs",           /* name of module */
    "An external module using the pulsedThreads library that calls Python methods in a pulsed fashion",  /* Doc string (may be NULL) */
    -1,                 /* Size of per-interpreter state or -1 */
    ptPyFuncsMethods       /* Method table */
  };

  /* Module initialization function */
  PyMODINIT_FUNC
  PyInit_ptPyFuncs(void) {
    return PyModule_Create(&ptPyFuncsModule);
  } 
