#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h> 
#include <stdlib.h> 
#include <windows.h> 
#include <time.h>

#define MIN_SLEEP_TIME 5 // 5 miliseconds.
#define MAX_SLEEP_TIME 3000 // 3 seconds.

#define MIN_WEIGHT 5 // min weight for cargo.
#define MAX_WEIGHT 50 // max weight for cargo.

#define BUFFER_SIZE 60 // Size of largest message to send/receive through pipes.
#define MAX_STRING 200 // Size of the larget string to send to the safe printf.

// Node of Queue
typedef struct Node_t {
	int vesselId;
	struct Node_t* prev;
} VesselNode;

// Queue for the Barrier, so nodes will leave FIFO.
typedef struct {
	VesselNode* head;
	VesselNode* tail;
	int size;
	int limit;
} VesselQueue;

// 1 to 1 relation between crane and vessel.
typedef struct {
	int craneId;
	int vesselId;
	int cargoWeight;
	int isOccupied;
} UnloadingQuayStation;

// Holds all unloading quay stations and the amount of them.
typedef struct {
	UnloadingQuayStation* unloadingQuayStation;
	int unloadingQuaySize;
} UnloadingQuayStruct;

// Functions which support handling a Queue.
VesselQueue* constructQueue(int limit);
void destructQueue(VesselQueue* UnloadingQuay);
int enqueue(VesselQueue* UnloadingQuay, int vesselId);
int dequeue(VesselQueue* UnloadingQuay);
int isEmpty(VesselQueue* UnloadingQuay);
// Functions which support handling UnloadingQuay.
UnloadingQuayStruct* constructUnloadingQuay(int cranesId[], int numberOfCranes);
void destructUnloadingQuay(UnloadingQuayStruct* pUnloadingQuay);
int isUnloadingQuayEmpty(UnloadingQuayStruct* pUnloadingQuay);
void removeVesselsFromUnloadingQuay(UnloadingQuayStruct* pUnloadingQuay);

// Random Functions:
// Thread safe rand().
int safeRand(void);
// Calculates sleep time according to the defined MIN_SLEEP_TIME and MAX_SLEEP_TIME.
int randomSleepTime(void);
// Calculates cargo weight according to the defined MIN_WEIGHT and MAX_WEIGHT.
int randomCargoWeight(void);

// Initialize and destruct all global Mutexes/Semaphores.
void initializeGlobalMutexAndSemaphores(int numberOfVessels, int numberOfCranes);
void cleanGlobalMutexAndSemaphores(int numberOfVessels, int numberOfCranes);

// Main thread functions:
// Read number of vessels from HaifaPort.
int getNumberOfVesselsFromHaifaPort(void);
// Processes whether the number of vessels is a prime number and according to that
// returns to HaifaPort its passage result.
void writeToHaifaPortPassageResult(int numberOfVessels);
// Returns TRUE or FALSE whether the number is a prime number or not.
int isPrimeNumber(int number);
// Returns a divisor which will operate as the number of cranes.
int getRandomDivisor(int dividendNumber);
// Create all crane threads according to the number given by the random divisor.
HANDLE* createCraneThreads(int numberOfCranes, int** cranesId);
// Create unloading quay thread and set its priority to be the highest.
void createUnloadingQuayThread(HANDLE* unloadingQuayHandler);
// "Listen" for vessels from HaifaPort and create their threads.
HANDLE* readAndCreateIncomingVesselsFromHaifaPort(int numberOfVessels, int** vesselsId);
// Check if all the vessels are done running in HaifaPort.
int areAllVesselsDoneatHaifaPort(void);
// Signal cranes semaphores to continue so they can reach the break point set by areAllVesselsDone.
void signalCranesToFinish(int numberOfCranes);
// Free vessels HANDLER and CloseHandle.
void freeVesselThreads(HANDLE* vesselsHandler, int* vesselsId, int numberOfVessels);
// Free cranes HANDLER and CloseHandle.
void freeCraneThreads(HANDLE* cranesHandler, int* cranesId, int numberOfCranes);
// CloseHandle for unloading quay and destruct both unloading quay and barrier.
void cleanUnloadingQuayAndBarrier(HANDLE* unloadingQuayHandler);
// Write to HaifaPort that EilatPort has cleaned all of its threads and it is exiting.
void writeToHaifaPortThatEilatPortIsDone(void);

// printf may be a thread safe function, though it isn't process safe and for that reason
// the function is protected by a semaphore that is shared between both HaifaPort and EilatPort.
int safePrintWithTimeStamp(char string[]);

// The thread functions for vessels, cranes and the unloading quay.
DWORD WINAPI Vessel(LPVOID Param);
DWORD WINAPI Crane(LPVOID Param);
DWORD WINAPI UnloadingQuay(LPVOID Param);

// These functions are pieces of the vessel thread:
int startSailingAndEnterBarrier(int vesselId, int vesselIndex);
int enterUnloadingQuayAndStartUnloadingProcess(int vesselId, int vesselIndex);
int stationVesselInUnloadingQuay(int vesselId);
int startUnloadingVessel(int vesselId, int vesselIndex, int stationIndex);
int exitUnloadingQuay(int vesselId, int stationIndex);
int sailToHaiafaPort(int vesselId);


// Queue which holds vessels that have reached the synchronization point.
VesselQueue* barrier; 

// Holds all relations between cranes and vessels.
UnloadingQuayStruct* unloadingQuay; 

// Struct for Date and Time. Fill in the struct with GetLocalTime().
SYSTEMTIME currentTime; 

// With this duo we are able to allow only 1 vessel at a time to be in the canal
HANDLE redToMedCanalSemaphore; // Mutex to allow only one vessel at a time to enter the canal (pipe).
HANDLE medToRedCanalSemaphore; // Mutex to allow only one vessel at a time to exit the canal (pipe).

// Semaphore/Mutex which allow us to control our threads.
HANDLE* vesselsSemaphores; // Semaphore for each Vessel to signal them when to wait and continue.
HANDLE* cranesSemaphores; // Semaphore for each Crane to signal them when to wait and continue.
HANDLE barrierSemaphore; // Semaphore which provides a synchronization point for the vessel threads.
HANDLE stationMutex; // Mutex to allow only one vessel at a time to enter unloading quay. 
HANDLE* unloadingQuaySemaphore; // Semaphore the size of unloading quay, which waits upon all vessels to leave. 

// Mutex to make rand() thread safe.
HANDLE randomMutex; 

// The reasoning behind the semaphore is to prevent race conditions.
// printf is a thread safe function, although it isn't process safe. 
// To solve this problem both HaifaPort and EilatPort need to wait untill it's their turn to print.
HANDLE processSafePrintSemaphore;

// Variables which support our pipes.
HANDLE readFromHaifaHandle; // Output for Med. Sea ==> Red Sea Pipe.
HANDLE writeToHaifaHandle; // Input for Med. Sea <== Red Sea Pipe.
DWORD numberOfReadBytes, numberOfWrittenBytes;
char buffer[BUFFER_SIZE]; // Contains messages that are sent/received through pipes.

// A "Bolean" variable to help us indicate that all the vessels have arrived to EilatPort.
int haveAllVesselsArrived = FALSE; 
// A "Boolean" variable with which the main thread will indicate the crane threads when to end.
int areAllVesselsDone = FALSE;

int main(int argc, char* argv[])
{
	// Receive pipe ends for output and input.
	readFromHaifaHandle = GetStdHandle(STD_INPUT_HANDLE);
	writeToHaifaHandle = GetStdHandle(STD_OUTPUT_HANDLE);

	const int numberOfVessels = getNumberOfVesselsFromHaifaPort();

	writeToHaifaPortPassageResult(numberOfVessels);

	// Set seed for rand() function.
	srand((unsigned int)time(NULL));

	const int numberOfCranes = getRandomDivisor(numberOfVessels);

	initializeGlobalMutexAndSemaphores(numberOfVessels, numberOfCranes);

	int* cranesId = NULL;
	HANDLE* cranesHandler = createCraneThreads(numberOfCranes, &cranesId);

	barrier = constructQueue(numberOfVessels);
	unloadingQuay = constructUnloadingQuay(cranesId, numberOfCranes);

	if (barrier == NULL || unloadingQuay == NULL || 
		unloadingQuay->unloadingQuayStation == NULL)
	{
		fprintf(stderr, "EilatPort::Main::Unexpected Error - "
			"barrier/unloadingQuay is NULL!\n");
		exit(EXIT_FAILURE);
	}

	HANDLE unloadingQuayHandler;
	createUnloadingQuayThread(&unloadingQuayHandler);

	int* vesselsId = NULL;
	HANDLE* vesselsHandler = readAndCreateIncomingVesselsFromHaifaPort(numberOfVessels, &vesselsId);
	haveAllVesselsArrived = TRUE;

	// Wait for all vessel threads to terminate.
	WaitForMultipleObjects(numberOfVessels, vesselsHandler, TRUE, INFINITE);

	// Indication for crane threads to end.
	areAllVesselsDone = areAllVesselsDoneatHaifaPort();
	signalCranesToFinish(numberOfCranes);

	// Wait for all crane threads to terminate.
	WaitForMultipleObjects(numberOfCranes, cranesHandler, TRUE, INFINITE);
	// Wait for unloading quay thread to terminate.
	WaitForSingleObject(unloadingQuayHandler, INFINITE);

	// Memory clean up.
	freeVesselThreads(vesselsHandler, vesselsId, numberOfVessels);
	freeCraneThreads(cranesHandler, cranesId, numberOfCranes);
	cleanUnloadingQuayAndBarrier(&unloadingQuayHandler);

	writeToHaifaPortThatEilatPortIsDone();

	cleanGlobalMutexAndSemaphores(numberOfVessels, numberOfCranes);

	// Close EilatPorts ends of pipes.
	CloseHandle(readFromHaifaHandle);
	CloseHandle(writeToHaifaHandle);

	return 0;
}

VesselQueue* constructQueue(int limit)
{
	VesselQueue* vesselQueue = (VesselQueue*)malloc(sizeof(VesselQueue));

	if (vesselQueue == NULL)
	{
		fprintf(stderr, "EilatPort::ConstructQueue::Unexpected Error - "
			"Memory allocation failed!\n");
		return NULL;
	}

	vesselQueue->limit = limit;
	vesselQueue->size = 0;
	vesselQueue->head = NULL;
	vesselQueue->tail = NULL;

	return vesselQueue;
}

void destructQueue(VesselQueue* vesselQueue)
{
	while (!isEmpty(vesselQueue))
	{
		dequeue(vesselQueue);
	}

	free(vesselQueue);
}

int enqueue(VesselQueue* vesselQueue, int vesselId)
{
	if (vesselQueue == NULL)
	{
		return FALSE;
	}

	if (vesselQueue->size >= vesselQueue->limit)
	{
		return FALSE;
	}

	VesselNode* vesselNode = (VesselNode*)malloc(sizeof(VesselNode));

	if (vesselNode == NULL)
	{
		return FALSE;
	}

	vesselNode->vesselId = vesselId;
	vesselNode->prev = NULL;

	if (vesselQueue->size == 0) // Queue is empty
	{
		vesselQueue->head = vesselNode;
		vesselQueue->tail = vesselNode;

	}
	else // Add to the end of the Queue
	{

		vesselQueue->tail->prev = vesselNode;
		vesselQueue->tail = vesselNode;
	}

	vesselQueue->size++;
	return TRUE;
}

int dequeue(VesselQueue* vesselQueue)
{
	if (isEmpty(vesselQueue))
	{
		return -1;
	}

	VesselNode* vesselNode;
	int vesselId;

	vesselNode = vesselQueue->head;
	vesselId = vesselNode->vesselId;
	vesselQueue->head = vesselNode->prev;
	vesselQueue->size--;

	free(vesselNode);

	return vesselId;
}

int isEmpty(VesselQueue* vesselQueue)
{
	return vesselQueue->size == 0;
}

UnloadingQuayStruct* constructUnloadingQuay(int cranesId[], int numberOfCranes)
{
	UnloadingQuayStruct* pUnloadingQuay =
		(UnloadingQuayStruct*)malloc(sizeof(UnloadingQuayStruct));

	if (pUnloadingQuay == NULL)
	{
		fprintf(stderr, "EilatPort::ConstructUnloadingQuay::Unexpected Error - "
			"UnloadingQuayStruct memory allocation failed!");
		return NULL;
	}

	pUnloadingQuay->unloadingQuayStation =
		(UnloadingQuayStation*)malloc(numberOfCranes * sizeof(UnloadingQuayStation));

	if (pUnloadingQuay->unloadingQuayStation == NULL)
	{
		fprintf(stderr, "EilatPort::ConstructUnloadingQuay::Unexpected Error - "
			"UnloadingQuayStation memory allocation failed!");
		return NULL;
	}

	pUnloadingQuay->unloadingQuaySize = numberOfCranes;

	for (int i = 0; i < pUnloadingQuay->unloadingQuaySize; i++)
	{
		pUnloadingQuay->unloadingQuayStation[i].craneId = cranesId[i];
		pUnloadingQuay->unloadingQuayStation[i].vesselId = -1;
		pUnloadingQuay->unloadingQuayStation[i].cargoWeight = -1;
		pUnloadingQuay->unloadingQuayStation[i].isOccupied = FALSE;
	}

	return pUnloadingQuay;
}

void destructUnloadingQuay(UnloadingQuayStruct* pUnloadingQuay)
{
	free(pUnloadingQuay->unloadingQuayStation);
	free(pUnloadingQuay);
}

int isUnloadingQuayEmpty(UnloadingQuayStruct* pUnloadingQuay)
{
	for (int i = 0; i < pUnloadingQuay->unloadingQuaySize; i++)
	{
		if (pUnloadingQuay->unloadingQuayStation[i].vesselId != -1 ||
			pUnloadingQuay->unloadingQuayStation[i].isOccupied != FALSE)
		{
			return FALSE;
		}
	}

	return TRUE;
}

void removeVesselsFromUnloadingQuay(UnloadingQuayStruct* pUnloadingQuay)
{
	for (int i = 0; i < pUnloadingQuay->unloadingQuaySize; i++)
	{
		pUnloadingQuay->unloadingQuayStation[i].isOccupied = FALSE;
		pUnloadingQuay->unloadingQuayStation[i].vesselId = -1;
	}
}

int safeRand(void)
{
	WaitForSingleObject(randomMutex, INFINITE);

	int randomNumber = rand();

	if (!ReleaseMutex(randomMutex))
	{
		fprintf(stderr, "safeRand::Unexpected error - randomMutex.V()\n");
	}

	return randomNumber;
}

int randomSleepTime(void)
{
	return safeRand() % (MAX_SLEEP_TIME - MIN_SLEEP_TIME + 1) + MIN_SLEEP_TIME;
}

int randomCargoWeight(void)
{
	return safeRand() % (MAX_WEIGHT - MIN_WEIGHT + 1) + MIN_WEIGHT;
}

void initializeGlobalMutexAndSemaphores(int numberOfVessels, int numberOfCranes)
{
	// Shared semaphore's names
	LPCWSTR medToRedCanalString = L"MedToRedCanal";
	LPCWSTR redToMedCanalString = L"RedToMedCanal";
	LPCWSTR processSafePrintString = L"ProcessSafePrint";

	randomMutex = CreateMutex(NULL, FALSE, NULL);
	stationMutex = CreateMutex(NULL, FALSE, NULL);
	barrierSemaphore = CreateSemaphore(NULL, 0, numberOfVessels, NULL);

	// Open shared semaphores between HaifaPort and EilatPort.
	redToMedCanalSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, redToMedCanalString);
	medToRedCanalSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, medToRedCanalString);
	processSafePrintSemaphore = OpenSemaphore(SEMAPHORE_ALL_ACCESS, FALSE, processSafePrintString);

	if (randomMutex == NULL || stationMutex == NULL || 
		barrierSemaphore == NULL || processSafePrintSemaphore == NULL ||
		medToRedCanalSemaphore == NULL || redToMedCanalSemaphore == NULL)
	{
		fprintf(stderr, "EilatPort::initializeGlobalMutexAndSemaphores::Unexpected Error -"
			" Mutex/Semaphore creation failed!\n");
		exit(EXIT_FAILURE);
	}

	vesselsSemaphores = (HANDLE*)malloc(numberOfVessels * sizeof(HANDLE));
	cranesSemaphores = (HANDLE*)malloc(numberOfCranes * sizeof(HANDLE));
	unloadingQuaySemaphore = (HANDLE*)malloc(numberOfCranes * sizeof(HANDLE));

	if (vesselsSemaphores == NULL || cranesSemaphores == NULL ||
		unloadingQuaySemaphore == NULL)
	{
		fprintf(stderr, "EilatPort::initializeGlobalMutexAndSemaphores::Unexpected Error -"
			" Memory allocation failed!\n");
		exit(EXIT_FAILURE);
	}

	for (int i = 0; i < numberOfVessels; i++)
	{
		vesselsSemaphores[i] = CreateSemaphore(NULL, 0, 1, NULL);

		if (vesselsSemaphores[i] == NULL)
		{
			fprintf(stderr, "EilatPort::initializeGlobalMutexAndSemaphores::Unexpected Error -"
				" vessel semphore %d Creation Failed!\n", i);
			exit(EXIT_FAILURE);
		}
	}

	for (int i = 0; i < numberOfCranes; i++)
	{
		cranesSemaphores[i] = CreateSemaphore(NULL, 0, 1, NULL);
		unloadingQuaySemaphore[i] = CreateSemaphore(NULL, 0, 1, NULL);

		if (cranesSemaphores[i] == NULL || unloadingQuaySemaphore[i] == NULL)
		{
			fprintf(stderr, "initializeGlobalMutexAndSemaphores::Unexpected Error - "
				"crane/unloadingquay semphore %d Creation Failed!\n", i);
			exit(EXIT_FAILURE);
		}
	}
}

void cleanGlobalMutexAndSemaphores(int numberOfVessels, int numberOfCranes)
{
	CloseHandle(randomMutex);
	CloseHandle(stationMutex);
	CloseHandle(barrierSemaphore);
	CloseHandle(redToMedCanalSemaphore);
	CloseHandle(medToRedCanalSemaphore);
	CloseHandle(processSafePrintSemaphore);

	for (int i = 0; i < numberOfVessels; i++)
	{
		CloseHandle(vesselsSemaphores[i]);
	}

	free(vesselsSemaphores);

	for (int i = 0; i < numberOfCranes; i++)
	{
		CloseHandle(cranesSemaphores[i]);
		CloseHandle(unloadingQuaySemaphore[i]);
	}

	free(cranesSemaphores);
	free(unloadingQuaySemaphore);
}

int getNumberOfVesselsFromHaifaPort(void)
{
	// Read number of vessels incoming from HaifaPort.
	if (!ReadFile(readFromHaifaHandle, buffer, BUFFER_SIZE, &numberOfReadBytes, NULL))
	{
		fprintf(stderr, "EilatPort::Main::Unexpected Error - "
			"Reading number of vessels from 'Med Sea. ==> Red Sea' pipe failed!\n");
		exit(EXIT_FAILURE);
	}

	return atoi(buffer);
}

void writeToHaifaPortPassageResult(int numberOfVessels)
{
	GetLocalTime(&currentTime);
	fprintf(stderr, "[%02d:%02d:%02d] Eilat Port: "
		"Proccessing passage approval for %d vessels...\n",
		currentTime.wHour, currentTime.wMinute, currentTime.wSecond, numberOfVessels);

	int passageResult = !isPrimeNumber(numberOfVessels);

	GetLocalTime(&currentTime);
	fprintf(stderr, "[%02d:%02d:%02d] Eilat Port: passage for %d vessels %s!\n",
		currentTime.wHour, currentTime.wMinute, currentTime.wSecond, numberOfVessels,
		passageResult ? "approved" : "denied");

	sprintf(buffer, "%d", passageResult);

	// Writing passage result to 'Med. Sea <== Red Sea' pipe
	if (!WriteFile(writeToHaifaHandle, buffer, BUFFER_SIZE, &numberOfWrittenBytes, NULL))
	{
		fprintf(stderr, "EilatPort::writeToHaifaPortPassageResult::Unexpected Error -"
			" Writing passage result to 'Med. Sea <== Red Sea' pipe failed\n");
		exit(EXIT_FAILURE);
	}

	if (!passageResult)
	{
		exit(EXIT_SUCCESS);
	}
}

int isPrimeNumber(int number)
{
	if (number == 1)
	{
		return FALSE;
	}

	for (int i = 2; i < number; i++)
	{
		if (number % i == 0)
		{
			return FALSE;
		}
	}

	return TRUE;
}

int getRandomDivisor(int dividendNumber)
{
	int divisor;

	do
	{
		divisor = rand() % (dividendNumber - 2) + 2;
	} while (dividendNumber % divisor != 0);

	return divisor;
}

HANDLE* createCraneThreads(int numberOfCranes, int** cranesId)
{
	DWORD threadId;
	*cranesId = (int*)malloc(numberOfCranes * sizeof(int));
	HANDLE* cranesHandler = (HANDLE*)malloc(numberOfCranes * sizeof(HANDLE));

	if (*cranesId == NULL || cranesHandler == NULL)
	{
		fprintf(stderr, "EilatPort::createCraneThreads::Unexpected Error -"
			" Memory allocation failed!\n");
		exit(EXIT_FAILURE);
	}

	// Create all Crane threads. ID starts with 1 till numberOfCranes.
	for (int i = 1; i <= numberOfCranes; i++)
	{
		int craneIndex = i - 1;

		(*cranesId)[craneIndex] = i;
		cranesHandler[craneIndex] =
			CreateThread(NULL, 0, Crane, &(*cranesId)[craneIndex], 0, &threadId);

		if (cranesHandler[craneIndex] == NULL)
		{
			fprintf(stderr, "EilatPort::createCraneThreads::Unexpected Error -"
				" Crane thread %d creation failed!\n", (*cranesId)[craneIndex]);
			exit(EXIT_FAILURE);
		}
	}

	return cranesHandler;
}

void createUnloadingQuayThread(HANDLE* unloadingQuayHandler)
{
	DWORD threadId;
	int unloadingQuayId = 1;
	*unloadingQuayHandler =
		CreateThread(NULL, 0, UnloadingQuay, &unloadingQuayId, 0, &threadId);

	if (*unloadingQuayHandler == NULL)
	{
		fprintf(stderr, "EilatPort::createUnloadingQuayThread::Unexpected Error -"
			" unloadingQuayHandle thread %d creation failed!\n", unloadingQuayId);
		exit(EXIT_FAILURE);
	}

	// Set threads prioirty to be the highest, so when the barrier has reached
	// an amount that is allowed to unload, the unloading quay will set in motion
	// immediately 
	if (!SetThreadPriority(*unloadingQuayHandler, THREAD_PRIORITY_HIGHEST))
	{
		fprintf(stderr, "EilatPort::createUnloadingQuayThread::Unexpected Error -"
			" thread priority failed!\n");
		exit(EXIT_FAILURE);
	}
}

HANDLE* readAndCreateIncomingVesselsFromHaifaPort(int numberOfVessels, int** vesselsId)
{
	DWORD threadId;
	*vesselsId = (int*)malloc(numberOfVessels * sizeof(int));
	HANDLE* vesselsHandler = (HANDLE*)malloc(numberOfVessels * sizeof(HANDLE));

	if (*vesselsId == NULL || vesselsHandler == NULL)
	{
		fprintf(stderr, "EilatPort::readAndCreateIncomingVesselsFromHaifaPort::Unexpected Error -"
			" Memory allocation failed!\n");
		exit(EXIT_FAILURE);
	}

	// Read incoming vessels from HaifaPort and create threads according to their ID.
	for (int i = 0; i < numberOfVessels; i++)
	{
		// Receive vessel's ID through the 'Med. Sea ==> Red Sea' pipe.
		if (!ReadFile(readFromHaifaHandle, buffer, BUFFER_SIZE, &numberOfReadBytes, NULL))
		{
			fprintf(stderr, "EilatPort::readAndCreateIncomingVesselsFromHaifaPort::Unexptected Error -"
				" reading vessel from 'Med. Sea ==> Red Sea' pipe failed!\n");
			exit(EXIT_FAILURE);
		}

		int vesselId = atoi(buffer);
		int vesselIndex = vesselId - 1;

		(*vesselsId)[vesselIndex] = vesselId;
		vesselsHandler[vesselIndex] =
			CreateThread(NULL, 0, Vessel, &(*vesselsId)[vesselIndex], 0, &threadId);

		if (vesselsHandler[vesselIndex] == NULL)
		{
			fprintf(stderr, "EilatPort::readAndCreateIncomingVesselsFromHaifaPort::Unexpected Error -" 
				"Vessel thread %d creation failed!\n", (*vesselsId)[vesselIndex]);
			exit(EXIT_FAILURE);
		}
	}

	return vesselsHandler;
}

int areAllVesselsDoneatHaifaPort(void)
{
	// Check that all vessel threads are done in HaifaPort.
	// Comment: This command operates more as a cosmetic reason, since when the last thread 
	// has returend to HaifaPort, EilatPort will start its printing ending messages. 
	// With this EilatPort will wait till the end of all vessel's messages.
	if (!ReadFile(readFromHaifaHandle, buffer, BUFFER_SIZE, &numberOfReadBytes, NULL))
	{
		fprintf(stderr, "EilatPort::areAllVesselsDoneatHaifaPort::Unexptected Error -"
			" Reading all vessels ended has failed!\n");
		exit(EXIT_FAILURE);
	}

	if (!atoi(buffer))
	{
		fprintf(stderr, "EilatPort::areAllVesselsDoneatHaifaPort::Unexptected Error -"
			" Vessels in HaifaPort still exist!\n");
		exit(EXIT_FAILURE);
	}

	return TRUE;
}

void signalCranesToFinish(int numberOfCranes)
{
	// Signal cranes to continue so they can end.
	for (int i = 0; i < numberOfCranes; i++)
	{
		if (!ReleaseSemaphore(cranesSemaphores[i], 1, NULL))
		{
			fprintf(stderr, "EilatPort::signalCranesToFinish::Unexpected Error -"
				" cranesSemaphores[%d].V()\n", i);
			exit(EXIT_FAILURE);
		}
	}
}

void freeVesselThreads(HANDLE* vesselsHandler, int* vesselsId, int numberOfVessels)
{
	char string[MAX_STRING];

	// Close all vessel Handles and free any related allocated memory.
	for (int i = 0; i < numberOfVessels; i++)
	{
		if (!CloseHandle(vesselsHandler[i]))
		{
			fprintf(stderr, "EilatPort::freeVesselThreads::Unexptected Error -"
				" CloseHandle(vesselsHandler[%d])\n", i);
			exit(EXIT_FAILURE);
		}
	}

	free(vesselsId);
	free(vesselsHandler);

	sprintf(string, "Eilat Port: All Vessel Threads are done");

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::freeVesselThreads::Unexpected Error -"
			" Print failed!\n");
		exit(EXIT_FAILURE);
	}
}

void freeCraneThreads(HANDLE* cranesHandler, int* cranesId, int numberOfCranes)
{
	char string[MAX_STRING];

	// Close all crane Handles and free any related allocated memory.
	for (int i = 0; i < numberOfCranes; i++)
	{
		if (!CloseHandle(cranesHandler[i]))
		{
			fprintf(stderr, "EilatPort::freeCraneThreads::Unexptected Error -"
				" CloseHandle(cranesHandler[%d])\n", i);
			exit(EXIT_FAILURE);
		}
	}

	free(cranesId);
	free(cranesHandler);

	sprintf(string, "Eilat Port: All Crane Threads are done");

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::freeCraneThreads::Unexpected Error -"
			" Print failed!\n");
		exit(EXIT_FAILURE);
	}
}

void cleanUnloadingQuayAndBarrier(HANDLE* unloadingQuayHandler)
{
	// Close unloading quay Handle and free any related allocated memory.
	CloseHandle(*unloadingQuayHandler);

	destructQueue(barrier);
	destructUnloadingQuay(unloadingQuay);

	/*sprintf(string, "Eilat Port: Unloading Quay Thread is done");

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::cleanUnloadingQuayAndBarrier::Unexpected Error -"
		" Print failed!\n");
		exit(EXIT_FAILURE);
	}*/
}

void writeToHaifaPortThatEilatPortIsDone(void)
{
	char string[MAX_STRING];

	sprintf(string, "Eilat Port: Exiting...");

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::writeToHaifaPortThatEilatPortIsDone::Unexpected Error -"
			" Print failed!\n");
		exit(EXIT_FAILURE);
	}

	// Write to HaifaPort that EilatPort has successfuly ended.
	sprintf(buffer, "%d", TRUE);

	if (!WriteFile(writeToHaifaHandle, buffer, BUFFER_SIZE, &numberOfWrittenBytes, NULL))
	{
		fprintf(stderr, "EilatPort::writeToHaifaPortThatEilatPortIsDone::Unexpected Error -"
			" Write process exit confimation to HaifaPort has failed!\n");
		exit(EXIT_FAILURE);
	}
}

int safePrintWithTimeStamp(char string[])
{
	WaitForSingleObject(processSafePrintSemaphore, INFINITE);

	GetLocalTime(&currentTime);
	fprintf(stderr, "[%02d:%02d:%02d] %s\n",
		currentTime.wHour, currentTime.wMinute, currentTime.wSecond, string);

	if (!ReleaseSemaphore(processSafePrintSemaphore, 1, NULL))
	{
		fprintf(stderr, "EilatPort::safePrintWithTimeStamp::Unexpected Error - "
			"processSafePrintSemaphore.V()\n");
		return FALSE;
	}

	return TRUE;
}

DWORD WINAPI Crane(LPVOID Param)
{
	// Get the thread's ID and save its index for array usage.
	int craneId = *(int*)Param;
	int craneIndex = craneId - 1;
	char string[MAX_STRING];

	sprintf(string, "Crane  %2d - starts operating", craneId);

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::Crane  %2d::Unexpected Error - Print failed!\n",
			craneId);
		return 1;
	}

	// Comment: for some reason rand() kept producing the same values
	// even though the seed has been set at the main. As far as I'm aware
	// the seed only needs to be set once, even in a threaded enviorment 
	// (for example our for Home Tasks it was enough only in the main).
	// For that reason the seed is set in every thread which makes use of rand().
	// I would like to know what's the reason for this if possible
	srand((unsigned int)time(NULL));

	// The function will live until indicated by the main thread to stop.
	// Comment: Would just like to mention that a for loop which runs 
	// numberOfVessels/numberOfCranes iterations could've also done the job,
	// but from what we understood in 5.6.3 we thought that an infinite loop was desired.
	while (!areAllVesselsDone)
	{
		// Wait till a vessel signals to start unloading its cargo.
		WaitForSingleObject(cranesSemaphores[craneIndex], INFINITE);

		// Check if the main thread has indicated to stop running.
		if (areAllVesselsDone)
		{
			break;
		}

		Sleep(randomSleepTime());

		sprintf(string, "Crane  %2d - unloaded %d tons from vessel %d", craneId,
			unloadingQuay->unloadingQuayStation[craneIndex].cargoWeight,
			unloadingQuay->unloadingQuayStation[craneIndex].vesselId);

		if (!safePrintWithTimeStamp(string))
		{
			fprintf(stderr, "EilatPort::Crane  %2d::Unexpected Error - Print failed!\n",
				craneId);
			return 1;
		}

		// Unload the vessel's cargo.
		unloadingQuay->unloadingQuayStation[craneIndex].cargoWeight = -1;

		// Signal vessel that the unloading process has ended.
		if (!ReleaseSemaphore(vesselsSemaphores[
			unloadingQuay->unloadingQuayStation[craneIndex].vesselId - 1], 1, NULL))
		{
			fprintf(stderr, "EilatPort::Crane::Unexpected Error - vesselsSemaphores[%d].V()\n",
				unloadingQuay->unloadingQuayStation[craneIndex].vesselId);
		}
	}

	sprintf(string, "Crane  %2d - done operating", craneId);

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::Crane  %2d::Unexpected Error - Print failed!\n",
			craneId);
		return 1;
	}

	return 0;
}

DWORD WINAPI Vessel(LPVOID Param)
{
	// Get the thread's ID and save its index for array usage.
	int vesselId = *(int*)Param;
	int vesselIndex = vesselId - 1;

	// Comment: for some reason rand() kept producing the same values
	// even though the seed has been set at the main. As far as I'm aware
	// the seed only needs to be set once, even in a threaded enviorment 
	// (for example our for Home Tasks it was enough only in the main).
	// For that reason the seed is set in every thread which makes use of rand().
	// I would like to know what's the reason for this if possible
	srand((unsigned int)time(NULL));

	return startSailingAndEnterBarrier(vesselId, vesselIndex) ||
		enterUnloadingQuayAndStartUnloadingProcess(vesselId, vesselIndex) ||
		sailToHaiafaPort(vesselId);



	return 0;
}

DWORD WINAPI UnloadingQuay(LPVOID Param)
{
	// Get the thread's ID
	int craneId = *(int*)Param;

	// Run untill all the vessels have left the barrier.
	while (!(isEmpty(barrier) && haveAllVesselsArrived))
	{
		// Wait for vessels of an equal number to cranes to reach the barrier.
		// By using a loop on WaitForSingleObject we are able to lower the 
		// semaphore's counter by the desired amount.
		for (int i = 0; i < unloadingQuay->unloadingQuaySize; i++)
		{
			WaitForSingleObject(barrierSemaphore, INFINITE);
		}

		if (barrier->size >= unloadingQuay->unloadingQuaySize &&
			isUnloadingQuayEmpty(unloadingQuay))
		{
			for (int i = 0; i < unloadingQuay->unloadingQuaySize; i++)
			{
				int vesselId = dequeue(barrier);

				if (vesselId == -1)
				{
					fprintf(stderr, "EilatPort::UnloadingQuay::Unexpected Error - "
						"Dequeue == -1!\n");
					return 1;
				}

				// Signal the vessel to continue its unloading process.
				if (!ReleaseSemaphore(vesselsSemaphores[vesselId - 1], 1, NULL))
				{
					fprintf(stderr, "EilatPort::UnloadingQuay::Unexpected Error - "
						"vesselsSemaphores[%d].V()\n", vesselId);
					return 1;
				}
			}

			// Wait untill all vessels have left the unloading quay (is empty).
			WaitForMultipleObjects(unloadingQuay->unloadingQuaySize, unloadingQuaySemaphore, TRUE, INFINITE);
			// Empty all unloading quay stations so new vessels can stop there.
			removeVesselsFromUnloadingQuay(unloadingQuay);
		}
	}

	return 0;
}

int startSailingAndEnterBarrier(int vesselId, int vesselIndex)
{
	char string[MAX_STRING];

	// Comment: At start we also printed this line, though we noticed that in the 
	// intsructions and the example it didn't appear so we decided to remove it.
	/*sprintf(string, "Vessel %2d - exiting Canal: Med. Sea ==> Red Sea", vesselId);

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::startSailingAndEnterBarrier::"
			"Unexpected Error - Print failed!\n", vesselId);
		exit(EXIT_FAILURE);
	}*/

	sprintf(string, "Vessel %2d - arrived @ Eilat Port", vesselId);

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::startSailingAndEnterBarrier::"
			"Unexpected Error - Print failed!\n", vesselId);
		return 1;
	}

	Sleep(randomSleepTime());

	// Signal that the 'Med. Sea ==> Red Sea' pipe is free for another vessel to pass.
	if (!ReleaseSemaphore(medToRedCanalSemaphore, 1, NULL))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::startSailingAndEnterBarrier::"
			"Unexpected Error - medToRedCanalSemaphore.V()\n", vesselId);
		exit(EXIT_FAILURE);
	}

	// Enter barrier for the unloading quay.
	if (!enqueue(barrier, vesselId))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::startSailingAndEnterBarrier::"
			"Unexpected Error - Enqueue failed!\n", vesselId);
		return 1;
	}

	sprintf(string, "Vessel %2d - entering Barrier", vesselId);

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::startSailingAndEnterBarrier::"
			"Unexpected Error - Print failed!\n", vesselId);
		return 1;
	}

	// Signal that the vessel has reached the barrier.
	if (!ReleaseSemaphore(barrierSemaphore, 1, NULL))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::startSailingAndEnterBarrier"
			"::Unexpected Error - barrierSemaphore.V()\n", vesselId);
		return 1;
	}

	// Wait untill the vessel enters the unloading quay.
	WaitForSingleObject(vesselsSemaphores[vesselIndex], INFINITE);

	return 0;
}

int enterUnloadingQuayAndStartUnloadingProcess(int vesselId, int vesselIndex)
{
	char string[MAX_STRING];

	sprintf(string, "Vessel %2d - entering Unloading Quay", vesselId);

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::enterUnloadingQuay::"
			"Unexpected Error - Print failed!\n", vesselId);
		return 1;
	}

	Sleep(randomSleepTime());

	int stationIndex = stationVesselInUnloadingQuay(vesselId);

	if (stationIndex == -1)
	{
		return 1;
	}

	sprintf(string, "Vessel %2d - stationed near crane %d", vesselId,
		unloadingQuay->unloadingQuayStation[stationIndex].craneId);

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::Unexpected Error - Print failed!\n",
			vesselId);
		return 1;
	}

	return startUnloadingVessel(vesselId, vesselIndex, stationIndex) ||
		exitUnloadingQuay(vesselId, stationIndex);
}

int stationVesselInUnloadingQuay(int vesselId)
{
	// "Critical Section" only allow one vessel to find a station 
	// in the unloading quay at a time to prevent race condition.
	WaitForSingleObject(stationMutex, INFINITE);

	int stationIndex = -1;

	// Find a free station for the vessel in the unloading quay.
	for (int i = 0; i < unloadingQuay->unloadingQuaySize; i++)
	{
		if (unloadingQuay->unloadingQuayStation[i].isOccupied == FALSE)
		{
			unloadingQuay->unloadingQuayStation[i].vesselId = vesselId;
			unloadingQuay->unloadingQuayStation[i].isOccupied = TRUE;
			stationIndex = i;

			break;
		}
	}

	if (stationIndex == -1)
	{
		fprintf(stderr, "EilatPort::Vessel %2d::findStationInUnloadingQuay::"
			"Unexpected Error - stationing vessel failed!\n", vesselId);
		return -1;
	}

	// Release entry to critical section to allow another vessel to find its station.
	if (!ReleaseMutex(stationMutex))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::findStationInUnloadingQuay::"
			"Unexpected Error - unloadingQuayMutex.V()\n", vesselId);
		return -1;
	}

	return stationIndex;
}

int startUnloadingVessel(int vesselId, int vesselIndex, int stationIndex)
{
	char string[MAX_STRING];

	// Assign random cargo weight for the vessel.
	unloadingQuay->unloadingQuayStation[stationIndex].cargoWeight = randomCargoWeight();

	sprintf(string, "Vessel %2d - cargo's weight is %d tons", vesselId,
		unloadingQuay->unloadingQuayStation[stationIndex].cargoWeight);

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::startUnloadingVessel::"
			"Unexpected Error - Print failed!\n", vesselId);
		return 1;
	}

	// Signal crane to start unloading cargo from the vessel.
	if (!ReleaseSemaphore(cranesSemaphores[stationIndex], 1, NULL))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::startUnloadingVessel::"
			"Unexpected Error - cranesSemaphores[%d].V()\n", vesselId,
			unloadingQuay->unloadingQuayStation[stationIndex].craneId);
		return 1;
	}

	// Wait untill the crane is done unloading cargo from the vessel.
	WaitForSingleObject(vesselsSemaphores[vesselIndex], INFINITE);

	return 0;
}

int exitUnloadingQuay(int vesselId, int stationIndex)
{
	char string[MAX_STRING];

	Sleep(randomSleepTime());

	sprintf(string, "Vessel %2d - exiting unloading quay", vesselId);

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::exitUnloadingQuay::"
			"Unexpected Error - Print failed!\n", vesselId);
		return 1;
	}

	// Signal the unloading quay that the vessel has left the station.
	if (!ReleaseSemaphore(unloadingQuaySemaphore[stationIndex], 1, NULL))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::exitUnloadingQuay::"
			"Unexpected Error - unloadingQuaySemaphore[%d].V()\n", vesselId,
			unloadingQuay->unloadingQuayStation[stationIndex].craneId);
		return 1;
	}

	return 0;
}

int sailToHaiafaPort(int vesselId)
{
	char string[MAX_STRING];

	// Wait for access to the canal.
	WaitForSingleObject(redToMedCanalSemaphore, INFINITE);

	sprintf(string, "Vessel %2d - entering Canal: Red Sea ==> Med.Sea", vesselId);

	if (!safePrintWithTimeStamp(string))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::sailToHaiafaPort::"
			"Unexpected Error - Print failed!\n", vesselId);
		return 1;
	}

	Sleep(randomSleepTime());

	sprintf(buffer, "%d", vesselId);

	// Writing vessel's ID to 'Med. Sea <== Red Sea' pipe.
	if (!WriteFile(writeToHaifaHandle, buffer, BUFFER_SIZE, &numberOfWrittenBytes, NULL))
	{
		fprintf(stderr, "EilatPort::Vessel %2d::sailToHaiafaPort::"
			"Unexpected Error - writing vessel to 'Med. Sea <== Red Sea' pipe failed\n", vesselId);
		return 1;
	}

	return 0;
}