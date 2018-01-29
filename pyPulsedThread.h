#ifndef PYPULSEDTHREAD_H
#define PYPULSEDTHREAD_H
#include <Python.h>
#include <pulsedThread.h>

/*****************************************************************************************************************
pyPulsedThread is code you can use to to wrap the C++ pulsedThread class into a Python external module. 
Other modules that wish to include pulsedThread need to provide an initialization function that returns a
PyCapsule containing a pointer to a pulsedThread object. The functions in pyPulsedThread can then
interact with the PyCapsule object asking it to do tasks and configure pulse/train timing. The code
is in the header file so the static functions can be incorporated into a module.

see pyPulsedThread_SimpleGPIO for an example that controls single pulses and trains of pulses on raspberry pi GPIO pins.
******************************************************************************************************************/

/*Function called automatically when PyCapsule object is deleted in Python */
static void  pulsedThread_del(PyObject * PyPtr){
    delete static_cast<pulsedThread *> (PyCapsule_GetPointer (PyPtr, "pulsedThread"));
}

/*Checks if a pulsedThread thread is busy */
static PyObject* pulsedThread_isBusy (PyObject *self, PyObject *PyPtr) {
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
     return Py_BuildValue("i", threadPtr -> isBusy());
}

/*Waits on a pulsedThread thread (up to TmeOutSecs), returning 0 when it is no longer busy or 1 if TmeOutSecs elapsed*/
static PyObject* pulsedThread_waitOnBusy (PyObject *self, PyObject *args) {
	PyObject *PyPtr;
	float timeOutSecs;
	if (!PyArg_ParseTuple(args,"Of", &PyPtr, &timeOutSecs)) {
		return NULL;
	}
	pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
	return Py_BuildValue("i", threadPtr -> waitOnBusy(timeOutSecs));
}

/*pulsedThread_doTask tells the pulsedThread object to perform whatever task it was configured to do */
static PyObject* pulsedThread_doTask (PyObject *self, PyObject *PyPtr) {
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    threadPtr->DoTask();
    Py_RETURN_NONE;
}

  /*pulsedThread_doTasks tells the pulsedThread object to perform whatever task it was configured to do multiple times*/
static PyObject* pulsedThread_doTasks (PyObject *self, PyObject *args) {
	PyObject *PyPtr;
	int nTimes;
	if (!PyArg_ParseTuple(args,"Oi", &PyPtr, &nTimes)) {
		return NULL;
	}
	pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
	threadPtr->DoTasks(nTimes);
	Py_RETURN_NONE;
}


  /*pulsedThread_startTrain tells  pulsedThread object configured as an infinite train to start ticking*/
static PyObject* pulsedThread_startTrain (PyObject *self, PyObject *PyPtr) {
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    threadPtr->startInfiniteTrain();
    Py_RETURN_NONE;
}

 /*pulsedThread_stopTrain tells  pulsedThread object configured as an infinite train to stop ticking*/
static PyObject* pulsedThread_stopTrain (PyObject *self, PyObject *PyPtr) {
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    threadPtr->stopInfiniteTrain();
    Py_RETURN_NONE;
}


/* ---------Modifiers for pulse timing based on individual pulses and numbers of pulses--------------
Modifies the delay of a pulse, or the "low" time of a train, value is in seconds*/
static PyObject*  pulsedThread_modDelay (PyObject *self, PyObject *args) {
    PyObject *PyPtr;
    float newDelay;
    if (!PyArg_ParseTuple(args,"Of", &PyPtr, &newDelay)) {
        return NULL;
    }
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    return Py_BuildValue("i", threadPtr ->modDelay ((unsigned int) round (1e06 * newDelay)));
}

/* modifies the duration of a pulse, or the "high" time of a train, value is in seconds*/
static PyObject*  pulsedThread_modDur (PyObject *self, PyObject *args) {
    PyObject *PyPtr;
    float newDur;
    if (!PyArg_ParseTuple(args,"Of", &PyPtr, &newDur)) {
        return NULL;
    }
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    return Py_BuildValue("i", threadPtr ->modDur ((unsigned int) round (1e06 * newDur)));
}


/*modifies the number of pulses of a train */
static PyObject*  pulsedThread_modTrainLength(PyObject *self, PyObject *args) {
    PyObject *PyPtr;
    unsigned int newTrainLength;
    if (!PyArg_ParseTuple(args,"OI", &PyPtr, &newTrainLength)) {
        return NULL;
    }
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    return Py_BuildValue("i", threadPtr ->modTrainLength (newTrainLength));
}


/* ---------Modifiers for pulse timing based on train characteristics of train duration, frequency, and duty cycle-------------

modifies the total duration of a train, new duration is in seconds */
static PyObject*  pulsedThread_modTrainDur (PyObject *self, PyObject *args) {
    PyObject *PyPtr;
    float newDur;
    if (!PyArg_ParseTuple(args,"Of", &PyPtr, &newDur)) {
        return NULL;
    }
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    return Py_BuildValue("i", threadPtr ->modTrainDur (newDur));
}

 /*modifies the frequency of a train, new frequency is in Hz */
static PyObject*  pulsedThread_modFreq(PyObject *self, PyObject *args) {
    PyObject *PyPtr;
    float newFreq;
    if (!PyArg_ParseTuple(args,"Of", &PyPtr, &newFreq)) {
        return NULL;
    }
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    return Py_BuildValue("i", threadPtr ->modFreq (newFreq));
}

/* modifies the duty cycle of a train (ON time/(ON time + OFF time)) */
static PyObject*  pulsedThread_modDutyCycle (PyObject *self, PyObject *args) {
    PyObject *PyPtr;
    float newDutyCycle;
    if (!PyArg_ParseTuple(args,"Of", &PyPtr, &newDutyCycle)) {
        return NULL;
    }
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    return Py_BuildValue("i", threadPtr ->modDutyCycle (newDutyCycle));
}

/* ---------Getters for pulse timing based on individual pulses and numbers of pulses--------------*/
static PyObject* pulsedThread_getPulseDelay (PyObject *self, PyObject *PyPtr) {
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
     return Py_BuildValue("f", (float) 1e-06 * threadPtr -> getpulseDelayUsecs());
}

static PyObject* pulsedThread_getPulseDuration (PyObject *self, PyObject *PyPtr) {
	pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
	return Py_BuildValue("f", (float) 1e-06 * threadPtr -> getpulseDurUsecs());
}

static PyObject* pulsedThread_getPulseNumber (PyObject *self, PyObject *PyPtr) {
	pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    return Py_BuildValue("I", threadPtr -> getNpulses());
}

/* ---------Getters for pulse timing based on train duration, frequency, duty cycle--------------*/
static PyObject* pulsedThread_getTrainDuration (PyObject *self, PyObject *PyPtr) {
	pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    return Py_BuildValue("f", threadPtr -> getTrainDuration());
}

static PyObject* pulsedThread_getTrainFrequency (PyObject *self, PyObject *PyPtr) {
	pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    return Py_BuildValue("f", threadPtr -> getTrainFrequency());
}
static PyObject* pulsedThread_getTrainDutyCycle (PyObject *self, PyObject *PyPtr) {
    pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
    return Py_BuildValue("f", threadPtr -> getTrainDutyCycle());
}

/*-------------------------------------------------------EndFunc utilities--------------------------------------------------------------------------------------------
we can check if an endFunc is installed and un-install an end func without knowing anything about the endFunc */

// checks if this pulsed thread is set to run an endFunc
static PyObject* pulsedThread_hasEndFunc (PyObject *self, PyObject *PyPtr) {
	pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));
	return Py_BuildValue("i", threadPtr ->hasEndFunc ());
}

// un-installs any installed endFunc
static PyObject* pulsedThread_UnSetEndFunc (PyObject *self, PyObject *PyPtr) {
	pulsedThread * threadPtr = static_cast<pulsedThread * > (PyCapsule_GetPointer(PyPtr, "pulsedThread"));	
	threadPtr->unSetEndFunc ();
	Py_RETURN_NONE;
}

#endif