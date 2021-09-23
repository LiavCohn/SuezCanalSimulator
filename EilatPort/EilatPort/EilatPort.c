#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <windows.h>
#include <stdlib.h>
#include <time.h>

#define True 1
#define False 0
#define MIN_SLEEP_TIME 5
#define MAX_SLEEP_TIME 3000
#define MAX_CARGO_WHIGHT 50
#define MIN_CARGO_WHIGHT 5
#define BUFFER_SIZE 1

enum CRANES_STATES { UNLOADING, RESTING };

void enter_UnloadingQuay(int id);
char* getTime();
int randRang(int max, int min);
int protectedRand1();
void initGlobalData();
int calcSleepTime();
void cleanupGlobalData();
void enterToCanalBack(int id);
int numberOfCrane(int numOfVessels);
int rand_cargo();
void canalListening();
void barrierFunction();
void enterBarrier(int id);

DWORD WINAPI Crane(PVOID Param);
DWORD WINAPI Eilat_Vessel(PVOID Param);


HANDLE mutex, randMutexEilat, mutexForM, queueMutex; // mutexs for random and for canal 
HANDLE barrierSem; // thread to randMutex
DWORD written,read;
HANDLE ReadHandle, WriteHandle;
int m;

int *working_cranes,*cranes_state,*crane_unloading,*barrier;
HANDLE* vesselsArr,*craneArr;
HANDLE* vesselsSem, * craneSem;
int prime(int n);
int numOfVessels, ptr, numOfCrane;
int head;

int main(int argc, char** argv)
{
	srand(time(NULL));
	CHAR buffer[BUFFER_SIZE];
	ReadHandle = GetStdHandle(STD_INPUT_HANDLE);
	WriteHandle = GetStdHandle(STD_OUTPUT_HANDLE);
	int isApproved = TRUE;
	char *str = "approved";


	/* Read form Haifa port the number of vessel */
	if (ReadFile(ReadHandle, buffer, BUFFER_SIZE, & read, NULL))
	{
		/*check if the number of vessel is a prime number*/
		numOfVessels = *buffer;
		fprintf(stderr, "%s checking number of vessels: %d if prime @Eilat port\n");
		if (prime(numOfVessels))
		{
			isApproved = FALSE;
			str = "disapproved";
		}
	}
	else
		fprintf(stderr, "Eilat Port: Error reading from pipe\n");

	//return to Hafia port if the number is prime by sending 0 = False ; 1 = True
	if (!WriteFile(WriteHandle, &isApproved, BUFFER_SIZE, &read, NULL))
		fprintf(stderr, "Eilat Port : Error writing to pipe\n");
	else
	{
		fprintf(stderr, "%s sending to Haifa port %s @Eilat port \n",getTime(), str);
	}

	 /*continue this process only if the number of vessels are not a prime 
		else close this pipes and exiting Eliat port.*/

	if (isApproved)
	{
		initGlobalData();
		canalListening();
		barrierFunction();

		WaitForMultipleObjects(numOfVessels,vesselsArr,TRUE,INFINITE);
		fprintf(stderr, "%s Eilat Port: All Vessel Thread are done\n", getTime());

		//close al handles and free memory!:
		for (int i = 0,j=0; i < numOfVessels; i++)
		{
			CloseHandle(vesselsArr[i]);
			CloseHandle(vesselsSem[i]);
			if (j < numOfCrane)
			{
				
				CloseHandle(craneSem[j]);
				CloseHandle(craneArr[j]);
				fprintf(stderr, "%s crane %d - closed @Eilat Port\n", getTime(), j+1);
				j++;

			}
		}

		WaitForMultipleObjects(numOfCrane, craneArr, TRUE, INFINITE);
		fprintf(stderr, "%s Eilat port: All Crane Thread are done\n", getTime());

		cleanupGlobalData();
	
	}
	CloseHandle(ReadHandle);
	CloseHandle(WriteHandle);
	fprintf(stderr, "%s Eilat Port: Exiting\n", getTime());
	exit(0);

}


DWORD WINAPI Eilat_Vessel(PVOID Param)
{
	int id = *(int*)Param;

	//Vessel arrive at Eilat Port
	fprintf(stderr,"%s Vessel %d - Arrived at Eilat Port @Eliat port\n", getTime(), id);
	Sleep(calcSleepTime());

	enterBarrier(id);

	WaitForSingleObject(vesselsSem[id - 1], INFINITE);

	Sleep(calcSleepTime());


	enter_UnloadingQuay(id);


	Sleep(calcSleepTime());

	//finish
	enterToCanalBack(id);

}

DWORD WINAPI Crane(PVOID Param)
{
	int id = *(int*)Param;
	while (True)
	{
		//if crane is not working-wait
		if (cranes_state[id-1]==RESTING)
		{
			WaitForSingleObject(craneSem[id - 1], INFINITE);		
		}
		Sleep(calcSleepTime());

		//print the crago that beging unload by the crane
		fprintf(stderr, "%s Crane %d - Unloaded %d Tons from Vessel %d @Eliat port\n", getTime(),id,crane_unloading[id-1],working_cranes[id-1]);
		

		cranes_state[id - 1] = RESTING;

		//tell the vessel that its done
		if(!ReleaseSemaphore(vesselsSem[working_cranes[id - 1]-1], 1, NULL))
		{
			fprintf(stderr, "Crane:: Unexpected error vesselsSem[%d].V @Eliat port() @Eliat port\n" ,working_cranes[id - 1]);
			exit(1);
		}
		//clean up	
		crane_unloading[id - 1] = -1;
		working_cranes[id - 1] = -1;

		//decrease number of active cranes
		WaitForSingleObject(mutexForM, INFINITE);
		m--;
		if (!ReleaseMutex(mutexForM))
		{
			fprintf(stderr, "Crane:: Unexpected error mutexForM.V @Eliat port() @Eliat port\n");
			exit(1);

		}
		// if this number is 0, we can release M vesseles from the barrier that are currently waiting.
		if(m==0)
			ReleaseSemaphore(barrierSem, 1, NULL);
	
	}

}

/*
this function simulate the UnloadingQuay it's called by the vessel and sent it id.
the vessel find the first crane who find it free to work.
*/

void enter_UnloadingQuay(int id)
{
	fprintf(stderr, "%s Vessel %d - Enter Unloading Quay @Eliat port\n", getTime(),id);

	Sleep(calcSleepTime());

	int crane = -1;
	//find the first available crane
	for (int i = 0; i < numberOfCrane; i++)
	{
		if (cranes_state[i] == RESTING)
		{
			crane = i+1;
			working_cranes[i] = id;
			break;
		}
	}

	//Abnormal Behaviour if crane is still -1;
	if (crane == -1)
	{
		printf("enter_UnloadingQuay::Abnormal Behaviour %d didn't find a free crane @Eliat port\n",id);
		exit(1);
	}

	//print info about which crane is serving the vessel
	fprintf(stderr, "%s Vessel %d - Stood infont of Crane %d @Eliat port\n", getTime(),id,crane);



	//unloading process beings
	int cargo = rand_cargo();

	fprintf(stderr, "%s Vessel %d - cargo is: %d tons @Eliat port\n", getTime(), id, cargo);


	//crane is now begining to unload the cargo
	cranes_state[crane - 1] = UNLOADING;
	crane_unloading[crane - 1] = cargo;

	if(!ReleaseSemaphore(craneSem[crane - 1], 1, NULL))
	{
		fprintf(stderr, "protectedRand Eilat:: Unexpected error randMutex.V()@Eliat port\n");
	}

	while (cranes_state[crane - 1] == UNLOADING)
	{
		//wait for the crane to finish the job
		WaitForSingleObject(vesselsSem[id - 1], INFINITE);
	}

	//leaving the unloading quay
	Sleep(calcSleepTime());

	fprintf(stderr, "%s Vessel %d - Leaving Unloading Quay @Eliat port\n", getTime(),id);




}

int prime(int n)
{
	int i;
	for (i = 2;i <= n / 2;i++)
	{
		if (n % i != 0)
			continue;
		else
			return 0;
	}
	return 1;
}


int rand_cargo()
{
	int res = rand();
	int num = (res % (MAX_CARGO_WHIGHT - MIN_CARGO_WHIGHT + 1)) + MIN_CARGO_WHIGHT;
	
	return num;
}



//better to write a generic function to randomise a Sleep time between MIN_SLEEP_TIME and 
//MAX_SLEEP_TIME msec
int randRang(int max, int min)
{
	return (protectedRand1() % (max- min+1 )) + min;
}
// a function to prevent race condition on rand
int protectedRand1()
{
	int res;
	WaitForSingleObject(randMutexEilat, INFINITE);
	res = rand();
	if (!ReleaseMutex(randMutexEilat))
	{
		fprintf(stderr, "protectedRand Eilat:: Unexpected error randMutex.V() @Eliat port\n");
	}
	return res;
}


void initGlobalData()
{
	DWORD ThreadId;
	//create mutex for critical work
	mutex = CreateMutex(NULL, FALSE, NULL);
	//create mutex for prevent race condition in the fuchtion rand
	randMutexEilat = CreateMutex(NULL, FALSE, NULL);

	//create mutex for prevent race condition in the update m
	mutexForM = CreateMutex(NULL, FALSE, NULL);

	//create mutex for prevent race condition in the update head if barrier
	queueMutex = CreateMutex(NULL, FALSE, NULL);

	if (mutex == NULL || randMutexEilat == NULL || mutexForM == NULL || queueMutex == NULL)
	{
		fprintf(stderr, "main::Unexpected Error in mutex/rand mutex Creation @Eliat port\n");
		return;
	}

	numOfCrane = numberOfCrane(numOfVessels); // number of crane devied by number of vasseles.
	m = 0;
	head = 0;

	// vassels arrays
	vesselsArr = (HANDLE*)malloc(numOfVessels * sizeof(HANDLE));
	vesselsSem = (HANDLE*)malloc(numOfVessels * sizeof(HANDLE));
	// crane arrays
	working_cranes = (int*)malloc(numOfCrane * sizeof(int)); //vessel match.
	crane_unloading = (int*)malloc(numOfCrane * sizeof(int)); //vessel cargo
	cranes_state = (int*)malloc(numOfCrane * sizeof(int)); // crane state
	craneArr = (HANDLE*)malloc(numOfCrane * sizeof(HANDLE));// crane thread
	craneSem = (HANDLE*)malloc(numOfCrane * sizeof(HANDLE));// crane semaphore

	//barrier fifo.
	barrier = (int*)malloc(numOfVessels * sizeof(int));

	//init cranes
	int *craneID = (int*)malloc(numOfCrane * sizeof(int)); //crane id
	for (int i = 0; i < numOfCrane; i++)
	{
		craneID[i] = i + 1;
		craneArr[i] = CreateThread(NULL, 0, Crane, &craneID[i], 0, &ThreadId);
		if (craneArr[i] == NULL) {
			fprintf(stderr, "main::Unexpected Error in Crane Thread %d Creation @Eliat port\n", i);
			exit(1);
		}
		craneSem[i] = CreateSemaphore(NULL, 0, 1, NULL);
		cranes_state[i] = RESTING;
		working_cranes[i] = -1;
		crane_unloading[i] = -1;

		fprintf(stderr, "%s crane %d - created @Eilat Port\n", getTime(), craneID[i]);

	}
	free(craneID);
	//creat barrier semaphore that will wait when m is != 0 .

	barrierSem = CreateSemaphore(NULL, 0, 1, NULL);
	if (barrierSem == NULL)
	{
		fprintf(stderr, "Main::Unexpected Error in Vessles Semaphore barrierSem Creation @Eliat Port\n");
		return FALSE;
	}
}


void enterToCanalBack(int id)
{
	WaitForSingleObject(mutex, INFINITE);

	fprintf(stderr, "%s Vessel %d - entring Canal:  Red Sea==> Med. Sea @Eliat port\n", getTime(),id);


	Sleep(calcSleepTime());//Simulate a process enter to the canal from Med. Sea towards Red Sea 

	if (!WriteFile(WriteHandle, &id, BUFFER_SIZE, &read, NULL))
		fprintf(stderr, "enterToCanal::Error writing to pipe @Eliat port\n");

	fprintf(stderr, "%s Vessel %d - Exiting Canal Red Sea==> Med.Sea @Eliat port\n", getTime(), id);

	if (!ReleaseMutex(mutex))
	{
		fprintf(stderr, "enterToCanal:: Unexpected error mutex.V() @Eliat port\n");
	}


}
int calcSleepTime()
{
	return protectedRand1() % (MAX_SLEEP_TIME - MIN_SLEEP_TIME + 1) + MIN_SLEEP_TIME;
}

void cleanupGlobalData()
{

	CloseHandle(mutex);
	CloseHandle(randMutexEilat);
	CloseHandle(mutexForM);
	CloseHandle(queueMutex);
	free(vesselsArr);
	free(vesselsSem);
	free(craneArr);
	free(craneSem);
	free(working_cranes);
	free(cranes_state);
	free(crane_unloading);
	free(barrier);
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


int numberOfCrane(int numOfVessels)
{
	int res;
	do
	{
		res = protectedRand1() % (numOfVessels - 2) + 2; // num of crane betweem 2 to num of vessels -1 

	} while (numOfVessels%res);
	return res;

}

void canalListening()
{
	DWORD ThreadId;
	CHAR id[BUFFER_SIZE];
	int* vesselsID = (int*)malloc(numOfVessels * sizeof(int));
	// read all vassels from Haifa port.
	for (int i = 0; i < numOfVessels; i++)
	{
		//if readfile is empty then it's stack.
		if (ReadFile(ReadHandle, id, BUFFER_SIZE, &read, NULL))
		{

			vesselsID[(*id) - 1] = (*id);
			vesselsSem[(*id) - 1] = CreateSemaphore(NULL, 0, 1, NULL);
			if (vesselsSem[(*id) - 1] == NULL)
			{
				fprintf(stderr, "Main::Unexpected Error in Vessles Semaphore %d Creation\n", (*id));
				return FALSE;
			}
			vesselsArr[(*id) - 1] = CreateThread(NULL, 0, Eilat_Vessel, &vesselsID[(*id) - 1], 0, &ThreadId);
			if (vesselsArr[(*id) - 1] == NULL) {
				fprintf(stderr, "main::Unexpected Error in Thread %d Creation \n", (*id));
				exit(1);
			}
		}
	}
	free(vesselsID);
}

void barrierFunction()
{
	int i = 0;
	while (i < numOfVessels)
	{
		// if number of active cranes is not 0, we need to wait until there are at least M available to work.
		if (m != 0)
			WaitForSingleObject(barrierSem, INFINITE);

		m = numOfCrane;
		for (int j = 0; j < numOfCrane; j++)
		{
			if (!ReleaseSemaphore(vesselsSem[barrier[i] - 1], 1, NULL))
			{
				fprintf(stderr, "main Eilat::Unexpected Error in releasing Vessel Sem[%d].V() in Barrier\n", barrier[i] - 1);
				exit(1);
			}

			i++;
		}
	}
}

void enterBarrier(int id)
{
	//prevent race condition on updateing the head of queue
	WaitForSingleObject(queueMutex, INFINITE);
	
	fprintf(stderr, "%s Vessel %d - enter to barrier @Eliat port\n", getTime(), id);

	barrier[head] = id;
	head++;

	if (!ReleaseMutex(queueMutex))
	{
		fprintf(stderr, "enterBarrier:: Unexpected error vesselID %d.V() @Eliat port @Eliat port\n", id);
		exit(1);
	}
}










