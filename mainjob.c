
#include <signal.h>
#include <errno.h> // errno, EINTR
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define EXTERNAL_BROKER 1
#include "dpride/typedefs.h"
#include "dpride/utils.h"
#include "dpride/dpride.h"
#include "dpride/node.c"
#include "dpride/eologfile.c"
#include "dpride/EoControl.c"

static const char version[] = "\n@ enoceanpnp-test Version 0.1 \n";

#define EO_DIRECTORY "/var/tmp/dpride"
#define AZ_PID_FILE "azure.pid"
#define AZ_BROKER_FILE "brokers.txt"

#define SIGENOCEAN (SIGRTMIN + 6)

void EoSignalAction(int signo, void (*func)(int));
void ExamineEvent(int Signum, siginfo_t *ps, void *pt);
char *EoMakePath(char *Dir, char *File);
INT EoReflesh(void);
EO_DATA *EoGetDataByIndex(int Index);
FILE *EoLogInit(char *Prefix, char *Extension);
void EoLog(char *id, char *eep, char *msg);

typedef FILE* HANDLE;
typedef char TCHAR;
typedef int BOOL;

enum EventStatus {
	NoEntry = 0,
	NoData = 1,
	DataExists = 2
};

enum EventStatus PatrolTable[NODE_TABLE_SIZE];

static int running;
static char *PidPath;
static char *BrokerPath;

//
//
//
void EoSignalAction(int signo, void (*func)(int))
{
        struct sigaction act, oact;

        if (signo == SIGENOCEAN)
        {
                act.sa_sigaction = (void(*)(int, siginfo_t *, void *)) func;
                sigemptyset(&act.sa_mask);
                act.sa_flags = SA_SIGINFO;
        }
        else
        {
                act.sa_handler = func;
                sigemptyset(&act.sa_mask);
                act.sa_flags = SA_RESTART;
        }
        if (sigaction(signo, &act, &oact) < 0)
        {
                fprintf(stderr, "error at sigaction\n");
        }
}

void ExamineEvent(int Signum, siginfo_t *ps, void *pt)
{
	int index;
	char message[6] = {">>>0\n"};
	index = (unsigned long) ps->si_value.sival_int;
	PatrolTable[index] = DataExists;
	message[3] = '0' + index;
	write(1, message, 6);
}

//
//
//
static void stopHandler(int sign)
{
	if (PidPath) {
                unlink(PidPath);
	}
	running = 0;
}

int
main(int argc, char *argv[])
{
	int i;
	//int opt;
	int itemCount;
	int totalCount;
	pid_t myPid = getpid();
	FILE *f;
        EO_DATA *pe;

	printf(version);
	
    PidPath = EoMakePath(EO_DIRECTORY, AZ_PID_FILE);
    f = fopen(PidPath, "w");
    if (f == NULL)
    {
        fprintf(stderr, ": cannot create pid file=%s\n",
                PidPath);
        return 1;
    }
    fprintf(f, "%d\n", myPid);
    fclose(f);

    BrokerPath = EoMakePath(EO_DIRECTORY, AZ_BROKER_FILE);
    f = fopen(BrokerPath, "w");
    if (f == NULL)
    {
        fprintf(stderr, ": cannot create broker file=%s\n",
                BrokerPath);
        return 1;
    }
    fprintf(f, "azure\r\n");
    fclose(f);

    printf("PID=%d file=%s proker=%s\n", myPid, PidPath, BrokerPath);

	signal(SIGINT, stopHandler); /* catches ctrl-c */
	signal(SIGTERM, stopHandler); /* catches kill -15 */
	EoSignalAction(SIGENOCEAN, (void(*)(int)) ExamineEvent);

	EoReflesh();

	totalCount = 0;
	running = 1;

	while(running) {
		for(i = 0; i < NODE_TABLE_SIZE; i++) {
			if (PatrolTable[i] == NoData) {
				break;
			}
			else if (PatrolTable[i] == DataExists) {
				double value;
				itemCount = 0;
				totalCount++;
				
				while((pe = EoGetDataByIndex(i)) != NULL) {
					itemCount++;
					value = strtod(pe->Data, NULL);
					printf("%d: %s: %s [%.3f]\n",
					       itemCount, pe->Name, pe->Desc, value);
					itemCount++;
				}
				PatrolTable[i] = NoData;
			}
			if (itemCount == 0) {
				printf("Found last line=%d\n", i);
				break;
			}
		}
		//wait
		sleep(5);
	}
	
}
