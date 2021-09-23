#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <time.h>

#define True 1
#define False 0
#define MIN_SLEEP_TIME 5
#define MAX_SLEEP_TIME 3000
#define BUFFER_SIZE 1

DWORD WINAPI Haifa_Vessel(PVOID Param);
char* getTime();
int calcSleepTime();
int protectedRand();
int initGlobalData();
void cleanupGlobalData();
void enterToCanal(int id);
void canalListening();

HANDLE mutex, randMutex; // mutexs for random and for canal 
HANDLE InputReadHandle, InputWriteHandle, OutputReadHandle, OutputWriteHandle; // pipes 
DWORD written;

int* vesselsID;
HANDLE* vesselsArr;
HANDLE* vesselsSem;
int numOfVessels;

int main(int argc, char* argv[])
{
	DWORD ThreadId;
	TCHAR ProcessName[256];
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
	CHAR isApproved[BUFFER_SIZE], vasselID[BUFFER_SIZE];
	srand(time(NULL));

	/* set up security attributes so that pipe handles are inherited */
	SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), NULL,TRUE };

	/* allocate memory */
	ZeroMemory(&pi, sizeof(pi));
	if (argc != 2)
	{
		fprintf(stderr, "There are incorrect number of arguments @Haifa port\n");
		exit(1);
	}

	numOfVessels = atoi(argv[1]);

	fprintf(stderr, "%s number of vessel is : %d @Haifa port\n", getTime(), numOfVessels);
	//check if the number is correct
	if (numOfVessels < 2 || numOfVessels > 50)
	{
		fprintf(stderr, "Wrong Value! Process Terminated. @Haifa port\n");
		exit(1);
	}
	/* create Input the pipe */
	if (!CreatePipe(&InputReadHandle, &InputWriteHandle, &sa, 0)) {
		fprintf(stderr, "Create Pipe Failed @Haifa port\n");
		return 1;
	}
	/* create Output the pipe */
	if (!CreatePipe(&OutputReadHandle, &OutputWriteHandle, &sa, 0)) {
		fprintf(stderr, "Create Pipe Failed @Haifa port\n");
		return 1;
	}

	/* establish the START_INFO structure for the child process */
	GetStartupInfo(&si);
	si.hStdError = GetStdHandle(STD_OUTPUT_HANDLE);

	/* redirect the standard input/output to the read end of the pipe */
	si.hStdOutput = OutputWriteHandle;
	si.hStdInput = InputReadHandle;
	si.dwFlags = STARTF_USESTDHANDLES;

	/* we do not want the child to inherit the write end of the pipe */
	SetHandleInformation(InputWriteHandle, HANDLE_FLAG_INHERIT, 0);
	
	wcscpy(ProcessName, L"..\\..\\EilatPort\\Debug\\EilatPort.exe");
	/* create the child process */
	if (!CreateProcess(NULL,
		ProcessName,
		NULL,
		NULL,
		TRUE, /* inherit handles */
		0,
		NULL,
		NULL,
		&si,
		&pi))
	{
		fprintf(stderr, "Process Creation Failed @Haifa port\n");
		return -1;
	}


	/* the Haifa now wants to write to the pipe */
	if (!WriteFile(InputWriteHandle, &numOfVessels, BUFFER_SIZE, &written, NULL))
		fprintf(stderr, "Error writing to pipe @Haifa port\n");
	else
		fprintf(stderr, "%s checking approval with Eilat port  @Haifa port\n",getTime(), numOfVessels);

	/* have the father read from the pipe */
	if (ReadFile(OutputReadHandle, isApproved, BUFFER_SIZE, &written, NULL))
	{
		if (!(*isApproved))
		{
			fprintf(stderr, "%s Eliat port gave negetive answer : %d @Haifa port\n", getTime());
			exit(1);
		}
		else
			fprintf(stderr, "%s Eliat port gave approval answer @Haifa port\n", getTime());

	}
	else
		fprintf(stderr, "Haifa Port: Error reading from pipe\n");


	// initialise Global Data (using initGlobalData function)
	if (!initGlobalData(numOfVessels))
	{
		fprintf(stderr, "Error:: initGlobalData function has failed. @Haifa port\n");
		exit(1);
	}

	// Create all Vessels Threads
	for (int i = 0; i < numOfVessels; i++)
	{
		vesselsID[i]= i+1;
		vesselsSem[i]= CreateSemaphore(NULL, 0, 1, NULL);
		if (vesselsSem[i] == NULL)
		{
			fprintf(stderr, "Main::Unexpected Error in Vessles Semaphore %d Creation @Haifa port\n", i);
			exit(1);
		}
		vesselsArr[i]= CreateThread(NULL, 0, Haifa_Vessel, &vesselsID[i], 0, &ThreadId);
		if (vesselsArr[i] == NULL) {
			fprintf(stderr,"main::Unexpected Error in Thread %d Creation @Haifa port\n", i);
			exit(1);
		}
	}

	canalListening();

	WaitForMultipleObjects(numOfVessels, vesselsArr, TRUE, INFINITE);

	fprintf(stderr, "%s Haifa Port: All Vessel Thread are done\n", getTime());
	WaitForSingleObject(pi.hProcess, INFINITE);

	
	CloseHandle(InputWriteHandle);
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	cleanupGlobalData();
	fprintf(stderr, "%s Haifa Port: Exiting\n", getTime());
}


DWORD WINAPI Haifa_Vessel(PVOID Param)
{
	int id = *(int*)Param;

	fprintf(stderr,"%s Vessel %d - starts sailing @Haifa Port \n", getTime(),id);
	Sleep(calcSleepTime());//Simulate a process Sailing towards the Canal

	enterToCanal(id);

	WaitForSingleObject(vesselsSem[id - 1],INFINITE);
	fprintf(stderr, "%s Vessel %d - arrived @Haifa Port\n", getTime(), id);

	Sleep(calcSleepTime());//Simulate a process of porting in Haifa port

	fprintf(stderr,"%s Vessel %d - done sailing @Haifa Port\n", getTime(),id);

}

//better to write a generic function to randomise a Sleep time between MIN_SLEEP_TIME and 
//MAX_SLEEP_TIME msec
int calcSleepTime()
{
	return protectedRand() % (MAX_SLEEP_TIME-MIN_SLEEP_TIME + 1) + MIN_SLEEP_TIME;
}
// a function to prevent race condition on rand
int protectedRand()
{
	int res;
	WaitForSingleObject(randMutex, INFINITE);
	res = rand();
	if (!ReleaseMutex(randMutex))
	{
		fprintf(stderr, "protectedRand Haifa:: Unexpected error randMutex.V() @Haifa port\n");
	}
	return res;
}


int initGlobalData()
{

	//create mutex for critical work
	mutex = CreateMutex(NULL, FALSE, NULL);
	//create mutex for prevent race condition in the fuchtion rand
	randMutex = CreateMutex(NULL, FALSE, NULL);

	if (mutex == NULL || randMutex == NULL)
	{
		fprintf(stderr, "main::Unexpected Error in mutex/rand mutex Creation @Haifa port\n");
		return FALSE;
	}

	vesselsArr = (HANDLE*)malloc(numOfVessels * sizeof(HANDLE));
	vesselsSem = (HANDLE*)malloc(numOfVessels * sizeof(HANDLE));
	vesselsID = (int*)malloc(numOfVessels * sizeof(int));

	return TRUE;

}
void enterToCanal(int id)
{
	WaitForSingleObject(mutex, INFINITE);
	fprintf(stderr, "%s Vessel %d - entring Canal: Med. Sea ==> Red Sea @Haifa port\n", getTime(),id);
	Sleep(calcSleepTime());//Simulate a process enter to the canal from Med. Sea towards Red Sea 

	if (!WriteFile(InputWriteHandle, &id, BUFFER_SIZE, &written, NULL))
		fprintf(stderr, "enterToCanal::Error writing to pipe @Haifa port\n");
	if (!ReleaseMutex(mutex))
	{
		fprintf(stderr, "enterToCanal:: Unexpected error mutex.V() @Haifa port\n");
	}
}


void cleanupGlobalData()
{
	/* close all handles */
	/* close Semaphore handles */

	CloseHandle(mutex);
	CloseHandle(randMutex);
	//free after colse handels
	for (int i = 0; i < numOfVessels; i++)
	{
		CloseHandle(vesselsSem[i]);
		CloseHandle(vesselsArr[i]);
	}

	free(vesselsArr);
	free(vesselsID);
	free(vesselsSem);
}


char* getTime()
{
	time_t rawtime;
	struct tm* tm_info;
	time(&rawtime);
	tm_info = localtime(&rawtime);
	char* now = asctime(tm_info);
	if (now != 0)
		strftime(now, 9, "%H:%M:%S", tm_info);
	return now;
}

void canalListening()
{
	CHAR vasselID[BUFFER_SIZE];
	//create canel listening = pipe listening. check if there is any vessle that returns to Haifa port
	int i = 0;
	while (i < numOfVessels)
	{
		//Sleep(calcSleepTime());//Simulate a process
		if (ReadFile(OutputReadHandle, vasselID, BUFFER_SIZE, &written, NULL))
		{
			if (!ReleaseSemaphore(vesselsSem[(*vasselID) - 1], 1, NULL))
			{
				fprintf(stderr, "CanelListening::Unexpected Error vesselsSem[%d].V() @Haifa port\n", (*vasselID) - 1);
			}
			i++;
		}
		else
			fprintf(stderr, "Haifa Port: Error reading from pipe\n");
	}

}