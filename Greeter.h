#ifndef GREETER_H
#define GREETER_H

/*************************************Structure for custom data*******************************************************
we will use the same structure for initialization of the custom data as we use for the custom data itself, but that does not
need to be the case */
typedef struct ptTestStruct{
	char name [256];	//name to add to message to print 
	int times;			// track number of times we we have printed it (not used for INIT, only for HI,LO funcs)
}ptTestStruct, *ptTestStructPtr;

/*function declarations for the 3 needed functions, Initialization, Hi, and Lo */
int ptTest_Init (void * , void *&);
void ptTest_Hi (void *);
void ptTest_Lo (void * );

#endif