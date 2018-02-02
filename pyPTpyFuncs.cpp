#include <pyPulsedThread.h>

/* --------- A pulsedThread where HI, LO, and EndFuncs are run from the same Python Object with HiFunc, LoFunc, and (possibly) endFunc methods--------------
Initialized with a a Python object that has HiFunc, LoFunc, and (possibly) endFunc methods */

// Runs PyObject.HiFunc()  taskCustomData is assumed to be a pointer to a python object that has a method called HiFunc that takes no arguments
static void pulsedThread_RunPythonHiFunc (void * taskCustomData){
	PyObject *PyObjPtr = (PyObject *) taskCustomData;
	PyGILState_STATE state=PyGILState_Ensure();
	PyObject *result = PyObject_CallMethod (PyObjPtr, "HiFunc", NULL);
	Py_DECREF (result);
	PyGILState_Release(state);
}

// Runs PyObject.LoFunc()  taskCustomData is assumed to be a pointer to a python object that has a method called LoFunc that takes no arguments
static void pulsedThread_RunPythonLoFunc (void * taskCustomData){
	PyObject *PyObjPtr = (PyObject *) taskCustomData;
	PyGILState_STATE state=PyGILState_Ensure();
	PyObject *result = PyObject_CallMethod (PyObjPtr, "LoFunc", NULL);
	Py_DECREF (result);
	PyGILState_Release(state);
}

// Runs PyObject.endFunc()  taskCustomData is assumed to be a pointer to a python object that has a method called endFunc that takes 4 int arguments 
static void pulsedThread_RunPythonEndFunc_p (taskParams * theTask){
	PyObject *PyObjPtr = (PyObject *) theTask->taskCustomData;
	PyGILState_STATE state=PyGILState_Ensure();
	PyObject *result = PyObject_CallMethod (PyObjPtr, "EndFunc", "(iiii)",theTask->pulseDelayUsecs, theTask->pulseDurUsecs, theTask->nPulses, theTask->doTask);
	Py_DECREF (result);
	PyGILState_Release(state);
}

// Runs PyObject.endFunc(frequency, dutyCycle, train Duration, doTask)  taskCustomData is assumed to be a pointer to a python object that has a method called EndFunc that takes 3 float arguments and 1 int arguments
static void pulsedThread_RunPythonEndFunc_f (taskParams * theTask){
	PyObject *PyObjPtr = (PyObject *) theTask->taskCustomData;
	PyGILState_STATE state=PyGILState_Ensure();
	PyObject *result = PyObject_CallMethod (PyObjPtr, "EndFunc", "(fffi)",theTask->trainFrequency, theTask->trainDutyCycle, theTask->trainDuration, theTask->doTask);
	Py_DECREF (result);
	PyGILState_Release(state);
}

// installs pulsedThread_RunPythonEndFunc, either interger or float version,  as the endFunc, calling the endFunc of the same Python object used for HI and LO funcs
static PyObject* pulsedThread_SetPythonEndFunc (PyObject *self, PyObject *args) {
	PyObject *PyPtr;		// first argument is the Python pyCapsule that points to the pulsedThread
	int endFuncPulseDesc;	// 0 for calling endFunc with frequency,duration,train length info, non-zero for microseond pulse delay, duration, and pulse number description
	if (!PyArg_ParseTuple(args,"Oi", &PyPtr, &endFuncPulseDesc)) {
		PyRun_SimpleString ("print (' error')");
		return NULL;
	}
	pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));	
	if (endFuncPulseDesc == 0){
		threadPtr->setEndFunc (&pulsedThread_RunPythonEndFunc_f);
	}else{
		threadPtr->setEndFunc (&pulsedThread_RunPythonEndFunc_p);
	}
	Py_RETURN_NONE;
}


// the modFunc that is passed to pulsedThread->modCustom so we change the object when thread is not busy
int pulsedThread_modObj (void * pyObjPtr, taskParams * taskDataP){
	taskDataP->taskCustomData = pyObjPtr;
	return 0;
}

// changes the object that is used when calling LO func, HI func, and ENDfunc
static PyObject* pulsedThread_SetPythonFuncObj (PyObject *self, PyObject *args) {
	PyObject *PyPtr;		// first argument is the Python pyCapsule that points to the pulsedThread
	PyObject *PyObjPtr;		// second argument is a Python object that better have HI, LO, and endFunc 
	if (!PyArg_ParseTuple(args,"OO", &PyPtr, &PyObjPtr)) {
		PyRun_SimpleString ("print (' error')");
		return NULL;
	}
	pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));	
	// set the endFunc data to the object
	threadPtr->modCustom (&pulsedThread_modObj, (void *) PyObjPtr, 1);
	Py_RETURN_NONE;
}

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
		PyRun_SimpleString ("print (' error')");
		return NULL;
	}
	// make the pulsed thread with PyObjPtr as the Init Data, no INIT function (just copy over the PyObjPtr to taskCustomData), and pulsedThread_RunPythonLoFunc and pulsedThread_RunPythonLoFunc as functions to run
	int errCode =0;
	pulsedThread * threadObj = new pulsedThread (lowTicks, highTicks, nPulses, (void *) PyObjPtr, nullptr, &pulsedThread_RunPythonLoFunc, &pulsedThread_RunPythonHiFunc, accLevel, errCode);
	if (errCode){
		PyRun_SimpleString ("print (' error')");
		return NULL;
	}
	// Activate Python Thread Awareness
	if (!PyEval_ThreadsInitialized()){
		PyEval_InitThreads();
	}
	return PyCapsule_New (static_cast <void *>(threadObj), "pulsedThread", pulsedThread_del);
}

// Makes a pulsedThread that runs a Python object's HiFunc and LoFunc, with arguments for frequency, duty-cycle, and train duration
static PyObject* pulsedThreadPy_f (PyObject *self, PyObject *args) {
	// parse arguments from Python
	PyObject *PyObjPtr;		// first argument is a Python object that better have a HiFunc and a LoFunc
	float frequency;		// frequency in HZ
	float dutyCycle;		// Duty-cycle (0-1)
	float trainDur;			//train duration in seconds
	int accLevel;			// accuracy level, 0,1,2 as usual
	if (!PyArg_ParseTuple(args,"Offf", &PyObjPtr, &frequency, &dutyCycle, &trainDur, &accLevel)) {
		PyRun_SimpleString ("print (' error')");
		return NULL;
	}
	// make the pulsed thread with PyObjPtr as the Init Data, no INIT function (just copy over the PyObjPtr to taskCustomData), and pulsedThread_RunPythonLoFunc and pulsedThread_RunPythonLoFunc as functions to run
	int errCode =0;
	pulsedThread * threadObj = new pulsedThread (frequency, dutyCycle, trainDur, (void *) PyObjPtr, nullptr, &pulsedThread_RunPythonLoFunc, &pulsedThread_RunPythonHiFunc, accLevel, errCode);
	if (errCode){
		PyRun_SimpleString ("print (' error')");
		return NULL;
	}
	// Activate Python Thread Awareness
	if (!PyEval_ThreadsInitialized()){
		PyEval_InitThreads();
	}
	return PyCapsule_New (static_cast <void *>(threadObj), "pulsedThread", pulsedThread_del);
}

  /* Module method table - the first 20 methods are defined in pyPulsedThread.h*/
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

	{"setFuntionObject", pulsedThread_SetPythonFuncObj, METH_VARARGS, "sets the Python object whose HiFunc, LoFunc, and endFunc are called form the pulsedThread"},
	{"initByPulse", pulsedThreadPy_p, METH_VARARGS, "Returns a new pulsedThread object that calls your objects HiFunc and LoFunc methods"},
	{"initByFreq", pulsedThreadPy_f, METH_VARARGS, "Returns a new pulsedThread object that calls your objects HiFunc and LoFunc methods"},
	{"setEndFunc", pulsedThread_SetPythonEndFunc, METH_VARARGS, "Sets the pulsed thread's end function to your object's endFunc method, getting either pulse time or frequency information"},

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
