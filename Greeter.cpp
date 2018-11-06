#include <stdio.h>
#include <stdlib.h>
#include <pulsedThread.h>
#include "Greeter.h"

/*************************************Initialization function*******************************************************
This function initializes the custom data the thread will need. Previously, we made and filled a ptTestStruct
structure initDataP and passed it to the pulsedThread construtor  (in main).
Here we make an initialization */
int ptTest_Init (void * initDataP, void * &taskDataP){

	// initDataP is a pointer to our custom data structure, so cast it to that
	ptTestStructPtr initData = (ptTestStructPtr) initDataP;
	// task data pointer is a void pointer that points to the taskCustomData field in the taskParams struct
	// It needs to be initialized to a pointer to ptTestStruct 
	taskDataP = new ptTestStruct;
	// make a local ptTestStructPtr  so we don't have to keep casting taskDataP each time we use it
	ptTestStructPtr taskCustomData  =(ptTestStructPtr) taskDataP;
	// copy over name from initialization struct
	int iPos;
	for (iPos=0; iPos < 255 && initData->name[iPos] != 0; iPos+=1){
		taskCustomData->name[iPos] = initData->name[iPos] ;
	}
	taskCustomData->name[iPos] =0;
	// set times to 0  at start
	taskCustomData->times = 0;
	return 0;
}

/****************************************High and Low functions*********************************************************/
void ptTest_Hi (void * taskData){
// cast task data to our custom struct
	ptTestStructPtr ourData = (ptTestStructPtr) taskData;
	// Print
	if (ourData->times ==0){
		printf ("Hello  from %s\n",  ourData->name);
	}else{
		printf ("Hello again for the %dth time from %s\n",  ourData->times + 1, ourData->name);
	}
}

void ptTest_Lo (void * taskDataP){
	// cast task data to our custom struct
	ptTestStructPtr ourData = (ptTestStructPtr) taskDataP;
	// Print
	if (ourData->times ==0){
		printf ("goodbye  from %s\n",  ourData->name);
	}else{
		printf ("goodbye again for the %dth time from %s\n", ourData->times + 1, ourData->name);
	}
	// only for the low, increment times
	ourData->times +=1;
}

// makes a pulsedThread to say hello and goodbye.
int main(int argc, char **argv){
	// make a ptTestStruct to use for iniitialization
	ptTestStruct initStruct;
	initStruct.name [0] = 'P';
	initStruct.name [1] = 'u';
	initStruct.name [2] = 'l';
	initStruct.name [3] = 's';
	initStruct.name [4] = 'e';
	initStruct.name [5] = 'd';
	initStruct.name [6] = ' ';
	initStruct.name [7] = 'T';
	initStruct.name [8] = 'h';
	initStruct.name [9] = 'r';
	initStruct.name [10] = 'e';
	initStruct.name [11] = 'a';
	initStruct.name [12] = 'd';
	initStruct.name [13] = 0;
	// make a pulsed thread with our pttestStruct and our Hi and Lo functions. USe microsecond ddelay, microsecond duration, and number of pulses method
	int errVar;
	pulsedThread * train1 = new pulsedThread ((unsigned int)50000, (unsigned int)50000, (unsigned int)10, (void *) &initStruct, &ptTest_Init, &ptTest_Lo, &ptTest_Hi, ACC_MODE_SLEEPS, errVar);
	if (errVar){
		printf ("Failed to make pulsed thread.\n");
		return 0;
	}
	// let's make another train ,running half the speed of the first one, with a different name 
	// because we copy the array into the custom data sruct, and not just pass a pointer to the array, we can reuse the same array to make second pulsedThread
	initStruct.name [13] = ' ';
	initStruct.name [14] = 'n';
	initStruct.name [15] = 'u';
	initStruct.name [16] = 'm';
	initStruct.name [17] = 'b';
	initStruct.name [18] = 'e';
	initStruct.name [19] = 'r';
	initStruct.name [20] = ' ';
	initStruct.name [21] = 't';
	initStruct.name [22] = 'w';
	initStruct.name [23] = 'o';
	initStruct.name [24] = 0;
	pulsedThread * train2 = new pulsedThread ((unsigned int)100000, (unsigned int)100000, (unsigned int)5, (void *) &initStruct, &ptTest_Init, &ptTest_Lo, &ptTest_Hi, ACC_MODE_SLEEPS, errVar);
	if (errVar){
		printf ("Failed to make second pulsed thread.\n");
		return 0;
	}
	train1->DoTask ();// a train of 10 times should be enough, so just do it once
	train2->DoOrUndoTasks (1);
	// show that the train is running on its own
	printf ("Trains were started and should be printing in a second\n");
	// a loop to show we can do real work in the main thread while the pulsedThreads do their own things.
	double aNum=0;
	while (train1->isBusy() && train2->isBusy()){
		for (float i=0;i<100000;i+=1){
			aNum += (i*i)/(i+1);
		}
		printf ("Value of busy-work variable calculated to be %.12f\n", aNum);
	}
	printf ("Threads are done printing and so am I\n");
	return 0;
}
	
