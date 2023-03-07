/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2019 Nest Labs, Inc.
 *    All rights reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/**
 *    @file
 *          Utility functions for working with OpenThread.
 */

#pragma once

#include <dlfcn.h>
#include <iostream>
#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <openthread/dataset.h>
#include <openthread/instance.h>
#include <openthread/link.h>
#include <openthread/netdata.h>
#include <openthread/thread.h>

#if CHIP_DEVICE_CONFIG_THREAD_FTD
#include <openthread/dataset_ftd.h>
#include <openthread/thread_ftd.h>
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD_SRP_CLIENT
#include <openthread/srp_client.h>
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD_DNS_CLIENT
#include <openthread/dns_client.h>
#endif

namespace chip {
namespace DeviceLayer {
namespace Internal {

typedef void (*otGetInstance)(otInstance ** instance, pthread_t * instanceId, const char * comPort, uint16_t debug);
typedef void (*otDestroyInstance)(void);
typedef void (*otLock)(void);
typedef void (*otUnlock)(void);

typedef otInstance * (*otInstanceInitSingle)(void);
typedef otError (*otSetStateChangedCallback)(otInstance * aInstance, otStateChangedCallback aCallback, void * aContext);
typedef void (*otIp6SetSlaacEnabled)(otInstance * aInstance, bool aEnabled);
typedef void (*otSrpClientSetCallback)(otInstance * aInstance, otSrpClientCallback aCallback, void * aContext);
typedef void (*otSrpClientEnableAutoStartMode)(otInstance * aInstance, otSrpClientAutoStartCallback aCallback, void * aContext);
typedef otDeviceRole (*otThreadGetDeviceRole)(otInstance * aInstance);
typedef bool (*otDatasetIsCommissioned)(otInstance * aInstance);
typedef otError (*otIp6SetEnabled)(otInstance * aInstance, bool aEnabled);
typedef otError (*otThreadSetEnabled)(otInstance * aInstance, bool aEnabled);
typedef const char * (*otThreadErrorToString)(otError aError);
typedef otError (*otNetDataGetNextRoute)(otInstance * aInstance, otNetworkDataIterator * aIterator,
                                         otExternalRouteConfig * aConfig);
typedef const otSrpClientHostInfo * (*otSrpClientGetHostInfo)(otInstance * aInstance);
typedef otError (*otThreadBecomeChild)(otInstance * aInstance);
typedef otError (*otDatasetSetActiveTlvs)(otInstance * aInstance, const otOperationalDatasetTlvs * aDataset);
typedef otError (*otDatasetGetActiveTlvs)(otInstance * aInstance, otOperationalDatasetTlvs * aDataset);
typedef otError (*otThreadDiscover)(otInstance * aInstance, uint32_t aScanChannels, uint16_t aPanId, bool aJoiner,
                                    bool aEnableEui64Filtering, otHandleActiveScanResult aCallback, void * aCallbackContext);
typedef otLinkModeConfig (*otThreadGetLinkMode)(otInstance * aInstance);
typedef bool (*otThreadIsRouterEligible)(otInstance * aInstance);
typedef otError (*otThreadSetRouterEligible)(otInstance * aInstance, bool aEligible);
typedef otError (*otThreadSetLinkMode)(otInstance * aInstance, otLinkModeConfig aConfig);
typedef void (*otThreadGetNetworkKey)(otInstance * aInstance, otNetworkKey * aNetworkKey);
typedef otError (*otThreadGetNextNeighborInfo)(otInstance * aInstance, otNeighborInfoIterator * aIterator, otNeighborInfo * aInfo);
typedef otError (*otDatasetGetActive)(otInstance * aInstance, otOperationalDataset * aDataset);
typedef const otMacCounters * (*otLinkGetCounters)(otInstance * aInstance);
typedef const otIpCounters * (*otThreadGetIp6Counters)(otInstance * aInstance);
typedef uint16_t (*otThreadGetRloc16)(otInstance * aInstance);
typedef uint8_t (*otThreadGetLeaderRouterId)(otInstance * aInstance);
typedef otError (*otThreadGetParentAverageRssi)(otInstance * aInstance, int8_t * aParentRssi);
typedef otError (*otThreadGetParentLastRssi)(otInstance * aInstance, int8_t * aLastRssi);
typedef uint32_t (*otThreadGetPartitionId)(otInstance * aInstance);
typedef void (*otPlatRadioGetIeeeEui64)(otInstance * aInstance, uint8_t * aIeeeEui64);
typedef const otExtAddress * (*otLinkGetExtendedAddress)(otInstance * aInstance);
typedef int8_t (*otPlatRadioGetRssi)(otInstance * aInstance);
typedef otError (*otThreadGetLeaderRloc)(otInstance * aInstance, otIp6Address * aLeaderRloc);
typedef uint8_t (*otThreadGetLeaderWeight)(otInstance * aInstance);
typedef uint8_t (*otThreadGetLocalLeaderWeight)(otInstance * aInstance);
typedef otError (*otNetDataGet)(otInstance * aInstance, bool aStable, uint8_t * aData, uint8_t * aDataLength);
typedef uint8_t (*otNetDataGetVersion)(otInstance * aInstance);
typedef uint8_t (*otNetDataGetStableVersion)(otInstance * aInstance);
#if CHIP_DEVICE_CONFIG_THREAD_FTD
typedef otError (*otThreadGetChildInfoById)(otInstance * aInstance, uint16_t aChildId, otChildInfo * aChildInfo);
#endif
typedef const otNetifAddress * (*otIp6GetUnicastAddresses)(otInstance * aInstance);
typedef uint8_t (*otLinkGetChannel)(otInstance * aInstance);
typedef const char * (*otThreadGetNetworkName)(otInstance * aInstance);
typedef otPanId (*otLinkGetPanId)(otInstance * aInstance);
typedef const otExtendedPanId * (*otThreadGetExtendedPanId)(otInstance * aInstance);
typedef const otMeshLocalPrefix * (*otThreadGetMeshLocalPrefix)(otInstance * aInstance);
typedef uint8_t (*otThreadGetMaxRouterId)(otInstance * aInstance);
typedef otError (*otThreadGetRouterInfo)(otInstance * aInstance, uint16_t aRouterId, otRouterInfo * aRouterInfo);
typedef otError (*otThreadGetParentInfo)(otInstance * aInstance, otRouterInfo * aParentInfo);
typedef const otMleCounters * (*otThreadGetMleCounters)(otInstance * aInstance);
typedef uint32_t (*otLinkGetPollPeriod)(otInstance * aInstance);
typedef bool (*otIp6IsEnabled)(otInstance * aInstance);
#if CHIP_DEVICE_CONFIG_THREAD_SSED
typedef uint16_t (*otLinkCslGetPeriod)(otInstance * aInstance);
typedef otError (*otLinkCslSetPeriod)(otInstance * aInstance, uint16_t aPeriod);
#endif
typedef otError (*otLinkSetPollPeriod)(otInstance * aInstance, uint32_t aPollPeriod);
typedef otError (*otInstanceErasePersistentInfo)(otInstance * aInstance);
typedef otJoinerState (*otJoinerGetState)(otInstance * aInstance);
typedef otError (*otJoinerSetDiscerner)(otInstance * aInstance, otJoinerDiscerner * aDiscerner);
typedef otError (*otJoinerStart)(otInstance * aInstance, const char * aPskd, const char * aProvisioningUrl,
                                 const char * aVendorName, const char * aVendorModel, const char * aVendorSwVersion,
                                 const char * aVendorData, otJoinerCallback aCallback, void * aContext);
typedef const otDnsQueryConfig * (*otDnsClientGetDefaultConfig)(otInstance * aInstance);
typedef void (*otDnsClientSetDefaultConfig)(otInstance * aInstance, const otDnsQueryConfig * aConfig);
typedef otError (*otSrpClientClearService)(otInstance * aInstance, otSrpClientService * aService);
typedef void (*otSrpClientSetLeaseInterval)(otInstance * aInstance, uint32_t aInterval);
typedef void (*otSrpClientSetKeyLeaseInterval)(otInstance * aInstance, uint32_t aInterval);
typedef otError (*otSrpClientAddService)(otInstance * aInstance, otSrpClientService * aService);
typedef otError (*otSrpClientRemoveService)(otInstance * aInstance, otSrpClientService * aService);
typedef otError (*otSrpClientSetHostName)(otInstance * aInstance, const char * aName);
typedef otError (*otSrpClientEnableAutoHostAddress)(otInstance * aInstance);
typedef otError (*otSrpClientSetHostAddresses)(otInstance * aInstance, const otIp6Address * aIp6Addresses, uint8_t aNumAddresses);
typedef otError (*otSrpClientRemoveHostAndServices)(otInstance * aInstance, bool aRemoveKeyLease, bool aSendUnregToServer);
typedef void (*otDnsInitTxtEntryIterator)(otDnsTxtEntryIterator * aIterator, const uint8_t * aTxtData, uint16_t aTxtDataLength);
typedef otError (*otDnsGetNextTxtEntry)(otDnsTxtEntryIterator * aIterator, otDnsTxtEntry * aEntry);
typedef otError (*otDnsBrowseResponseGetServiceName)(const otDnsBrowseResponse * aResponse, char * aNameBuffer,
                                                     uint16_t aNameBufferSize);
typedef otError (*otDnsBrowseResponseGetServiceInstance)(const otDnsBrowseResponse * aResponse, uint16_t aIndex,
                                                         char * aLabelBuffer, uint8_t aLabelBufferSize);
typedef otError (*otDnsBrowseResponseGetServiceInfo)(const otDnsBrowseResponse * aResponse, const char * aInstanceLabel,
                                                     otDnsServiceInfo * aServiceInfo);

typedef otError (*otDnsClientBrowse)(otInstance * aInstance, const char * aServiceName, otDnsBrowseCallback aCallback,
                                     void * aContext, const otDnsQueryConfig * aConfig);
typedef otError (*otDnsServiceResponseGetServiceName)(const otDnsServiceResponse * aResponse, char * aLabelBuffer,
                                                      uint8_t aLabelBufferSize, char * aNameBuffer, uint16_t aNameBufferSize);

typedef otError (*otDnsServiceResponseGetServiceInfo)(const otDnsServiceResponse * aResponse, otDnsServiceInfo * aServiceInfo);
typedef otError (*otDnsClientResolveService)(otInstance * aInstance, const char * aInstanceLabel, const char * aServiceName,
                                             otDnsServiceCallback aCallback, void * aContext, const otDnsQueryConfig * aConfig);
typedef uint16_t (*otMessageRead)(const otMessage * aMessage, uint16_t aOffset, void * aBuf, uint16_t aLength);
typedef uint16_t (*otMessageGetLength)(const otMessage * aMessage);

class OpenThreadDL_Manager
{

private:
    OpenThreadDL_Manager();

public:
    OpenThreadDL_Manager(const OpenThreadDL_Manager & src)         = delete;
    OpenThreadDL_Manager & operator=(const OpenThreadDL_Manager &) = delete;

    static OpenThreadDL_Manager & getInstance();

    virtual ~OpenThreadDL_Manager();

    bool open(const char * filename, int flags = RTLD_LAZY | RTLD_GLOBAL);
    void close();

    void * getSymbol(const char * symbolStr);

    void lookup();

public:
    otGetInstance mOtGetInstance;
    otDestroyInstance mOtDestroyInstance;
    otLock mOtLock;
    otUnlock mOtUnlock;
    otInstanceInitSingle mOtInstanceInitSingle;
    otSetStateChangedCallback mOtSetStateChangedCallback;
    otIp6SetSlaacEnabled mOtIp6SetSlaacEnabled;
    otSrpClientSetCallback mOtSrpClientSetCallback;
    otSrpClientEnableAutoStartMode mOtSrpClientEnableAutoStartMode;
    otThreadGetDeviceRole mOtThreadGetDeviceRole;
    otDatasetIsCommissioned mOtDatasetIsCommissioned;
    otIp6SetEnabled mOtIp6SetEnabled;
    otThreadSetEnabled mOtThreadSetEnabled;
    otThreadErrorToString mOtThreadErrorToString;
    otNetDataGetNextRoute mOtNetDataGetNextRoute;
    otSrpClientGetHostInfo mOtSrpClientGetHostInfo;
    otThreadBecomeChild mOtThreadBecomeChild;
    otDatasetSetActiveTlvs mOtDatasetSetActiveTlvs;
    otDatasetGetActiveTlvs mOtDatasetGetActiveTlvs;
    otThreadDiscover mOtThreadDiscover;
    otThreadGetLinkMode mOtThreadGetLinkMode;
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    otThreadIsRouterEligible mOtThreadIsRouterEligible;
    otThreadSetRouterEligible mOtThreadSetRouterEligible;
#endif
    otThreadSetLinkMode mOtThreadSetLinkMode;
    otThreadGetNetworkKey mOtThreadGetNetworkKey;
    otThreadGetNextNeighborInfo mOtThreadGetNextNeighborInfo;
    otDatasetGetActive mOtDatasetGetActive;
    otLinkGetCounters mOtLinkGetCounters;
    otThreadGetIp6Counters mOtThreadGetIp6Counters;
    otThreadGetRloc16 mOtThreadGetRloc16;
    otThreadGetLeaderRouterId mOtThreadGetLeaderRouterId;
    otThreadGetParentAverageRssi mOtThreadGetParentAverageRssi;
    otThreadGetParentLastRssi mOtThreadGetParentLastRssi;
    otThreadGetPartitionId mOtThreadGetPartitionId;
    otPlatRadioGetIeeeEui64 mOtPlatRadioGetIeeeEui64;
    otLinkGetExtendedAddress mOtLinkGetExtendedAddress;
    otPlatRadioGetRssi mOtPlatRadioGetRssi;
    otThreadGetLeaderRloc mOtThreadGetLeaderRloc;
    otThreadGetLeaderWeight mOtThreadGetLeaderWeight;
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    otThreadGetLocalLeaderWeight mOtThreadGetLocalLeaderWeight;
#endif
    otNetDataGet mOtNetDataGet;
    otNetDataGetVersion mOtNetDataGetVersion;
    otNetDataGetStableVersion mOtNetDataGetStableVersion;
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    otThreadGetChildInfoById mOtThreadGetChildInfoById;
#endif
    otIp6GetUnicastAddresses mOtIp6GetUnicastAddresses;
    otLinkGetChannel mOtLinkGetChannel;
    otThreadGetNetworkName mOtThreadGetNetworkName;
    otLinkGetPanId mOtLinkGetPanId;
    otThreadGetExtendedPanId mOtThreadGetExtendedPanId;
    otThreadGetMeshLocalPrefix mOtThreadGetMeshLocalPrefix;
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    otThreadGetMaxRouterId mOtThreadGetMaxRouterId;
    otThreadGetRouterInfo mOtThreadGetRouterInfo;
#endif
    otThreadGetParentInfo mOtThreadGetParentInfo;
    otThreadGetMleCounters mOtThreadGetMleCounters;
    otLinkGetPollPeriod mOtLinkGetPollPeriod;
    otIp6IsEnabled mOtIp6IsEnabled;
#if CHIP_DEVICE_CONFIG_THREAD_SSED
    otLinkCslGetPeriod mOtLinkCslGetPeriod;
    otLinkCslSetPeriod mOtLinkCslSetPeriod;
#endif
    otLinkSetPollPeriod mOtLinkSetPollPeriod;
    otInstanceErasePersistentInfo mOtInstanceErasePersistentInfo;
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    otJoinerGetState mOtJoinerGetState;
    otJoinerSetDiscerner mOtJoinerSetDiscerner;
    otJoinerStart mOtJoinerStart;
#endif
    otDnsClientGetDefaultConfig mOtDnsClientGetDefaultConfig;
    otDnsClientSetDefaultConfig mOtDnsClientSetDefaultConfig;
    otSrpClientClearService mOtSrpClientClearService;
    otSrpClientSetLeaseInterval mOtSrpClientSetLeaseInterval;
    otSrpClientSetKeyLeaseInterval mOtSrpClientSetKeyLeaseInterval;
    otSrpClientAddService mOtSrpClientAddService;
    otSrpClientRemoveService mOtSrpClientRemoveService;
    otSrpClientSetHostName mOtSrpClientSetHostName;
    otSrpClientEnableAutoHostAddress mOtSrpClientEnableAutoHostAddress;
    otSrpClientSetHostAddresses mOtSrpClientSetHostAddresses;
    otSrpClientRemoveHostAndServices mOtSrpClientRemoveHostAndServices;
    otDnsInitTxtEntryIterator mOtDnsInitTxtEntryIterator;
    otDnsGetNextTxtEntry mOtDnsGetNextTxtEntry;
    otDnsBrowseResponseGetServiceName mOtDnsBrowseResponseGetServiceName;
    otDnsBrowseResponseGetServiceInstance mOtDnsBrowseResponseGetServiceInstance;
    otDnsBrowseResponseGetServiceInfo mOtDnsBrowseResponseGetServiceInfo;
    otDnsClientBrowse mOtDnsClientBrowse;
    otDnsServiceResponseGetServiceName mOtDnsServiceResponseGetServiceName;
    otDnsServiceResponseGetServiceInfo mOtDnsServiceResponseGetServiceInfo;
    otDnsClientResolveService mOtDnsClientResolveService;
    otMessageRead mOtMessageRead;
    otMessageGetLength mOtMessageGetLength;

private:
    void * mHandle;
};

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip
