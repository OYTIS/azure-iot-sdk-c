// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

// CAVEAT: This sample is to demonstrate azure IoT client concepts only and is not a guide design principles or style
// Checking of return codes and error values shall be omitted for brevity.  Please practice sound engineering practices
// when writing production code.
#include <stdio.h>
#include <stdlib.h>

#include "iothub.h"
#include "iothub_message.h"
#include "iothub_client_version.h"
#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/tickcounter.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_c_shared_utility/http_proxy_io.h"

#include "iothub_device_client_ll.h"
#include "iothub_client_options.h"

#include "azure_prov_client/prov_security_factory.h"

#include "provisioning_flow.h"

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
#include "certs.h"
#endif // SET_TRUSTED_CERT_IN_SAMPLES

//
// The protocol you wish to use should be uncommented
//
#define SAMPLE_MQTT
//#define SAMPLE_MQTT_OVER_WEBSOCKETS
//#define SAMPLE_AMQP
//#define SAMPLE_AMQP_OVER_WEBSOCKETS
//#define SAMPLE_HTTP

#ifdef SAMPLE_MQTT
#include "iothubtransportmqtt.h"
#include "azure_prov_client/prov_transport_mqtt_client.h"
#endif // SAMPLE_MQTT
#ifdef SAMPLE_MQTT_OVER_WEBSOCKETS
#include "iothubtransportmqtt_websockets.h"
#include "azure_prov_client/prov_transport_mqtt_ws_client.h"
#endif // SAMPLE_MQTT_OVER_WEBSOCKETS
#ifdef SAMPLE_AMQP
#include "iothubtransportamqp.h"
#include "azure_prov_client/prov_transport_amqp_client.h"
#endif // SAMPLE_AMQP
#ifdef SAMPLE_AMQP_OVER_WEBSOCKETS
#include "iothubtransportamqp_websockets.h"
#include "azure_prov_client/prov_transport_amqp_ws_client.h"
#endif // SAMPLE_AMQP_OVER_WEBSOCKETS
#ifdef SAMPLE_HTTP
#include "iothubtransportmqtt.h"
#include "azure_prov_client/prov_transport_http_client.h"
#endif // SAMPLE_HTTP

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
#include "certs.h"
#endif // SET_TRUSTED_CERT_IN_SAMPLES

// This sample is to demostrate iothub reconnection with provisioning and should not
// be confused as production code

MU_DEFINE_ENUM_STRINGS(PROV_DEVICE_RESULT, PROV_DEVICE_RESULT_VALUE);
MU_DEFINE_ENUM_STRINGS(PROV_DEVICE_REG_STATUS, PROV_DEVICE_REG_STATUS_VALUES);

static const char* global_prov_uri = "global.azure-devices-provisioning.net";
static const char* id_scope = "[ID Scope]";

bool g_traceOn = true;

#define MESSAGES_TO_SEND            2
#define TIME_BETWEEN_MESSAGES       2

typedef struct IOTHUB_CLIENT_SAMPLE_INFO_TAG
{
    int connected;
    int stop_running;
} IOTHUB_CLIENT_SAMPLE_INFO;

static IOTHUBMESSAGE_DISPOSITION_RESULT receive_msg_callback(IOTHUB_MESSAGE_HANDLE message, void* user_context)
{
    (void)message;
    IOTHUB_CLIENT_SAMPLE_INFO* iothub_info = (IOTHUB_CLIENT_SAMPLE_INFO*)user_context;
    (void)printf("Stop message recieved from IoTHub\r\n");
    iothub_info->stop_running = 1;
    return IOTHUBMESSAGE_ACCEPTED;
}

static void iothub_connection_status(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
{
    (void)reason;
    if (user_context == NULL)
    {
        printf("iothub_connection_status user_context is NULL\r\n");
    }
    else
    {
        IOTHUB_CLIENT_SAMPLE_INFO* iothub_info = (IOTHUB_CLIENT_SAMPLE_INFO*)user_context;
        if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
        {
            iothub_info->connected = 1;
        }
        else
        {
            iothub_info->connected = 0;
            iothub_info->stop_running = 1;
        }
    }
}

int main()
{
    // Initialize the HSM information
    SECURE_DEVICE_TYPE hsm_type;
    //hsm_type = SECURE_DEVICE_TYPE_TPM;
    hsm_type = SECURE_DEVICE_TYPE_X509;
    //hsm_type = SECURE_DEVICE_TYPE_SYMMETRIC_KEY;

    (void)IoTHub_Init();
    (void)prov_dev_security_init(hsm_type);
    // Set the symmetric key if using they auth type
    //prov_dev_set_symmetric_key_info("<symm_registration_id>", "<symmetric_Key>");

    PROV_DEVICE_TRANSPORT_PROVIDER_FUNCTION prov_transport;
    IOTHUB_CLIENT_TRANSPORT_PROVIDER iothub_transport;

    // Protocol to USE - HTTP, AMQP, AMQP_WS, MQTT, MQTT_WS
#ifdef SAMPLE_MQTT
    prov_transport = Prov_Device_MQTT_Protocol;
    iothub_transport = MQTT_Protocol;
#endif // SAMPLE_MQTT
#ifdef SAMPLE_MQTT_OVER_WEBSOCKETS
    prov_transport = Prov_Device_MQTT_WS_Protocol;
    iothub_transport = MQTT_WebSocket_Protocol;
#endif // SAMPLE_MQTT_OVER_WEBSOCKETS
#ifdef SAMPLE_AMQP
    prov_transport = Prov_Device_AMQP_Protocol;
    iothub_transport = AMQP_Protocol;
#endif // SAMPLE_AMQP
#ifdef SAMPLE_AMQP_OVER_WEBSOCKETS
    prov_transport = Prov_Device_AMQP_WS_Protocol;
    iothub_transport = AMQP_Protocol_over_WebSocketsTls;
#endif // SAMPLE_AMQP_OVER_WEBSOCKETS
#ifdef SAMPLE_HTTP
    prov_transport = Prov_Device_HTTP_Protocol;
    iothub_transport = MQTT_Protocol;
#endif // SAMPLE_HTTP

    printf("Provisioning API Version: %s\r\n", Prov_Device_LL_GetVersionString());
    printf("Iothub API Version: %s\r\n", IoTHubClient_GetVersionString());

    IOTHUB_DEVICE_CLIENT_LL_HANDLE device_ll_handle;

    (void)printf("Creating IoTHub Device handle\r\n");
    if ((device_ll_handle = ProvisionIoThubDevice(global_prov_uri, id_scope, prov_transport, iothub_transport) ) == NULL)
    {
        (void)printf("failed create IoTHub client from provisioning!\r\n");
    }
    else
    {
        IOTHUB_CLIENT_SAMPLE_INFO iothub_info;
        TICK_COUNTER_HANDLE tick_counter_handle = tickcounter_create();
        tickcounter_ms_t current_tick;
        tickcounter_ms_t last_send_time = 0;
        size_t msg_count = 0;
        iothub_info.stop_running = 0;
        iothub_info.connected = 0;

        (void)IoTHubDeviceClient_LL_SetConnectionStatusCallback(device_ll_handle, iothub_connection_status, &iothub_info);

        // Set any option that are neccessary.
        // For available options please see the iothub_sdk_options.md documentation

        IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_LOG_TRACE, &g_traceOn);

#ifdef SET_TRUSTED_CERT_IN_SAMPLES
        // Setting the Trusted Certificate.  This is only necessary on system with without
        // built in certificate stores.
        IoTHubDeviceClient_LL_SetOption(device_ll_handle, OPTION_TRUSTED_CERT, certificates);
#endif // SET_TRUSTED_CERT_IN_SAMPLES

        (void)IoTHubDeviceClient_LL_SetMessageCallback(device_ll_handle, receive_msg_callback, &iothub_info);

        (void)printf("Sending 1 messages to IoTHub every %d seconds for %d messages (Send any message to stop)\r\n", TIME_BETWEEN_MESSAGES, MESSAGES_TO_SEND);
        do
        {
            if (iothub_info.connected != 0)
            {
                // Send a message every TIME_BETWEEN_MESSAGES seconds
                (void)tickcounter_get_current_ms(tick_counter_handle, &current_tick);
                if ((current_tick - last_send_time) / 1000 > TIME_BETWEEN_MESSAGES)
                {
                    static char msgText[1024];
                    sprintf_s(msgText, sizeof(msgText), "{ \"message_index\" : \"%zu\" }", msg_count++);

                    IOTHUB_MESSAGE_HANDLE msg_handle = IoTHubMessage_CreateFromByteArray((const unsigned char*)msgText, strlen(msgText));
                    if (msg_handle == NULL)
                    {
                        (void)printf("ERROR: iotHubMessageHandle is NULL!\r\n");
                    }
                    else
                    {
                        if (IoTHubDeviceClient_LL_SendEventAsync(device_ll_handle, msg_handle, NULL, NULL) != IOTHUB_CLIENT_OK)
                        {
                            (void)printf("ERROR: IoTHubClient_LL_SendEventAsync..........FAILED!\r\n");
                        }
                        else
                        {
                            (void)tickcounter_get_current_ms(tick_counter_handle, &last_send_time);
                            (void)printf("IoTHubClient_LL_SendEventAsync accepted message [%zu] for transmission to IoT Hub.\r\n", msg_count);

                        }
                        IoTHubMessage_Destroy(msg_handle);
                    }
                }
            }
            IoTHubDeviceClient_LL_DoWork(device_ll_handle);
            ThreadAPI_Sleep(1);
        } while (iothub_info.stop_running == 0 && msg_count < MESSAGES_TO_SEND);

        size_t index = 0;
        for (index = 0; index < 10; index++)
        {
            IoTHubDeviceClient_LL_DoWork(device_ll_handle);
            ThreadAPI_Sleep(1);
        }
        tickcounter_destroy(tick_counter_handle);
        // Clean up the iothub sdk handle
        IoTHubDeviceClient_LL_Destroy(device_ll_handle);
    }

    prov_dev_security_deinit();

    // Free all the sdk subsystem
    IoTHub_Deinit();

    (void)printf("Press any enter to continue:\r\n");
    (void)getchar();

    return 0;
}
