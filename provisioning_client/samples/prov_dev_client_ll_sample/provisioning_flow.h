// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stdio.h>
#include <stdlib.h>

#include "iothub_transport_ll.h"
#include "iothub_device_client_ll.h"
#include "azure_prov_client/prov_device_ll_client.h"

extern IOTHUB_DEVICE_CLIENT_LL_HANDLE ProvisionIoThubDevice(const char* prov_uri, const char* id_scope, PROV_DEVICE_TRANSPORT_PROVIDER_FUNCTION prov_transport, IOTHUB_CLIENT_TRANSPORT_PROVIDER iothub_transport);
