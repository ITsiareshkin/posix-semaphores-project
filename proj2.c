#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SEMSCOUNT 7

// region Enums
enum PARAMS
{
	NE, // Number of Elves
    NR, // Number of Deers
    TE, // Max time Elf works alone
    TR  // Max time after which Deer returns home
};

enum SEMS
{
	OUT, SHARED, HELP, GOT, ACTION, HITCH, SLEEP
};
// endregion

// region Text arrays
const char* semNames[] =
{
	"/xtsiar00_out",
	"/xtsiar00_shared",
	"/xtsair00_help",
	"/xtsiar00_got",
	"/xtsiar00_action",
	"/xtsiar00_hitch",
	"/xtsiar00_sleep"
};

const char* errorMsgs[] =
{
    "ERROR: Wrong parameters. Expected: 0<NE<1000 0<NR<19 0<=TE<=1000 0<=TR<=1000",
	"ERROR: Not all semaphores were open",
	"ERROR: Can't open file",
	"ERROR: sem_wait doesn't work properly",
	"ERROR: sem_post doesn't work properly",
	"ERROR: Error creating shared memory",
	"ERROR: fork() error. All processes are killed (×_×)"
};
// endregion

// region Shared memory struct
typedef struct
{
	int actionCounter, waitingHelp, gotHelp, returnHome, getHitched, closeWorkshop, pidIndex;
	pid_t pids[1];
} Tshared;
// endregion

FILE* f = NULL;

// region parseParams function
int parseParams(int paramCount, char* paramString[], int* params)
{
	char* endptr;
	int i;

	if(paramCount != 4)
    {
        return 1;
    }

	for(i = 0; i < paramCount; ++i)
	{
		params[i] = strtol(paramString[i], &endptr, 10);
		if(*endptr != '\0' || params[i] < 0)
        {
            return 1;
        }
	}

	if(   params[NE] == 0 || params[NE] > 999    //elf
       || params[NR] == 0 || params[NR] > 19     //deer
	   || params[TE] < 0  || params[TE] > 1000   //elf work
	   || params[TR] < 0  || params[TR] > 1000 ) //deer return
    {
        return 1;
    }

	return 0;
}
// endregion

// region createSems function
int createSems(sem_t* semaphores[])
{
	for(int i = 0; i < SEMSCOUNT; i++)
	{
		semaphores[i] = sem_open(semNames[i], O_CREAT|O_EXCL, 0666, i < 2);
		if(semaphores[i] == SEM_FAILED)
        {
            return i;
        }
	}
	return SEMSCOUNT;
}
// endregion

// region createSharedMemory function
Tshared* createSharedMemory(int * params, int *shmid, int Elfs, int Deers)
{
	Tshared* shared;

	*shmid = shmget(IPC_PRIVATE, sizeof(Tshared)+sizeof(int)*Elfs+sizeof(int)*Deers, IPC_CREAT|0666);
	if(*shmid < 0)
    {
        return NULL;
    }

	shared = (Tshared*)shmat(*shmid, NULL, 0);
	if(shared == (void*) -1)
	{
		shmctl(*shmid, IPC_RMID, NULL);
		return NULL;
	}

	shared->pidIndex = 0;
	shared->waitingHelp = 0;
	shared->gotHelp = 0;
	shared->closeWorkshop = 0;
	shared->returnHome = params[NR]; // returnHome = Number of Deers
    shared->getHitched = params[NR]; // getHitched = Number of Deers

	return shared;
}
// endregion

// region outFile function (creates output file)
int outFile(FILE** f)
{
	*f = fopen("proj2.out", "w");
	if(*f == NULL)
	{
		return 1;
	}else
	{
		return 0;
	}
}
// endregion

// region closeSems
void closeSems(sem_t* semaphores[], int openSems)
{
	for(int i = 0; i < openSems; ++i)
	{
		sem_close(semaphores[i]);
	}
}
// endregion

// region unlinkSems function
void unlinkSems(int openSems)
{
	for(int i = 0; i < openSems; ++i)
	{
		sem_unlink(semNames[i]);
	}
}
// endregion

// region semWait function
// This function checks if sem_wait works properly
void semWait(sem_t* sem)
{
	int res = sem_wait(sem);
	if(res == -1)
	{
		fprintf(stderr, "%s\n", errorMsgs[3]);
		exit(-1);
	}
}
// endregion

// region semPost function
// This function checks if sem_post works properly
void semPost(sem_t* sem)
{
	int res = sem_post(sem);
	if(res == -1)
	{
		fprintf(stderr, "%s\n", errorMsgs[4]);
		exit(-1);
	}
}
// endregion

// region printStatus function
// This function prints information in a specific format to the output file
void printStatus(sem_t* out, Tshared* shared, const char* fmt, int ElfID, int rdID)
{
	semWait(out);
	++shared->actionCounter;

	// If ElfID = 0, rdID = 0, then function prints format "santa"
	// If ElfID = 0, rdID = rdID, then function prints format "RD"
	// If ElfID = ElfID, then function prints format "Elf"

	if(ElfID == 0)
	{
        if(rdID == 0)
        {
            fprintf(f, fmt, shared->actionCounter);
        }else
        {
            fprintf(f, fmt, shared->actionCounter, rdID);
        }
	}else
	{
		fprintf(f, fmt, shared->actionCounter, ElfID);
	}
	fflush(f);
	semPost(out);
}
// endregion

// region elf function
int elf(int* params, sem_t* semaphores[], Tshared* shared, int ElfID)
{
	printStatus(semaphores[OUT], shared, "%d: Elf %d: started\n", ElfID, 0);
	int r = (int)getpid();
	int i = 0;

	while(1)
	{
		srand(r);
		r = rand();
		usleep(r%(params[TE]+1)*1000);
		printStatus(semaphores[OUT], shared, "%d: Elf %d: need help\n", ElfID, 0);
		semWait(semaphores[SHARED]);
		++shared->waitingHelp;
		semPost(semaphores[SHARED]);

		if(shared->closeWorkshop == 1)
		{
			for(i = 0; i < shared->waitingHelp; i++)
			{
				semPost(semaphores[GOT]);
			}
			semWait(semaphores[SHARED]);
			shared->waitingHelp-=i;
			semPost(semaphores[SHARED]);

			break;
		}

		semWait(semaphores[GOT]);
		if(shared->closeWorkshop != 1)
		{
			printStatus(semaphores[OUT], shared, "%d: Elf %d: get help\n", ElfID, 0);
			semWait(semaphores[SHARED]);
			++shared->gotHelp;
			semPost(semaphores[SHARED]);

			if(shared->gotHelp == 3)
			{
				semPost(semaphores[SLEEP]);
			}
		}else
		{
			printStatus(semaphores[OUT], shared, "%d: Elf %d: taking holidays\n", ElfID, 0);
			shmdt(shared);
			closeSems(semaphores, SEMSCOUNT);
			break;
		}
	}

	printStatus(semaphores[OUT], shared, "%d: Elf %d: taking holidays\n", ElfID, 0);

	shmdt(shared);
	closeSems(semaphores, SEMSCOUNT);

	return 0;
}
// endregion

// region santa fucntion
int santa(int* params, sem_t* semaphores[], Tshared* shared)
{
	(void)(*params);
	printStatus(semaphores[OUT], shared, "%d: Santa: going to sleep\n", 0, 0);

	while(1)
	{
		if(shared->returnHome == 0)
		{	break;	}

		if(shared->waitingHelp >= 3)
		{
			printStatus(semaphores[OUT], shared, "%d: Santa: helping elves\n", 0, 0);
			semPost(semaphores[GOT]);
			semPost(semaphores[GOT]);
			semPost(semaphores[GOT]);

			semWait(semaphores[SLEEP]);
			printStatus(semaphores[OUT], shared, "%d: Santa: going to sleep\n", 0, 0);

			semWait(semaphores[SHARED]);
			shared->gotHelp-=3;
			shared->waitingHelp-=3;
			semPost(semaphores[SHARED]);
		}
	}

	printStatus(semaphores[OUT], shared, "%d: Santa: closing workshop\n", 0, 0);

	semWait(semaphores[SHARED]);
	++shared->closeWorkshop;
	semPost(semaphores[SHARED]);

	// If number of Elves < 3, then they will never get help
	if(params[NE] < 3)
	{
		for(int i = 0; i < params[NE]; i++)
		{
			semPost(semaphores[GOT]);
		}
	}

	if(shared->waitingHelp != 0)
	{
		for(int i = 0; i < shared->waitingHelp; i++)
		{
			semPost(semaphores[GOT]);
		}
	}

	while(1)
	{
		if(shared->getHitched == 0)
		{	break;	}
		semPost(semaphores[ACTION]);
		semWait(semaphores[HITCH]);
	}

	printStatus(semaphores[OUT], shared, "%d: Santa: Christmas started\n", 0, 0);

	shmdt(shared);
	closeSems(semaphores, SEMSCOUNT);
	return 0;
}
// endregion

// region deer fucntion
int deer(int* params, sem_t* semaphores[], Tshared* shared, int rdID)
{
	int r = (int)getpid();

	printStatus(semaphores[OUT], shared, "%d: RD %d: rstarted\n", 0, rdID);
	srand(r);
	r = rand();
	usleep(r%(params[TR]+1)*1000);

	printStatus(semaphores[OUT], shared, "%d: RD %d: return home\n", 0, rdID);
	semWait(semaphores[SHARED]);
	--shared->returnHome;
	semPost(semaphores[SHARED]);

	semWait(semaphores[ACTION]);

	printStatus(semaphores[OUT], shared, "%d: RD %d: get hitched\n", 0, rdID);
	semPost(semaphores[HITCH]);

	semWait(semaphores[SHARED]);
	--shared->getHitched;
	semPost(semaphores[SHARED]);

	semPost(semaphores[HITCH]);


	shmdt(shared);
	closeSems(semaphores, SEMSCOUNT);
	return 0;
}
// endregion

// region killAll function
// This function kills all processes if fork() doesn't work properly
void killAll(Tshared* shared)
{
	for(int i = 0; i < shared->pidIndex; ++i)
	{
		kill(shared->pids[i], SIGTERM);
	}
}
// endregion

// region clean function
void clean(sem_t* semaphores[], Tshared* shared, int shmid, int openSems)
{
	shmdt(shared);
	shmctl(shmid, IPC_RMID, NULL);
	closeSems(semaphores, openSems);
	unlinkSems(openSems);
    fclose(f);
}
// endregion

// region main function
int main(int argc, char* argv[])
{
	sem_t* semaphores[SEMSCOUNT];
	Tshared* shared;
	int openSems, file;

	int params[4];
	int forkRes, shmid, paramsRes = parseParams(argc-1, &argv[1], params);

	if(paramsRes != 0)
	{
		fprintf(stderr, "%s\n", errorMsgs[0]);
		return -1;
	}

	openSems = createSems(semaphores);
	if(openSems != SEMSCOUNT)
	{
		closeSems(semaphores, openSems);
		unlinkSems(openSems);

		fprintf(stderr, "%s\n", errorMsgs[1]);
		return 1;
	}

	shared = createSharedMemory(params, &shmid, params[NE], params[NR]);
	if(shared == NULL)
	{
		closeSems(semaphores, openSems);
		unlinkSems(SEMSCOUNT);

		fprintf(stderr, "%s\n", errorMsgs[5]);
		return 2;
	}

	file = outFile(&f);
	if(file != 0)
	{
		clean(semaphores, shared, shmid, openSems);

		fprintf(stderr, "%s\n", errorMsgs[2]);
		return 2;
	}

	// Creating Santa's process
	forkRes = fork();
	if(forkRes == 0)
	{
		semWait(semaphores[SHARED]);
		shared->pids[shared->pidIndex++] = getpid();
		semPost(semaphores[SHARED]);
		return santa(params, semaphores, shared);
	}else
    if(forkRes == -1)
	{
		fprintf(stderr, "%s\n", errorMsgs[6]);
		killAll(shared);
		clean(semaphores, shared, shmid, openSems);
		return 2;
	}

	// Creating Elves processes
	for(int i = 0; i < params[NE]; ++i)
	{
		forkRes = fork();
		if(forkRes == 0)
		{
			semWait(semaphores[SHARED]);
			shared->pids[shared->pidIndex++] = getpid();
			semPost(semaphores[SHARED]);
			return elf(params, semaphores, shared, i+1);
		}else
        if(forkRes == -1)
		{
			fprintf(stderr, "%s\n", errorMsgs[6]);
			killAll(shared);
			clean(semaphores, shared, shmid, openSems);
			return 2;
		}
	}

	// Creating Deers processes
    for(int i = 0; i < params[NR]; ++i)
    {
        forkRes = fork();
        if(forkRes == 0)
        {
            semWait(semaphores[SHARED]);
            shared->pids[shared->pidIndex++] = getpid();
            semPost(semaphores[SHARED]);
            return deer(params, semaphores, shared, i+1);
        }else
        if(forkRes == -1)
        {
            fprintf(stderr, "%s\n", errorMsgs[6]);
            killAll(shared);
            clean(semaphores, shared, shmid, openSems);
            return 2;
        }
    }

	// Waiting for end of all processes
	while(wait(NULL))
	{
		if(errno == ECHILD)
		{
			break;
		}
	}

	clean(semaphores, shared, shmid, openSems);
	return 0;
}
// endregion
