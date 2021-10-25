#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h> 
#include <stdlib.h> 
#include <windows.h> 
#include <time.h> 

#define MIN_NUMBER_OF_VESSELS 2
#define MAX_NUMBER_OF_VESSELS 50

#define MIN_SLEEP_TIME 5 // 5 miliseconds 
#define MAX_SLEEP_TIME 3000 // 3 seconds

#define BUFFER_SIZE 60 // Size of largest message to send/receive through pipes.
#define MAX_STRING 200 // Size of the larget string to send to the safe fprintf.

// Random Functions:
// Thread safe rand()
int safeRand(void);
// Calculates sleep time according to the defined MIN_SLEEP_TIME and MAX_SLEEP_TIME.
int randomSleepTime(void);

// Initialize and destruct all global Mutexes/Semaphores.
void initializeGlobalMutexAndSemaphores(int numberOfVessels, SECURITY_ATTRIBUTES* securityAttributes);
void cleanGlobalMutexAndSemaphores(int numberOfVessels);

// Main thread functions:
// Creates 'Med. Sea ==> Red Sea' and 'Med. Sea <== Red Sea' pipes.
void createSuezCanalPipes(SECURITY_ATTRIBUTES* securityAttributes);
// Set the STARTUPINFO struct and create EilatPort process.
void setStartUpInfoAndStartEilatPortProcess(void);
// Handles all of the passage approval process between Haifa and Eilat ports.
void suezCanalPassageApproval(int numberOfVessels);
// Create all vessel threads according to the number given at the command line.
HANDLE* createVesselThreads(int numberOfVessels, int** vesselsId);
// Listen for incoming vessels from 'Med. Sea <== Red Sea' pipe and signal them to continue.
void readIncomingVesselsFromEilatPort(int numberOfVessels);
// Write To EilatPort that all Vessel threads are done and also wait till all EilatPort 
// threads are done.
void updateEilatAllVesselsDoneAndWaitForThreads(void);
// Free vessels HANDLER and CloseHandle.
void freeVesselThreads(HANDLE* vesselsHandler, int* vesselsId, int numberOfVessels);

// fprintf may be a thread safe function, though it isn't process safe and for that reason
// the function is protected by a semaphore that is shared between both HaifaPort and EilatPort.
int safePrintWithTimeStamp(char string[]);

// The vessels thread function.
DWORD WINAPI Vessel(LPVOID Param);

// These functions are pieces of the vessel thread:
int startSailing(int vesselId);
int sailToEilatPort(int vesselId);
int returnFromEilatToEndSailing(int vesselId);

// Struct for Date and Time. Fill in the struct with GetLocalTime().
SYSTEMTIME currentTime; 

// With this duo we are able to allow only 1 vessel at a time to be in the canal
HANDLE medToRedCanalSemaphore; // Mutex to allow only one vessel at a time to enter the canal (pipe).
HANDLE redToMedCanalSemaphore; // Mutex to allow only one vessel at a time to exit the canal (pipe).

// Mutex to make rand() thread safe.
HANDLE randomMutex;

// The reasoning behind the semaphore is to prevent race conditions.
// fprintf is a thread safe function, although it isn't process safe. 
// To solve this problem both HaifaPort and EilatPort need to wait untill it's their turn to print.
HANDLE processSafePrintSemaphore;

// Semaphore for each Vessel to signal when to wait and continue.
HANDLE* vesselsSemaphores; 

// Variables which support our pipes.
HANDLE readFromHaifaHandle, writeToEilatHandle; // Output and Input for Med. Sea ==> Red Sea Pipe.
HANDLE readFromEilatHandle, writeToHaifaHandle; // Output and Input for Med. Sea <== Red Sea Pipe.
DWORD numberOfReadBytes, numberOfWrittenBytes;
char buffer[BUFFER_SIZE]; // Contains messages that are sent/received through pipes.

int main(int argc, char* argv[])
{
    // Check that the user's input is valid and save it to a variable.
    if (argc != 2)
    {
        fprintf(stderr, "HaifaPort::Main::Error - Number of arguments is invalid!"
            " Please enter only 1 arguement!\n");
        exit(EXIT_SUCCESS);
    }

    const int numberOfVessels = atoi(argv[1]);

    if (numberOfVessels < MIN_NUMBER_OF_VESSELS || numberOfVessels > MAX_NUMBER_OF_VESSELS)
    {
        fprintf(stderr, "HaifaPort::Main::Error - Number of vessels must be between %d-%d!\n",
            MIN_NUMBER_OF_VESSELS, MAX_NUMBER_OF_VESSELS);
        exit(EXIT_SUCCESS);
    }

    // Set seed for rand() function.
    srand((unsigned int)time(NULL));

    // Set-up security attributes, so that handles may be inherited.
    SECURITY_ATTRIBUTES securityAttributes = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };

    createSuezCanalPipes(&securityAttributes);

    // Initialize Mutex/Semaphores before the EilatPort process is created,
    // so they can be inherited if so desired.
    initializeGlobalMutexAndSemaphores(numberOfVessels, &securityAttributes);

    setStartUpInfoAndStartEilatPortProcess();

    // Close HaifaPort's unused ends of the pipes.
    CloseHandle(readFromHaifaHandle);
    CloseHandle(writeToHaifaHandle);

    // Send the number of vessels to EilatPort and operate according to the approval result.
    suezCanalPassageApproval(numberOfVessels);

    // Run all vessel threads and Wait for them to return from EilatPort.
    int* vesselsId = NULL;
    HANDLE* vesselsHandler = createVesselThreads(numberOfVessels, &vesselsId);
    readIncomingVesselsFromEilatPort(numberOfVessels);

    // Wait for all vessels threads to terminate.
    WaitForMultipleObjects(numberOfVessels, vesselsHandler, TRUE, INFINITE);
    updateEilatAllVesselsDoneAndWaitForThreads();
    
    // Close HaifaPorts ends of pipes.
    CloseHandle(readFromEilatHandle);
    CloseHandle(writeToEilatHandle);

    freeVesselThreads(vesselsHandler, vesselsId, numberOfVessels);
    cleanGlobalMutexAndSemaphores(numberOfVessels);

    GetLocalTime(&currentTime);
    fprintf(stderr, "[%02d:%02d:%02d] Haifa Port: Exiting...\n",
        currentTime.wHour, currentTime.wMinute, currentTime.wSecond);

    return 0;
}

int safeRand(void)
{
    WaitForSingleObject(randomMutex, INFINITE);

    int randomNumber = rand();

    if (!ReleaseMutex(randomMutex))
    {
        fprintf(stderr, "HaifaPort::safeRand::Unexpected error - randomMutex.V()\n");
    }

    return randomNumber;
}

int randomSleepTime(void)
{
    return safeRand() % (MAX_SLEEP_TIME - MIN_SLEEP_TIME + 1) + MIN_SLEEP_TIME;
}

void initializeGlobalMutexAndSemaphores(int numberOfVessels, SECURITY_ATTRIBUTES* securityAttributes)
{
    // Shared semaphore's names
    LPCWSTR medToRedCanalString = L"MedToRedCanal";
    LPCWSTR redToMedCanalString = L"RedToMedCanal";
    LPCWSTR processSafePrintString = L"ProcessSafePrint";

    randomMutex = CreateMutex(NULL, FALSE, NULL);
    // Create shared semaphores between HaifaPort and EilatPort.
    medToRedCanalSemaphore = CreateSemaphore(securityAttributes, 1, 1, medToRedCanalString);
    redToMedCanalSemaphore = CreateSemaphore(securityAttributes, 1, 1, redToMedCanalString);
    processSafePrintSemaphore = CreateSemaphore(securityAttributes, 1, 1, processSafePrintString);

    if (randomMutex == NULL || medToRedCanalSemaphore == NULL || 
        redToMedCanalSemaphore == NULL || processSafePrintSemaphore == NULL)
    {
        fprintf(stderr, "HaifaPort::initializeGlobalMutexAndSemaphores::Unexpected Error - "
            "Mutex/Semaphore creation failed!\n");
        exit(EXIT_FAILURE);
    }

    vesselsSemaphores = (HANDLE*)malloc(numberOfVessels * sizeof(HANDLE));

    if (vesselsSemaphores == NULL)
    {
        fprintf(stderr, "HaifaPort::initializeGlobalMutexAndSemaphores::Unexpected Error - "
            "Memory allocation failed!\n");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < numberOfVessels; i++)
    {
        vesselsSemaphores[i] = CreateSemaphore(NULL, 0, 1, NULL);

        if (vesselsSemaphores[i] == NULL)
        {
            fprintf(stderr, "HaifaPort::initializeGlobalMutexAndSemaphores::Unexpected Error - "
                "vesselsSemaphores[%d] Creation Failed!\n", i);
            exit(EXIT_FAILURE);
        }
    }
}

void cleanGlobalMutexAndSemaphores(int numberOfVessels)
{
    CloseHandle(randomMutex);
    CloseHandle(medToRedCanalSemaphore);
    CloseHandle(redToMedCanalSemaphore);
    CloseHandle(processSafePrintSemaphore);

    for (int i = 0; i < numberOfVessels; i++)
    {
        CloseHandle(vesselsSemaphores[i]);
    }

    free(vesselsSemaphores);
}

void createSuezCanalPipes(SECURITY_ATTRIBUTES* securityAttributes)
{
    // Create the pipe Haifa to Eilat (Med. Sea ==> Red Sea)
    if (!CreatePipe(&readFromHaifaHandle, &writeToEilatHandle, securityAttributes, 0))
    {
        fprintf(stderr, "HaifaPort::createSuezCanalPipes::Unexpected Error - "
            "'Med. Sea ==> Red Sea' pipe creation failed!\n");
        exit(EXIT_FAILURE);
    }

    // create the pipe Eilat to Haifa (Med. Sea <== Red Sea)
    if (!CreatePipe(&readFromEilatHandle, &writeToHaifaHandle, securityAttributes, 0))
    {
        fprintf(stderr, "HaifaPort::createSuezCanalPipes::Unexpected Error - "
            "'Med. Sea <== Red Sea' pipe creation failed!\n");
        exit(EXIT_FAILURE);
    }
}

void setStartUpInfoAndStartEilatPortProcess(void)
{
    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInformation;

    // Fill pi with zeros, in bytes.
    SecureZeroMemory(&processInformation, sizeof(processInformation));

    // Retrieves the conents of the STARTUPINFO structure
    // that was specified when the calling process was created.
    GetStartupInfo(&startupInfo);

    // The standard error device. This is the active console screen buffer.
    startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    // Redirect the standard input to the read end of the pipe.
    startupInfo.hStdOutput = writeToHaifaHandle;
    startupInfo.hStdInput = readFromHaifaHandle;
    // To inherit the standard input, output, and error handles, 
    // the dwFlags must include STARTF_USESTDHANDLES.
    startupInfo.dwFlags = STARTF_USESTDHANDLES;

    TCHAR ProcessName[MAX_STRING];
    wcscpy(ProcessName, L"EilatPort.exe");

    // Create and start the EilatPort process
    if (!CreateProcess(NULL,    // No module name (use command line).
        ProcessName,            // Command line.
        NULL,                   // Process handle not inheritable.
        NULL,                   // Thread handle not inheritable.
        TRUE,                   // Set handle inheritance to TRUE.
        0,                      // No creation flags.
        NULL,                   // Use parent's environment block.
        NULL,                   // Use parent's starting directory.
        &startupInfo,           // Pointer to STARTUPINFO structure.
        &processInformation)    // Pointer to PROCESS_INFORMATION structure.
        )
    {
        fprintf(stderr, "HaifaPort::setStartUpInfoAndStartEilatPortProcess::Unexpected Error -"
            " CreateProcess for EilatPort failed (%d)!\n", GetLastError());
        exit(EXIT_FAILURE);
    }
}

void suezCanalPassageApproval(int numberOfVessels)
{
    char string[MAX_STRING];

    sprintf(string, "Haifa Port : There are %d Vessels in the port",numberOfVessels);

    if (!safePrintWithTimeStamp(string))
    {
        fprintf(stderr, "HaifaPort::suezCanalPassageApproval::Unexpected Error -"
            " Print failed!\n");
        return 1;
    }

    sprintf(string, "Haifa Port: Requesting passage from Eilat Port...");

    if (!safePrintWithTimeStamp(string))
    {
        fprintf(stderr, "HaifaPort::suezCanalPassageApproval::Unexpected Error -"
            " Print failed!\n");
        return 1;
    }

    sprintf(buffer, "%d", numberOfVessels);

    // Writing number of vessels to 'Med. Sea ==> Red Sea' pipe.
    if (!WriteFile(writeToEilatHandle, buffer, BUFFER_SIZE, &numberOfWrittenBytes, NULL))
    {
        fprintf(stderr, "HaifaPort::suezCanalPassageApproval::Unexptected Error - "
            "Writing numberOfVessels to 'Med. Sea ==> Red Sea' pipe failed\n");
        exit(EXIT_FAILURE);
    }

    int isPassageApproved = FALSE;

    // Read passage result response from Eilat port through 'Med. Sea <== Red Sea' pipe.
    if (!ReadFile(readFromEilatHandle, buffer, BUFFER_SIZE, &numberOfReadBytes, NULL))
    {
        fprintf(stderr, "HaifaPort::suezCanalPassageApproval::Unexptected Error - "
            "reading passage answer from 'Med. Sea <== Red Sea' pipe failed\n");
        exit(EXIT_FAILURE);
    }

    isPassageApproved = atoi(buffer);

    sprintf(string, "Haifa Port: passage from Eilat Port %s!",
        isPassageApproved ? "approved" : "denied");

    if (!safePrintWithTimeStamp(string))
    {
        fprintf(stderr, "HaifaPort::Main::Unexpected Error - Print failed!\n");
        return 1;
    }

    if (!isPassageApproved)
    {
        exit(EXIT_SUCCESS);
    }
}

HANDLE* createVesselThreads(int numberOfVessels, int** vesselsId)
{
    DWORD threadId;
    *vesselsId = (int*)malloc(numberOfVessels * sizeof(int));
    HANDLE* vesselsHandler = (HANDLE*)malloc(numberOfVessels * sizeof(HANDLE));

    if (*vesselsId == NULL || vesselsHandler == NULL)
    {
        fprintf(stderr, "HaifaPort::createVesselThreads::Unexpected Error - "
            "Memory allocation failed!\n");
        exit(EXIT_FAILURE);
    }

    // Create all Vessel threads. ID starts with 1 till numberOfVessels.
    for (int i = 1; i <= numberOfVessels; i++)
    {
        int vesselIndex = i - 1;

        (*vesselsId)[vesselIndex] = i;
        vesselsHandler[vesselIndex] =
            CreateThread(NULL, 0, Vessel, &(*vesselsId)[vesselIndex], 0, &threadId);

        if (vesselsHandler[vesselIndex] == NULL)
        {
            fprintf(stderr, "HaifaPort::createVesselThreads::Unexpected Error - "
                "Vessel thread %d creation failed!\n", (*vesselsId)[vesselIndex]);
            exit(EXIT_FAILURE);
        }
    }

    return vesselsHandler;
}

void readIncomingVesselsFromEilatPort(int numberOfVessels)
{
    // Read incoming vessels from EilatPort and signal them to continue.
    for (int i = 0; i < numberOfVessels; i++)
    {
        if (!ReadFile(readFromEilatHandle, buffer, BUFFER_SIZE, &numberOfReadBytes, NULL))
        {
            fprintf(stderr, "HaifaPort::readIncomingVesselsFromEilatPort::Unexptected Error -"
                " Reading incoming vessel from 'Med. Sea <== Red Sea' pipe failed!\n");
            exit(EXIT_FAILURE);
        }

        int vesselId = atoi(buffer);

        // Signal that vessel has returned from EilatPort and continue its tasks.
        if (!ReleaseSemaphore(vesselsSemaphores[vesselId - 1], 1, NULL))
        {
            fprintf(stderr, "HaifaPort::readIncomingVesselsFromEilatPort::Unexpected Error -"
                "vesselsSemaphores[%d].V()\n", vesselId - 1);
            exit(EXIT_FAILURE);
        }
    }
}

void updateEilatAllVesselsDoneAndWaitForThreads(void)
{
    // Write to EilatPort that all vessel threads are done.
    // Comment: This command operates more as a cosmetic reason, since when the last thread has returend
    // EilatPort will start printing ending messages. With this EilatPort will wait till 
    // the end of all vessel's messages.
    sprintf(buffer, "%d", TRUE);

    if (!WriteFile(writeToEilatHandle, buffer, BUFFER_SIZE, &numberOfWrittenBytes, NULL))
    {
        fprintf(stderr, "HaifaPort::updateEilatAllVesselsDoneAndWaitForThreads::Unexpected Error -"
            " Writing that vessels ended has failed!\n");
        exit(EXIT_FAILURE);
    }

    // Check that all threads are done in EilatPort.
    if (!ReadFile(readFromEilatHandle, buffer, BUFFER_SIZE, &numberOfReadBytes, NULL))
    {
        fprintf(stderr, "HaifaPort::updateEilatAllVesselsDoneAndWaitForThreads::Unexptected Error -"
            " Reading EilatPort's end of threads has failed!\n");
        exit(EXIT_FAILURE);
    }

    if (!atoi(buffer))
    {
        fprintf(stderr, "HaifaPort::updateEilatAllVesselsDoneAndWaitForThreads::Unexptected Error -"
            " threads in EilatPort still exist!\n");
        exit(EXIT_FAILURE);
    }
}

void freeVesselThreads(HANDLE* vesselsHandler, int* vesselsId, int numberOfVessels)
{
    char string[MAX_STRING];

    sprintf(string, "Haifa Port: All Vessel Threads are done");

    if (!safePrintWithTimeStamp(string))
    {
        fprintf(stderr, "HaifaPort::Main::Unexpected Error - Print failed!\n");
        return 1;
    }

    for (int i = 0; i < numberOfVessels; i++)
    {
        CloseHandle(vesselsHandler[i]);
    }

    free(vesselsId);
    free(vesselsHandler);
}

int safePrintWithTimeStamp(char string[])
{
    WaitForSingleObject(processSafePrintSemaphore, INFINITE);

    GetLocalTime(&currentTime);
    fprintf(stderr, "[%02d:%02d:%02d] %s\n",
        currentTime.wHour, currentTime.wMinute, currentTime.wSecond, string);

    if (!ReleaseSemaphore(processSafePrintSemaphore, 1, NULL))
    {
        fprintf(stderr, "HaifaPort::safePrintWithTimeStamp::Unexpected Error - "
            "processSafePrintSemaphore.V()\n");
        return FALSE;
    }

    return TRUE;
}

DWORD WINAPI Vessel(LPVOID Param)
{
    // Get the thread's ID.
    int vesselId = *(int*)Param;

    // Comment: for some reason rand() kept producing the same values
    // even though the seed has been set at the main. As far as I'm aware
    // the seed only needs to be set once, even in a threaded enviorment 
    // (for example our for Home Tasks it was enough only in the main).
    // For that reason the seed is set in every thread which makes use of rand().
    // I would like to know what's the reason for this if possible
    srand((unsigned int)time(NULL));

    return startSailing(vesselId) ||
        sailToEilatPort(vesselId) ||
        returnFromEilatToEndSailing(vesselId);
}

int startSailing(int vesselId)
{
    char string[MAX_STRING];

    sprintf(string, "Vessel %2d - starts sailing @ Haifa Port", vesselId);

    if (!safePrintWithTimeStamp(string))
    {
        fprintf(stderr, "HaifaPort::Vessel %2d::Unexpected Error -"
            " Print failed!\n", vesselId);
        return 1;
    }

    Sleep(randomSleepTime());

    return 0;
}

int sailToEilatPort(int vesselId)
{
    // Allow only 1 vessel at a time to enter the canal (pipe).
    WaitForSingleObject(medToRedCanalSemaphore, INFINITE);

    char string[MAX_STRING];

    sprintf(string, "Vessel %2d - entering Canal: Med. Sea ==> Red Sea", vesselId);

    if (!safePrintWithTimeStamp(string))
    {
        fprintf(stderr, "HaifaPort::Vessel %2d::sailToEilatPort::Unexpected Error -"
            " Print failed!\n", vesselId);
        return 1;
    }

    Sleep(randomSleepTime());

    sprintf(buffer, "%d", vesselId);

    // Writing vessel ID to 'Med. Sea -> Red Sea' pipe.
    if (!WriteFile(writeToEilatHandle, buffer, BUFFER_SIZE, &numberOfWrittenBytes, NULL))
    {
        fprintf(stderr, "HaifaPort::Vessel %2d::sailToEilatPort::Unexpected Error -"
            " Writing vessel ID to 'Med. Sea ==> Red Sea' pipe failed\n", vesselId);
        return 1;
    }

    return 0;
}

int returnFromEilatToEndSailing(int vesselId)
{
    char string[MAX_STRING];

    // Wait for vessel to return from EilatPort
    WaitForSingleObject(vesselsSemaphores[vesselId - 1], INFINITE);

    sprintf(string, "Vessel %2d - exiting Canal: Red Sea ==> Med. Sea", vesselId);

    if (!safePrintWithTimeStamp(string))
    {
        fprintf(stderr, "HaifaPort::Vessel %2d::returnFromEilatToEndSailing::Unexpected Error -"
            " Print failed!\n", vesselId);
        return 1;
    }

    Sleep(randomSleepTime());

    // Signal that the 'Med. Sea <== Red Sea' pipe is free for another vessel to pass.
    if (!ReleaseSemaphore(redToMedCanalSemaphore, 1, NULL))
    {
        fprintf(stderr, "HaifaPort::Vessel %2d::returnFromEilatToEndSailing::Unexpected Error -"
            " medToRedCanalSemaphore.V()\n", vesselId);
        return 1;
    }

    sprintf(string, "Vessel %2d - done sailing @ Haifa Port", vesselId);

    if (!safePrintWithTimeStamp(string))
    {
        fprintf(stderr, "HaifaPort::Vessel %2d::returnFromEilatToEndSailing::Unexpected Error -"
            " Print failed!\n", vesselId);
        return 1;
    }

    return 0;
}