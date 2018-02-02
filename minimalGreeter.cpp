#include <stdio.h>
#include <stdlib.h>
#include <pulsedThread.h>

/****************************************High function*********************************************************/
void ptTest_Hi (void * taskData){
	// cast task data to a character array.
	char* name = (char *) taskData;
	// Print hello 
	printf ("Hello  from %s\n",  name);
}

// makes a very minimal pulsedThread to say hello, with no lowFunc, no initFunc, and customData as a simple array, not a structure
int main(int argc, char **argv){
	// make a ptTestStruct to use for initialization
	char name [13];
	name [0] = 'P';
	name [1] = 'u';
	name [2] = 'l';
	name [3] = 's';
	name [4] = 'e';
	name [5] = 'd';
	name [6] = ' ';
	name [7] = 'T';
	name [8] = 'h';
	name [9] = 'r';
	name [10] = 'e';
	name [11] = 'a';
	name [12] = 'd';
	name [13] = 0;
	// make a pulsed thread with our Hi function but no lo func. Value for low time needs to be 0 for this. Use microsecond delay, microsecond duration, and number of pulses method
	int errVar;
	pulsedThread * train1 = new pulsedThread ((unsigned int)0, (unsigned int)5E5, (unsigned int)10, (void *) &name, nullptr, nullptr, &ptTest_Hi, ACC_MODE_SLEEPS, errVar);
	if (errVar){
		printf ("Failed to make pulsed thread.\n");
		return 1;
	}
	// ask the thread to do a train
	train1->DoTask ();// a train of 10 times should be enough, so just do it once
	// show that the train is running on its own
	printf ("Train was started and should be printing in a second\n");
	// a loop to show we can do real work in the main thread while the pulsedThreads do their own things.
	double aNum=0;
	while (train1->isBusy()){
		for (float i=0;i<100000;i+=1){
			aNum += (i*i)/(i+1);
		}
		printf ("Value of busy-work variable calculated to be %.12f\n", aNum);
	}
	printf ("Thread is done printing and so am I\n");
	return 0;
}
	
