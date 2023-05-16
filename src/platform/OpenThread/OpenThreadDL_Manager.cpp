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

#include <platform/OpenThread/OpenThreadDL_Manager.h>

namespace chip {
namespace DeviceLayer {
namespace Internal {

OpenThreadDL_Manager::OpenThreadDL_Manager() : mHandle(nullptr)
{
    mOtInstanceInitSingle      = nullptr;
    mOtSetStateChangedCallback = nullptr;
}

OpenThreadDL_Manager::~OpenThreadDL_Manager()
{
    close();
}

OpenThreadDL_Manager & OpenThreadDL_Manager::getInstance()
{
    static OpenThreadDL_Manager instance;
    return instance;
}

bool OpenThreadDL_Manager::open(const char * filename, int flags)
{
    bool ret = false;

    mHandle = dlopen(filename, flags);

    if (NULL == mHandle)
    {
        ChipLogProgress(DeviceLayer, "dlopen failed : %s", dlerror());
        ExitNow();
    }

    dlerror(); // Clear any existing error
    ret = true;
    ChipLogProgress(DeviceLayer, "dlopen success : %s", filename);
    lookup();
    ChipLogProgress(DeviceLayer, "symbol lookup success");
exit:
    return ret;
}

void OpenThreadDL_Manager::close()
{
    if (nullptr != mHandle)
    {
        dlclose(mHandle);
    }
    ChipLogProgress(DeviceLayer, "dlclose success");
}

void * OpenThreadDL_Manager::getSymbol(const char * symbolStr)
{
    void * func = nullptr;
    func        = dlsym(mHandle, symbolStr);

    if (NULL == func)
    {
        ChipLogProgress(DeviceLayer, "dlsym failed : %s", dlerror());
        ExitNow();
    }

    ChipLogProgress(DeviceLayer, "dlsym success : %s", symbolStr);

exit:
    return func;
}

void OpenThreadDL_Manager::lookup()
{
    mOtGetInstance                  = (otGetInstance) getSymbol("otGetInstance");
    mOtDestroyInstance              = (otDestroyInstance) getSymbol("otDestroyInstance");
    mOtLock                         = (otLock) getSymbol("otLock");
    mOtUnlock                       = (otUnlock) getSymbol("otUnlock");
    mOtInstanceInitSingle           = (otInstanceInitSingle) getSymbol("otInstanceInitSingle");
    mOtSetStateChangedCallback      = (otSetStateChangedCallback) getSymbol("otSetStateChangedCallback");
    mOtIp6SetSlaacEnabled           = (otIp6SetSlaacEnabled) getSymbol("otIp6SetSlaacEnabled");
    mOtSrpClientSetCallback         = (otSrpClientSetCallback) getSymbol("otSrpClientSetCallback");
    mOtSrpClientEnableAutoStartMode = (otSrpClientEnableAutoStartMode) getSymbol("otSrpClientEnableAutoStartMode");
    mOtThreadGetDeviceRole          = (otThreadGetDeviceRole) getSymbol("otThreadGetDeviceRole");
    mOtDatasetIsCommissioned        = (otDatasetIsCommissioned) getSymbol("otDatasetIsCommissioned");
    mOtIp6SetEnabled                = (otIp6SetEnabled) getSymbol("otIp6SetEnabled");
    mOtThreadSetEnabled             = (otThreadSetEnabled) getSymbol("otThreadSetEnabled");
    mOtThreadErrorToString          = (otThreadErrorToString) getSymbol("otThreadErrorToString");
    mOtNetDataGetNextRoute          = (otNetDataGetNextRoute) getSymbol("otNetDataGetNextRoute");
    mOtSrpClientGetHostInfo         = (otSrpClientGetHostInfo) getSymbol("otSrpClientGetHostInfo");
    mOtThreadBecomeChild            = (otThreadBecomeChild) getSymbol("otThreadBecomeChild");
    mOtDatasetSetActiveTlvs         = (otDatasetSetActiveTlvs) getSymbol("otDatasetSetActiveTlvs");
    mOtDatasetGetActiveTlvs         = (otDatasetGetActiveTlvs) getSymbol("otDatasetGetActiveTlvs");
    mOtThreadDiscover               = (otThreadDiscover) getSymbol("otThreadDiscover");
    mOtThreadGetLinkMode            = (otThreadGetLinkMode) getSymbol("otThreadGetLinkMode");
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    mOtThreadIsRouterEligible  = (otThreadIsRouterEligible) getSymbol("otThreadIsRouterEligible");
    mOtThreadSetRouterEligible = (otThreadSetRouterEligible) getSymbol("otThreadSetRouterEligible");
#endif
    mOtThreadSetLinkMode          = (otThreadSetLinkMode) getSymbol("otThreadSetLinkMode");
    mOtThreadGetNetworkKey        = (otThreadGetNetworkKey) getSymbol("otThreadGetNetworkKey");
    mOtThreadGetNextNeighborInfo  = (otThreadGetNextNeighborInfo) getSymbol("otThreadGetNextNeighborInfo");
    mOtDatasetGetActive           = (otDatasetGetActive) getSymbol("otDatasetGetActive");
    mOtLinkGetCounters            = (otLinkGetCounters) getSymbol("otLinkGetCounters");
    mOtThreadGetIp6Counters       = (otThreadGetIp6Counters) getSymbol("otThreadGetIp6Counters");
    mOtThreadGetRloc16            = (otThreadGetRloc16) getSymbol("otThreadGetRloc16");
    mOtThreadGetLeaderRouterId    = (otThreadGetLeaderRouterId) getSymbol("otThreadGetLeaderRouterId");
    mOtThreadGetParentAverageRssi = (otThreadGetParentAverageRssi) getSymbol("otThreadGetParentAverageRssi");
    mOtThreadGetParentLastRssi    = (otThreadGetParentLastRssi) getSymbol("otThreadGetParentLastRssi");
    mOtThreadGetPartitionId       = (otThreadGetPartitionId) getSymbol("otThreadGetPartitionId");
    mOtPlatRadioGetIeeeEui64      = (otPlatRadioGetIeeeEui64) getSymbol("otPlatRadioGetIeeeEui64");
    mOtLinkGetExtendedAddress     = (otLinkGetExtendedAddress) getSymbol("otLinkGetExtendedAddress");
    mOtPlatRadioGetRssi           = (otPlatRadioGetRssi) getSymbol("otPlatRadioGetRssi");
    mOtThreadGetLeaderRloc        = (otThreadGetLeaderRloc) getSymbol("otThreadGetLeaderRloc");
    mOtThreadGetLeaderWeight      = (otThreadGetLeaderWeight) getSymbol("otThreadGetLeaderWeight");
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    mOtThreadGetLocalLeaderWeight = (otThreadGetLocalLeaderWeight) getSymbol("otThreadGetLocalLeaderWeight");
#endif
    mOtNetDataGet              = (otNetDataGet) getSymbol("otNetDataGet");
    mOtNetDataGetVersion       = (otNetDataGetVersion) getSymbol("otNetDataGetVersion");
    mOtNetDataGetStableVersion = (otNetDataGetStableVersion) getSymbol("otNetDataGetStableVersion");
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    mOtThreadGetChildInfoById = (otThreadGetChildInfoById) getSymbol("otThreadGetChildInfoById");
#endif
    mOtIp6GetUnicastAddresses   = (otIp6GetUnicastAddresses) getSymbol("otIp6GetUnicastAddresses");
    mOtLinkGetChannel           = (otLinkGetChannel) getSymbol("otLinkGetChannel");
    mOtThreadGetNetworkName     = (otThreadGetNetworkName) getSymbol("otThreadGetNetworkName");
    mOtLinkGetPanId             = (otLinkGetPanId) getSymbol("otLinkGetPanId");
    mOtThreadGetExtendedPanId   = (otThreadGetExtendedPanId) getSymbol("otThreadGetExtendedPanId");
    mOtThreadGetMeshLocalPrefix = (otThreadGetMeshLocalPrefix) getSymbol("otThreadGetMeshLocalPrefix");
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    mOtThreadGetMaxRouterId = (otThreadGetMaxRouterId) getSymbol("otThreadGetMaxRouterId");
    mOtThreadGetRouterInfo  = (otThreadGetRouterInfo) getSymbol("otThreadGetRouterInfo");
#endif
    mOtThreadGetParentInfo  = (otThreadGetParentInfo) getSymbol("otThreadGetParentInfo");
    mOtThreadGetMleCounters = (otThreadGetMleCounters) getSymbol("otThreadGetMleCounters");
    mOtLinkGetPollPeriod    = (otLinkGetPollPeriod) getSymbol("otLinkGetPollPeriod");
    mOtIp6IsEnabled         = (otIp6IsEnabled) getSymbol("otIp6IsEnabled");
#if CHIP_DEVICE_CONFIG_THREAD_SSED
    mOtLinkCslGetPeriod = (otLinkCslGetPeriod) getSymbol("otLinkCslGetPeriod");
    mOtLinkCslSetPeriod = (otLinkCslSetPeriod) getSymbol("otLinkCslSetPeriod");
#endif
    mOtLinkSetPollPeriod           = (otLinkSetPollPeriod) getSymbol("otLinkSetPollPeriod");
    mOtInstanceErasePersistentInfo = (otInstanceErasePersistentInfo) getSymbol("otInstanceErasePersistentInfo");
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    mOtJoinerGetState     = (otJoinerGetState) getSymbol("otJoinerGetState");
    mOtJoinerSetDiscerner = (otJoinerSetDiscerner) getSymbol("otJoinerSetDiscerner");
    mOtJoinerStart        = (otJoinerStart) getSymbol("otJoinerStart");
#endif
    mOtDnsClientGetDefaultConfig       = (otDnsClientGetDefaultConfig) getSymbol("otDnsClientGetDefaultConfig");
    mOtDnsClientSetDefaultConfig       = (otDnsClientSetDefaultConfig) getSymbol("otDnsClientSetDefaultConfig");
    mOtSrpClientClearService           = (otSrpClientClearService) getSymbol("otSrpClientClearService");
    mOtSrpClientSetLeaseInterval       = (otSrpClientSetLeaseInterval) getSymbol("otSrpClientSetLeaseInterval");
    mOtSrpClientSetKeyLeaseInterval    = (otSrpClientSetKeyLeaseInterval) getSymbol("otSrpClientSetKeyLeaseInterval");
    mOtSrpClientAddService             = (otSrpClientAddService) getSymbol("otSrpClientAddService");
    mOtSrpClientRemoveService          = (otSrpClientRemoveService) getSymbol("otSrpClientRemoveService");
    mOtSrpClientSetHostName            = (otSrpClientSetHostName) getSymbol("otSrpClientSetHostName");
    mOtSrpClientEnableAutoHostAddress  = (otSrpClientEnableAutoHostAddress) getSymbol("otSrpClientEnableAutoHostAddress");
    mOtSrpClientSetHostAddresses       = (otSrpClientSetHostAddresses) getSymbol("otSrpClientSetHostAddresses");
    mOtSrpClientRemoveHostAndServices  = (otSrpClientRemoveHostAndServices) getSymbol("otSrpClientRemoveHostAndServices");
    mOtDnsInitTxtEntryIterator         = (otDnsInitTxtEntryIterator) getSymbol("otDnsInitTxtEntryIterator");
    mOtDnsGetNextTxtEntry              = (otDnsGetNextTxtEntry) getSymbol("otDnsGetNextTxtEntry");
    mOtDnsBrowseResponseGetServiceName = (otDnsBrowseResponseGetServiceName) getSymbol("otDnsBrowseResponseGetServiceName");
    mOtDnsBrowseResponseGetServiceInstance =
        (otDnsBrowseResponseGetServiceInstance) getSymbol("otDnsBrowseResponseGetServiceInstance");
    mOtDnsBrowseResponseGetServiceInfo  = (otDnsBrowseResponseGetServiceInfo) getSymbol("otDnsBrowseResponseGetServiceInfo");
    mOtDnsClientBrowse                  = (otDnsClientBrowse) getSymbol("otDnsClientBrowse");
    mOtDnsServiceResponseGetServiceName = (otDnsServiceResponseGetServiceName) getSymbol("otDnsServiceResponseGetServiceName");
    mOtDnsServiceResponseGetServiceInfo = (otDnsServiceResponseGetServiceInfo) getSymbol("otDnsServiceResponseGetServiceInfo");
    mOtDnsClientResolveService          = (otDnsClientResolveService) getSymbol("otDnsClientResolveService");
    mOtDnsClientResolveAddress          = (otDnsClientResolveAddress) getSymbol("otDnsClientResolveAddress");
    mOtDnsAddressResponseGetAddress     = (otDnsAddressResponseGetAddress) getSymbol("otDnsAddressResponseGetAddress");
    mOtMessageRead                      = (otMessageRead) getSymbol("otMessageRead");
    mOtMessageGetLength                 = (otMessageGetLength) getSymbol("otMessageGetLength");
}

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip
