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
		PyRun_SimpleString ("print (' error Parsing Input')");
		return NULL;
	}
	// make the pulsed thread with PyObjPtr as the Init Data, no INIT function (just copy over the PyObjPtr to taskData), and pulsedThread_RunPythonLoFunc and pulsedThread_RunPythonLoFunc as functions to run
	int errCode =0;
	pulsedThread * threadObj = new pulsedThread ((unsigned int)lowTicks, (unsigned int) highTicks, (unsigned int) nPulses, (void *) PyObjPtr, nullptr, &pulsedThread_RunPythonLoFunc, &pulsedThread_RunPythonHiFunc, accLevel, errCode);
	if (errCode){
		PyRun_SimpleString ("print (' error making pulsedThread object')");
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
		PyRun_SimpleString ("print (' error parsing input')");
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

  /* Module method table - the first 22 methods are defined in pyPulsedThread.h*/
static PyMethodDef ptPyFuncsMethods[]= {	
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
	{"setTaskObject", pulsedThread_SetPythonTaskObj, METH_VARARGS, "sets a Python object whose LoFunc and HiFunc are called from the pulsedThread"},
	{"setEndFuntionObject", pulsedThread_SetPythonEndFuncObj, METH_VARARGS, "sets a Python object whose endFunc is called from the pulsedThread, getting either pulse time or frequency information"},

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
