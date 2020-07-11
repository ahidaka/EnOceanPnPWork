//
// Core header files for C and IoTHub layer
//
#include <stdio.h>

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

#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/xlogging.h"
#include "pnp_device.h"

// IoT Central requires DPS.  Include required header and constants
#include "azure_prov_client/iothub_security_factory.h"
#include "azure_prov_client/prov_device_ll_client.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"
#include "azure_prov_client/prov_security_factory.h"

#ifdef SET_TRUSTED_CERT_IN_CODE
#include "certs.h"
#else
static const char *certificates = NULL;
#endif // SET_TRUSTED_CERT_IN_CODE

static bool iotHubConnected = false;

// State of DPS registration process.  We cannot proceed with DPS until we get into the state APP_DPS_REGISTRATION_SUCCEEDED.
typedef enum APP_DPS_REGISTRATION_STATUS_TAG
{
    APP_DPS_REGISTRATION_PENDING,
    APP_DPS_REGISTRATION_SUCCEEDED,
    APP_DPS_REGISTRATION_FAILED
} APP_DPS_REGISTRATION_STATUS;

const SECURE_DEVICE_TYPE secureDeviceTypeForProvisioning = SECURE_DEVICE_TYPE_SYMMETRIC_KEY;
const IOTHUB_SECURITY_TYPE secureDeviceTypeForIotHub = IOTHUB_SECURITY_TYPE_SYMMETRIC_KEY;

// The DPS global device endpoint
static const char *globalDpsEndpoint = "global.azure-devices-provisioning.net";

// DPS ID scope
static char *dpsIdScope;

// DPS symmetric keys for authentication
static char *sasKey;

// Device Id
static char *deviceId;

// TODO: Fill in DIGITALTWIN_DEVICE_CAPABILITY_MODEL_INLINE_DATA if want to make deivce self-describing.
#define DIGITALTWIN_DEVICE_CAPABILITY_MODEL_INLINE_DATA "{}"

static const char *digitalTwinSample_CustomProvisioningData = "{"
                                                              "\"__iot:interfaces\":"
                                                              "{"
                                                              "\"CapabilityModelId\": \"urn:enoceanPnp:EnOceanPnPTest2_1yu:1\" ,"
                                                              "\"CapabilityModel\": \"" DIGITALTWIN_DEVICE_CAPABILITY_MODEL_INLINE_DATA "\""
                                                              "}"
                                                              "}";

// Amount in ms to sleep between querying state from DPS registration loop
#define dpsRegistrationPollSleep 100

// Maximum amount of times we'll poll for DPS registration being ready, 1 min.
#define dpsRegistrationMaxPolls (60 * 1000 / dpsRegistrationPollSleep)

// State of DigitalTwin registration process.  We cannot proceed with DigitalTwin until we get into the state APP_DIGITALTWIN_REGISTRATION_SUCCEEDED.
typedef enum APP_DIGITALTWIN_REGISTRATION_STATUS_TAG
{
    APP_DIGITALTWIN_REGISTRATION_PENDING,
    APP_DIGITALTWIN_REGISTRATION_SUCCEEDED,
    APP_DIGITALTWIN_REGISTRATION_FAILED
} APP_DIGITALTWIN_REGISTRATION_STATUS;

#define IOT_HUB_CONN_STR_MAX_LEN 512

static char *dpsIotHubUri;
static char *dpsDeviceId;

///////// EnOcean staff /////////
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
    if (PidPath)
    {
        unlink(PidPath);
    }
    running = 0;
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

/////////////////////////////////////////

static void provisioningRegisterCallback(PROV_DEVICE_RESULT register_result, const char *iothub_uri, const char *device_id, void *user_context)
{
    APP_DPS_REGISTRATION_STATUS *appDpsRegistrationStatus = (APP_DPS_REGISTRATION_STATUS *)user_context;

    if (register_result != PROV_DEVICE_RESULT_OK)
    {
        LogError("DPS Provisioning callback called with error state %d", register_result);
        *appDpsRegistrationStatus = APP_DPS_REGISTRATION_FAILED;
    }
    else
    {
        if ((mallocAndStrcpy_s(&dpsIotHubUri, iothub_uri) != 0) ||
            (mallocAndStrcpy_s(&dpsDeviceId, device_id) != 0))
        {
            LogError("Unable to copy provisioning information");
            *appDpsRegistrationStatus = APP_DPS_REGISTRATION_FAILED;
        }
        else
        {
            LogInfo("Provisioning callback indicates success.  iothubUri=%s, deviceId=%s", dpsIotHubUri, dpsDeviceId);
            *appDpsRegistrationStatus = APP_DPS_REGISTRATION_SUCCEEDED;
        }
    }
}

static bool registerDevice(bool traceOn)
{
    PROV_DEVICE_RESULT provDeviceResult;
    PROV_DEVICE_LL_HANDLE provDeviceLLHandle = NULL;
    bool result = false;

    APP_DPS_REGISTRATION_STATUS appDpsRegistrationStatus = APP_DPS_REGISTRATION_PENDING;

    if (IoTHub_Init() != 0)
    {
        LogError("IoTHub_Init failed");
        return false;
    }

    if (prov_dev_set_symmetric_key_info(deviceId, sasKey) != 0)
    {
        LogError("prov_dev_set_symmetric_key_info failed.");
    }
    else if (prov_dev_security_init(secureDeviceTypeForProvisioning) != 0)
    {
        LogError("prov_dev_security_init failed");
    }
    else if ((provDeviceLLHandle = Prov_Device_LL_Create(globalDpsEndpoint, dpsIdScope, Prov_Device_MQTT_Protocol)) == NULL)
    {
        LogError("failed calling Prov_Device_Create");
    }
    else if ((provDeviceResult = Prov_Device_LL_SetOption(provDeviceLLHandle, PROV_OPTION_LOG_TRACE, &traceOn)) != PROV_DEVICE_RESULT_OK)
    {
        LogError("Setting provisioning tracing on failed, error=%d", provDeviceResult);
    }
#ifdef SET_TRUSTED_CERT_IN_CODE
    else if ((provDeviceResult = Prov_Device_LL_SetOption(provDeviceLLHandle, "TrustedCerts", certificates)) != PROV_DEVICE_RESULT_OK)
    {
        LogError("Setting provisioning TrustedCerts failed, error=%d", provDeviceResult);
    }
#endif // SET_TRUSTED_CERT_IN_CODE
    else if ((provDeviceResult = Prov_Device_LL_Set_Provisioning_Payload(provDeviceLLHandle, digitalTwinSample_CustomProvisioningData)) != PROV_DEVICE_RESULT_OK)
    {
        LogError("Failed setting provisioning data, error=%d", provDeviceResult);
    }
    else if ((provDeviceResult = Prov_Device_LL_Register_Device(provDeviceLLHandle, provisioningRegisterCallback, &appDpsRegistrationStatus, NULL, NULL)) != PROV_DEVICE_RESULT_OK)
    {
        LogError("Prov_Device_Register_Device failed, error=%d", provDeviceResult);
    }
    else
    {
        // Pulling the registration status
        for (int i = 0; (i < dpsRegistrationMaxPolls) && (appDpsRegistrationStatus == APP_DPS_REGISTRATION_PENDING); i++)
        {
            ThreadAPI_Sleep(dpsRegistrationPollSleep);
            Prov_Device_LL_DoWork(provDeviceLLHandle);
        }

        if (appDpsRegistrationStatus == APP_DPS_REGISTRATION_SUCCEEDED)
        {
            LogInfo("DPS successfully registered.  Continuing on to creation of IoTHub device client handle.");
            result = true;
        }
        else if (appDpsRegistrationStatus == APP_DPS_REGISTRATION_PENDING)
        {
            LogError("Timed out attempting to register DPS device");
        }
        else
        {
            LogError("Error registering device for DPS");
        }
    }

    if (provDeviceLLHandle != NULL)
    {
        Prov_Device_LL_Destroy(provDeviceLLHandle);
    }
    IoTHub_Deinit();

    return result;
}

static void setup()
{
    char buff[IOT_HUB_CONN_STR_MAX_LEN];
    iotHubConnected = false;

    // Initialize device model application
    if (registerDevice(false))
    {
        buff[0] = 0;
        if (secureDeviceTypeForProvisioning == SECURE_DEVICE_TYPE_SYMMETRIC_KEY)
        {
            snprintf(buff, sizeof(buff),
                     "HostName=%s;DeviceId=%s;SharedAccessKey=%s",
                     dpsIotHubUri,
                     dpsDeviceId,
                     sasKey);
        }
        else if (secureDeviceTypeForProvisioning == SECURE_DEVICE_TYPE_X509)
        {
            snprintf(buff, sizeof(buff),
                     "HostName=%s;DeviceId=%s;UseProvisioning=true",
                     dpsIotHubUri,
                     dpsDeviceId);
        }
        
        if (pnp_device_initialize(buff, certificates) == 0)
        {
            iotHubConnected = true;
            LogInfo("PnP enabled, running...");
        }
    }
}

// main entry point.
int main(int argc, char *argv[])
{
    pid_t myPid = getpid();
    int itemCount;
    int totalCount;
    FILE *f;

    if (argc == 4)
    {
        deviceId = argv[1];
        dpsIdScope = argv[2];
        sasKey = argv[3];
    }
    else
    {
        LogError("USAGE: enoceanpnp [Device ID] [DPS ID Scope] [DPS symmetric key]");
        return 1;
    }

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

    printf("PID=%d file=%s broker=%s\n", myPid, PidPath, BrokerPath);

    signal(SIGINT, stopHandler); /* catches ctrl-c */
    signal(SIGTERM, stopHandler); /* catches kill -15 */
    EoSignalAction(SIGENOCEAN, (void(*)(int)) ExamineEvent);

    EoReflesh();

    totalCount = 0;
    running = 1;

    setup();
    
    if (iotHubConnected)
    {
        EO_DATA *pe;
        int i;

        while (running)
        {
            pnp_device_run();

            for(i = 0; i < NODE_TABLE_SIZE; i++)
            {
                if (PatrolTable[i] == NoData)
                {
                    break;
                }
                else if (PatrolTable[i] == DataExists)
                {
                    double value;
                    int number;
                    itemCount = 0;
                    totalCount++;

                    while((pe = EoGetDataByIndex(i)) != NULL)
                    {
                        value = strtod(pe->Data, NULL);
                        printf("%d: %s: %s [%.3f]\n",
                           itemCount, pe->Name, pe->Desc, value);

                        switch(itemCount)
                        {
                        case 0:
                            eo_TP = value;
                            break;
                        case 1:
                            eo_HU = value;
                            break;
                        case 2:
                            eo_IL = value;
                            break;
                        case 3:
                            eo_AS = (int) strtoul(pe->Data, NULL, 10);
                            break;
                        case 4:
                            eo_AX = value;
                            break;
                        case 5:
                            eo_AY = value;
                            break;
                        case 6:
                            eo_AZ = value;
                            break;
                        case 7:
                            eo_CO = (int) strtoul(pe->Data, NULL, 10);
                            break;
                        default:
                            break;
                        }
                        itemCount++;
                    }
                    PatrolTable[i] = NoData;
                }
                if (itemCount == 0) {
                   printf("Found last line=%d\n", i);
                   break;
                }
            }

            ThreadAPI_Sleep(10 * 1000);
        }
    }

    return 0;
}
