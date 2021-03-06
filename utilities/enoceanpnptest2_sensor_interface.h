/************************************************************************************************
 * This code was automatically generated by Digital Twin Code Generator tool 0.6.9.
 * Changes to this file may cause incorrect behavior and will be lost if the code is regenerated.
 *
 * Generated Date: 2020/06/07
 ***********************************************************************************************/

#ifndef ENOCEANPNPTEST2_SENSOR_INTERFACE_H
#define ENOCEANPNPTEST2_SENSOR_INTERFACE_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "digitaltwin_interface_client.h"
#include "azure_c_shared_utility/xlogging.h"
#include "azure_c_shared_utility/threadapi.h"

#include "digitaltwin_client_helper.h"
#include "digitaltwin_serializer.h"
#include "parson.h"
#include "../EnOceanPnPTest2_1yu_impl.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum ENOCEANPNPTEST2_SENSOR_TELEMETRY_TAG
{
    EnOceanPnPTest2_sensor_TEMPERATURE_TELEMETRY,
    EnOceanPnPTest2_sensor_HUMIDITY_TELEMETRY,
    EnOceanPnPTest2_sensor_ILLUMINATION_TELEMETRY,
    EnOceanPnPTest2_sensor_ACCELERATIONSTATUS_TELEMETRY,
    EnOceanPnPTest2_sensor_ACCELERATIONX_TELEMETRY,
    EnOceanPnPTest2_sensor_ACCELERATIONY_TELEMETRY,
    EnOceanPnPTest2_sensor_ACCELERATIONZ_TELEMETRY,
    EnOceanPnPTest2_sensor_CONTACT_TELEMETRY
} ENOCEANPNPTEST2_SENSOR_TELEMETRY;

// DigitalTwin interface name from service perspective.
static const char EnOceanPnPTest2_sensorInterfaceId[] = "urn:enoceanPnp:EnOceanPnPTest2_sensor:1";
static const char EnOceanPnPTest2_sensorInterfaceInstanceName[] = "EnOceanPnPTest2_sensor";

// Telemetry names for this interface.

static const char EnOceanPnPTest2_sensorInterface_TemperatureTelemetry[] = "temperature";

static const char EnOceanPnPTest2_sensorInterface_HumidityTelemetry[] = "humidity";

static const char EnOceanPnPTest2_sensorInterface_IlluminationTelemetry[] = "illumination";

static const char EnOceanPnPTest2_sensorInterface_AccelerationstatusTelemetry[] = "accelerationstatus";

static const char EnOceanPnPTest2_sensorInterface_AccelerationxTelemetry[] = "accelerationx";

static const char EnOceanPnPTest2_sensorInterface_AccelerationyTelemetry[] = "accelerationy";

static const char EnOceanPnPTest2_sensorInterface_AccelerationzTelemetry[] = "accelerationz";

static const char EnOceanPnPTest2_sensorInterface_ContactTelemetry[] = "Contact";

// Property names for this interface.

// Command names for this interface

// Methods
DIGITALTWIN_INTERFACE_CLIENT_HANDLE EnOceanPnPTest2_sensorInterface_Create();

void EnOceanPnPTest2_sensorInterface_Close(DIGITALTWIN_INTERFACE_CLIENT_HANDLE digitalTwinInterfaceClientHandle);

DIGITALTWIN_CLIENT_RESULT EnOceanPnPTest2_sensorInterface_Telemetry_SendAll();

#ifdef __cplusplus
}
#endif

#endif  // ENOCEANPNPTEST2_SENSOR_INTERFACE_H
