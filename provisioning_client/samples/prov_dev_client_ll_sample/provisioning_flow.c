// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "iothub_device_client_ll.h"

#include "azure_c_shared_utility/threadapi.h"
#include "azure_c_shared_utility/shared_util_options.h"
#include "azure_prov_client/prov_device_ll_client.h"

#include "provisioning_flow.h"

extern bool g_traceOn;

typedef struct PROV_SAMPLE_INFO_TAG
{
    unsigned int sleep_time;
    char* iothub_uri;
    char* access_key_name;
    char* device_key;
    char* device_id;
    int registration_complete;
} PROV_SAMPLE_INFO;

static void register_device_callback(PROV_DEVICE_RESULT register_result, const char* iothub_uri, const char* device_id, void* user_context)
{
    if (user_context == NULL)
    {
        printf("user_context is NULL\r\n");
    }
    else
    {
        PROV_SAMPLE_INFO* user_ctx = (PROV_SAMPLE_INFO*)user_context;
        if (register_result == PROV_DEVICE_RESULT_OK)
        {
            (void)printf("Registration Information received from service: %s!\r\n", iothub_uri);
            (void)mallocAndStrcpy_s(&user_ctx->iothub_uri, iothub_uri);
            (void)mallocAndStrcpy_s(&user_ctx->device_id, device_id);
            user_ctx->registration_complete = 1;
        }
        else
        {
            (void)printf("Failure encountered on registration %s\r\n", MU_ENUM_TO_STRING(PROV_DEVICE_RESULT, register_result));
            user_ctx->registration_complete = 2;
        }
    }
}

static void registration_status_callback(PROV_DEVICE_REG_STATUS reg_status, void* user_context)
{
    (void)user_context;
    (void)printf("Provisioning Status: %s\r\n", MU_ENUM_TO_STRING(PROV_DEVICE_REG_STATUS, reg_status));
}

IOTHUB_DEVICE_CLIENT_LL_HANDLE ProvisionIoThubDevice(const char* prov_uri, const char* id_scope, PROV_DEVICE_TRANSPORT_PROVIDER_FUNCTION prov_transport, IOTHUB_CLIENT_TRANSPORT_PROVIDER iothub_transport)
{
    PROV_SAMPLE_INFO prov_ctx;
    PROV_DEVICE_LL_HANDLE handle;

    memset(&prov_ctx, 0, sizeof(PROV_SAMPLE_INFO));
    prov_ctx.registration_complete = 0;
    prov_ctx.sleep_time = 10;

    if ((handle = Prov_Device_LL_Create(prov_uri, id_scope, prov_transport)) == NULL)
    {
        (void)printf("failed calling Prov_Device_LL_Create\r\n");
    }
    else
    {
        Prov_Device_LL_SetOption(handle, PROV_OPTION_LOG_TRACE, &g_traceOn);
#ifdef SET_TRUSTED_CERT_IN_SAMPLES
        // Setting the Trusted Certificate.  This is only necessary on system with without
        // built in certificate stores.
        Prov_Device_LL_SetOption(handle, OPTION_TRUSTED_CERT, certificates);
#endif // SET_TRUSTED_CERT_IN_SAMPLES

        // This option sets the registration ID it overrides the registration ID that is 
        // set within the HSM so be cautious if setting this value
        //Prov_Device_SetOption(prov_device_handle, PROV_REGISTRATION_ID, "[REGISTRATION ID]");

        if (Prov_Device_LL_Register_Device(handle, register_device_callback, &prov_ctx, registration_status_callback, &prov_ctx) != PROV_DEVICE_RESULT_OK)
        {
            (void)printf("failed calling Prov_Device_LL_Register_Device\r\n");
        }
        else
        {
            do
            {
                Prov_Device_LL_DoWork(handle);
                ThreadAPI_Sleep(prov_ctx.sleep_time);
            } while (prov_ctx.registration_complete == 0);
        }
        Prov_Device_LL_Destroy(handle);
    }
    IOTHUB_DEVICE_CLIENT_LL_HANDLE result;

    if (prov_ctx.registration_complete != 1)
    {
        (void)printf("registration with the provisioning service failed!\r\n");
        result = NULL;
    }
    else
    {
        (void)printf("Creating IoTHub Device handle\r\n");
        if ((result = IoTHubDeviceClient_LL_CreateFromDeviceAuth(prov_ctx.iothub_uri, prov_ctx.device_id, iothub_transport)) == NULL)
        {
            (void)printf("failed create IoTHub client from connection string %s!\r\n", prov_ctx.iothub_uri);
        }
        free(prov_ctx.iothub_uri);
        free(prov_ctx.device_id);
    }
    return result;
}
