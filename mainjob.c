
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

#include "AssociativeAarray.c"

#define MS_NUM_POINT (8) // Number of Multi Sensor points

static const char *KeyPoints[MS_NUM_POINT + 1] = {
	"TP", //0
	"HU", //1
	"IL", //2
	"AS", //3
	"AX", //4
	"AY", //5
	"AZ", //6
	"CO", //7
	NULL
};

int KeyIndex(char *Key)
{
	INT i;
	BOOL found = FALSE;

	for(i = 0; i < MS_NUM_POINT; i++) {
		if(!strcmp(Key, KeyPoints[i])) {
			found = TRUE;
			break;
		}
	}
	return(found ? i : -1);
}

char *IndexKey(int num)
{
	return((char *)KeyPoints[num & 7]);
}

//// export variables
double eo_TP;
double eo_HU;
double eo_IL;
int eo_AS;
double eo_AX;
double eo_AY;
double eo_AZ;
int eo_CO;

int
main(int argc, char *argv[])
{
	int i;
	int rtn, rtn2;
	int itemCount;
	int totalCount;
	pid_t myPid = getpid();
	FILE *f;
	char *key;
	char *value;
	EO_DATA *pe;

	AAInit(0);

	for(i = 0; i < MS_NUM_POINT; i++) {
		AASetStr(IndexKey(i), IndexKey(i));
	}

	for(i = 1; argv[i] != NULL; i++) {
		rtn = AASplit(argv[i], &key, &value);
		if (rtn > 0) {
			rtn = AASetStr(key, value);
			rtn2 = AASetStr(value, key);
			printf("AASet=%d,%d %s %s\n", rtn, rtn2, key, value);
		}
	}
	
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

#if 0
#define SC_SIZE (16)
#define NODE_TABLE_SIZE (256)
#define EO_DATSIZ (8)
typedef struct _eodata {
        int  Index;
        int  Id;
        char *Eep;
        char *Name;
        char *Desc;
        int  PIndex;
        int  PCount;
        char Data[EO_DATSIZ];
}
EO_DATA;
#endif

	while(running) {
		for(i = 0; i < NODE_TABLE_SIZE; i++) {
			if (PatrolTable[i] == NoData) {
				break;
			}
			else if (PatrolTable[i] == DataExists) {
				int indicator = 0;
				itemCount = 0;
				totalCount++;
				
				//TP,HU,IL,AS,AX,AY,AZ,CO
				while((pe = EoGetDataByIndex(i)) != NULL) {

					AASetData(pe->Name, pe->Data);

					printf("%d:%d:%d(%s) %s [%s]\n",
					       i, itemCount, indicator, pe->Name, pe->Desc, pe->Data);

					itemCount++;
				}
				PatrolTable[i] = NoData;
			}
			if (itemCount == 0) {
				printf("Found last line=%d\n", i);
				break;
			}
		} //for

		for(i = 0; i < 8; i++) {
			char *key = IndexKey(i);
			char *diverted;
			char data[64];
			double value = 0.00D;
			int number = 0;
			int status;

			diverted = AAGetStr(key);
			status = AAGetValidData(diverted, data);
			if (status >= 0) {
				if (!strcmp("AS", diverted) || !strcmp("CO", diverted)) {
					number = (int) strtoul(data, NULL, 10);
				}
				else {
					value = strtod(data, NULL);
				}

				printf("%s=%s [%s] num=%d dat=%03f\n",
					key, diverted, data, number, value);
			}
		} 

		//wait
		sleep(5);
	}

}
