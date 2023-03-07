/*
 *
 *    Copyright (c) 2020-2022 Project CHIP Authors
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
 *          Contains non-inline method definitions for the
 *          GenericThreadStackManagerImpl_OpenThread<> template.
 */

#ifndef GENERIC_THREAD_STACK_MANAGER_IMPL_OPENTHREAD_IPP
#define GENERIC_THREAD_STACK_MANAGER_IMPL_OPENTHREAD_IPP

#include <cassert>

#include <openthread/cli.h>
#include <openthread/dataset.h>
#include <openthread/joiner.h>
#include <openthread/link.h>
#include <openthread/netdata.h>
#include <openthread/tasklet.h>
#include <openthread/thread.h>

#if CHIP_DEVICE_CONFIG_THREAD_FTD
#include <openthread/dataset_ftd.h>
#include <openthread/thread_ftd.h>
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD_SRP_CLIENT
#include <openthread/srp_client.h>
#endif

#include <app/AttributeAccessInterface.h>
#include <app/clusters/network-commissioning/network-commissioning.h>
#include <lib/core/CHIPEncoding.h>
#include <lib/support/CHIPMemString.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/FixedBufferAllocator.h>
#include <lib/support/ThreadOperationalDataset.h>
#include <lib/support/logging/CHIPLogging.h>
#if CHIP_SAMSUNG_UI_LOGGING
#include <lib/support/IoTer/IoTer_logging.h>
#endif
#include <platform/OpenThread/GenericNetworkCommissioningThreadDriver.h>
#include <platform/OpenThread/GenericThreadStackManagerImpl_OpenThread.h>
#include <platform/OpenThread/OpenThreadDL_Manager.h>
#include <platform/OpenThread/OpenThreadUtils.h>
#include <platform/ThreadStackManager.h>
#include <platform/internal/CHIPDeviceLayerInternal.h>

#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/MessageDef/AttributeDataIB.h>
#include <app/data-model/Encode.h>

#include <limits>
#if CHIP_SYSTEM_CONFIG_USE_OPEN_THREAD_ENDPOINT
#include <app/server/Server.h>
#endif // CHIP_SYSTEM_CONFIG_USE_OPEN_THREAD_ENDPOINT
extern "C" void otSysProcessDrivers(otInstance * aInstance);

#if CHIP_DEVICE_CONFIG_THREAD_ENABLE_CLI
extern "C" void otAppCliInit(otInstance * aInstance);
#endif

namespace chip {
namespace DeviceLayer {
namespace Internal {

// Network commissioning
namespace {
#ifndef _NO_NETWORK_COMMISSIONING_DRIVER_
NetworkCommissioning::GenericThreadDriver sGenericThreadDriver;
app::Clusters::NetworkCommissioning::Instance sThreadNetworkCommissioningInstance(0 /* Endpoint Id */, &sGenericThreadDriver);
#endif

void initNetworkCommissioningThreadDriver(void)
{
#ifndef _NO_NETWORK_COMMISSIONING_DRIVER_
    sThreadNetworkCommissioningInstance.Init();
#endif
}

NetworkCommissioning::otScanResponseIterator<NetworkCommissioning::ThreadScanResponse> mScanResponseIter;
} // namespace

static bool mOpenthreadInitialized;

/**
 * Called by OpenThread to alert the ThreadStackManager of a change in the state of the Thread stack.
 *
 * By default, applications never need to call this method directly.  However, applications that
 * wish to receive OpenThread state change call-backs directly from OpenThread (e.g. by calling
 * otSetStateChangedCallback() with their own callback function) can call this method to pass
 * state change events to the ThreadStackManager.
 */
template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::OnOpenThreadStateChange(uint32_t flags, void * context)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::OnOpenThreadStateChange 0x%x", flags);
    ChipDeviceEvent event;
    event.Type                                = DeviceEventType::kThreadStateChange;
    event.ThreadStateChange.RoleChanged       = (flags & OT_CHANGED_THREAD_ROLE) != 0;
    event.ThreadStateChange.AddressChanged    = (flags & (OT_CHANGED_IP6_ADDRESS_ADDED | OT_CHANGED_IP6_ADDRESS_REMOVED)) != 0;
    event.ThreadStateChange.NetDataChanged    = (flags & OT_CHANGED_THREAD_NETDATA) != 0;
    event.ThreadStateChange.ChildNodesChanged = (flags & (OT_CHANGED_THREAD_CHILD_ADDED | OT_CHANGED_THREAD_CHILD_REMOVED)) != 0;
    event.ThreadStateChange.OpenThread.Flags  = flags;

    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::OnOpenThreadStateChange - RoleChanged [%d]",
                    event.ThreadStateChange.RoleChanged);
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::OnOpenThreadStateChange - AddressChanged [%d]",
                    event.ThreadStateChange.AddressChanged);
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::OnOpenThreadStateChange - NetDataChanged [%d]",
                    event.ThreadStateChange.NetDataChanged);

    CHIP_ERROR status = PlatformMgr().PostEvent(&event);
    if (status != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "Failed to post Thread state change: %" CHIP_ERROR_FORMAT, status.Format());
    }

    DeviceLayer::SystemLayer().ScheduleLambda([]() { ThreadStackMgrImpl()._UpdateNetworkStatus(); });
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::_ProcessThreadActivity(void)
{
    otTaskletsProcess(mOTInst);
    otSysProcessDrivers(mOTInst);
}

template <class ImplClass>
bool GenericThreadStackManagerImpl_OpenThread<ImplClass>::_HaveRouteToAddress(const Inet::IPAddress & destAddr)
{
    bool res = false;

    // Lock OpenThread
    Impl()->LockThreadStack();

    // No routing of IPv4 over Thread.
    VerifyOrExit(!destAddr.IsIPv4(), res = false);

    // If the device is attached to a Thread network...
    if (IsThreadAttachedNoLock())
    {
        // Link-local addresses are always presumed to be routable, provided the device is attached.
        if (destAddr.IsIPv6LinkLocal())
        {
            ExitNow(res = true);
        }

        // Iterate over the routes known to the OpenThread stack looking for a route that covers the
        // destination address.  If found, consider the address routable.
        // Ignore any routes advertised by this device.
        // If the destination address is a ULA, ignore default routes. Border routers advertising
        // default routes are not expected to be capable of routing CHIP fabric ULAs unless they
        // advertise those routes specifically.
        {
            otError otErr;
            otNetworkDataIterator routeIter = OT_NETWORK_DATA_ITERATOR_INIT;
            otExternalRouteConfig routeConfig;
            const bool destIsULA = destAddr.IsIPv6ULA();

            while ((otErr = OpenThreadDL_Manager::getInstance().mOtNetDataGetNextRoute(Impl()->OTInstance(), &routeIter,
                                                                                       &routeConfig)) == OT_ERROR_NONE)
            {
                const Inet::IPPrefix prefix = ToIPPrefix(routeConfig.mPrefix);
                char addrStr[64];
                prefix.IPAddr.ToString(addrStr);
                if (!routeConfig.mNextHopIsThisDevice && (!destIsULA || routeConfig.mPrefix.mLength > 0) &&
                    ToIPPrefix(routeConfig.mPrefix).MatchAddress(destAddr))
                {
                    ExitNow(res = true);
                }
            }
        }
    }

exit:

    // Unlock OpenThread
    Impl()->UnlockThreadStack();

    return res;
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::_OnPlatformEvent(const ChipDeviceEvent * event)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_OnPlatformEvent 0x%04x", event->Type);
    if (event->Type == DeviceEventType::kThreadStateChange)
    {
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD_SRP_CLIENT && (OPENTHREAD_API_VERSION < 218)
        if (event->ThreadStateChange.AddressChanged)
        {
            const otSrpClientHostInfo * hostInfo =
                OpenThreadDL_Manager::getInstance().mOtSrpClientGetHostInfo(Impl()->OTInstance());
            if (hostInfo && hostInfo->mName)
            {
                Impl()->_SetupSrpHost(hostInfo->mName);
            }
        }
#endif

        bool isThreadAttached = Impl()->_IsThreadAttached();
        ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_OnPlatformEvent : old thread state [%d]",
                        mIsAttached);
        ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_OnPlatformEvent : new thread state [%d]",
                        isThreadAttached);
        // Avoid sending muliple events if the attachement state didn't change (Child->router or disable->Detached)
        // if (event->ThreadStateChange.RoleChanged && (isThreadAttached != mIsAttached))
        if (isThreadAttached) // fix connectNetwork error issue
        {
            ChipDeviceEvent attachEvent;
            attachEvent.Clear();
            attachEvent.Type                            = DeviceEventType::kThreadConnectivityChange;
            attachEvent.ThreadConnectivityChange.Result = (isThreadAttached) ? kConnectivity_Established : kConnectivity_Lost;
            CHIP_ERROR status                           = PlatformMgr().PostEvent(&attachEvent);
            if (status == CHIP_NO_ERROR)
            {
                mIsAttached = isThreadAttached;
            }
            else
            {
                ChipLogError(DeviceLayer, "Failed to post Thread connectivity change: %" CHIP_ERROR_FORMAT, status.Format());
            }
        }

#if CHIP_SYSTEM_CONFIG_USE_OPEN_THREAD_ENDPOINT
        if (event->ThreadStateChange.AddressChanged && isThreadAttached)
        {
            // Refresh Multicast listening
            ChipLogDetail(DeviceLayer, "Thread Attached updating Multicast address");
            Server::GetInstance().RejoinExistingMulticastGroups();
        }
#endif // CHIP_SYSTEM_CONFIG_USE_OPEN_THREAD_ENDPOINT

#if CHIP_DETAIL_LOGGING
        LogOpenThreadStateChange(mOTInst, event->ThreadStateChange.OpenThread.Flags);
#endif // CHIP_DETAIL_LOGGING
    }
}

template <class ImplClass>
bool GenericThreadStackManagerImpl_OpenThread<ImplClass>::_IsThreadEnabled(void)
{
    otDeviceRole curRole;

    Impl()->LockThreadStack();
    curRole = OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst);
    Impl()->UnlockThreadStack();

    return (curRole != OT_DEVICE_ROLE_DISABLED);
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_SetThreadEnabled(bool val)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_SetThreadEnabled [%d]", val);

    otError otErr = OT_ERROR_NONE;

    Impl()->LockThreadStack();

    bool isEnabled    = (OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst) != OT_DEVICE_ROLE_DISABLED);
    bool isIp6Enabled = OpenThreadDL_Manager::getInstance().mOtIp6IsEnabled(mOTInst);

    if (val && !isIp6Enabled)
    {
        otErr = OpenThreadDL_Manager::getInstance().mOtIp6SetEnabled(mOTInst, val);
        VerifyOrExit(otErr == OT_ERROR_NONE, );
    }

    if (val != isEnabled)
    {
        otErr = OpenThreadDL_Manager::getInstance().mOtThreadSetEnabled(mOTInst, val);
        VerifyOrExit(otErr == OT_ERROR_NONE, );
    }
/* jinwon ( when processing [thread stop], [ifconfig down] isn't processed.)
    if (!val && isIp6Enabled)
    {
        otErr = otIp6SetEnabled(mOTInst, val);
        VerifyOrExit(otErr == OT_ERROR_NONE, );
    }
*/
exit:
    Impl()->UnlockThreadStack();

    return MapOpenThreadError(otErr);
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_SetChild()
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_SetChild");

    otError otErr = OT_ERROR_NONE;

    Impl()->LockThreadStack();

    bool isRouter = (OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst) >= OT_DEVICE_ROLE_CHILD);

    if (isRouter)
    {
        otErr = OpenThreadDL_Manager::getInstance().mOtThreadBecomeChild(mOTInst);
        VerifyOrExit(otErr == OT_ERROR_NONE, );
    }

exit:
    Impl()->UnlockThreadStack();

    return MapOpenThreadError(otErr);
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_SetThreadProvision(ByteSpan netInfo)
{
    otError otErr = OT_ERROR_FAILED;
    otOperationalDatasetTlvs tlvs;

    assert(netInfo.size() <= Thread::kSizeOperationalDataset);
    tlvs.mLength = static_cast<uint8_t>(netInfo.size());
    memcpy(tlvs.mTlvs, netInfo.data(), netInfo.size());

    // Set the dataset as the active dataset for the node.
    Impl()->LockThreadStack();
    otErr = OpenThreadDL_Manager::getInstance().mOtDatasetSetActiveTlvs(mOTInst, &tlvs);
    Impl()->UnlockThreadStack();
    if (otErr != OT_ERROR_NONE)
    {
        return MapOpenThreadError(otErr);
    }

    // post an event alerting other subsystems about change in provisioning state
    ChipDeviceEvent event;
    event.Type                                           = DeviceEventType::kServiceProvisioningChange;
    event.ServiceProvisioningChange.IsServiceProvisioned = true;
    return PlatformMgr().PostEvent(&event);
}

template <class ImplClass>
bool GenericThreadStackManagerImpl_OpenThread<ImplClass>::_IsThreadProvisioned(void)
{
    bool provisioned;

    Impl()->LockThreadStack();
    provisioned = OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(mOTInst);
    Impl()->UnlockThreadStack();

    return provisioned;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_GetThreadProvision(Thread::OperationalDataset & dataset)
{
    VerifyOrReturnError(Impl()->IsThreadProvisioned(), CHIP_ERROR_INCORRECT_STATE);
    otOperationalDatasetTlvs datasetTlv;

    Impl()->LockThreadStack();
    otError otErr = OpenThreadDL_Manager::getInstance().mOtDatasetGetActiveTlvs(mOTInst, &datasetTlv);
    Impl()->UnlockThreadStack();
    if (otErr != OT_ERROR_NONE)
    {
        return MapOpenThreadError(otErr);
    }

    ReturnErrorOnFailure(dataset.Init(ByteSpan(datasetTlv.mTlvs, datasetTlv.mLength)));

    return CHIP_NO_ERROR;
}

template <class ImplClass>
bool GenericThreadStackManagerImpl_OpenThread<ImplClass>::_IsThreadAttached(void)
{
    otDeviceRole curRole;

    Impl()->LockThreadStack();
    curRole = OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst);
    if (curRole > OT_DEVICE_ROLE_ROUTER)
        _SetChild();
    Impl()->UnlockThreadStack();

    return (curRole != OT_DEVICE_ROLE_DISABLED && curRole != OT_DEVICE_ROLE_DETACHED);
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_AttachToThreadNetwork(
    const Thread::OperationalDataset & dataset, NetworkCommissioning::Internal::WirelessDriver::ConnectCallback * callback)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_AttachToThreadNetwork - start");

    // Reset the previously set callback since it will never be called in case incorrect dataset was supplied.
    mpConnectCallback = nullptr;

    if (_IsThreadAttached() && _IsWantedNetwork(dataset)) // already conntected to wanted thread network
    {
        ChipLogProgress(DeviceLayer,
                        "GenericThreadStackManagerImpl_OpenThread::_AttachToThreadNetwork - already conntected to wanted network");
        mpConnectCallback = callback;
        _OnThreadAttachFinished();
    }
    else
    {
        //    ReturnErrorOnFailure(Impl()->SetThreadEnabled(false));
        ChipLogProgress(DeviceLayer, "JINWON GenericThreadStackManagerImpl_OpenThread::_AttachToThreadNetwork - ThreadProvision");
        ReturnErrorOnFailure(Impl()->SetThreadProvision(dataset.AsByteSpan()));

        if (dataset.IsCommissioned())
        {
            ReturnErrorOnFailure(Impl()->SetThreadEnabled(true));
            ReturnErrorOnFailure(_SetChild());
            mpConnectCallback = callback;
        }

        ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_AttachToThreadNetwork - end");
    }

    return CHIP_NO_ERROR;
}
template <class ImplClass>
bool GenericThreadStackManagerImpl_OpenThread<ImplClass>::_IsWantedNetwork(Thread::OperationalDataset newDataset)
{

    // old connected network information
    const char * networkName_old = OpenThreadDL_Manager::getInstance().mOtThreadGetNetworkName(mOTInst);
    const otPanId panId_old      = OpenThreadDL_Manager::getInstance().mOtLinkGetPanId(mOTInst);
    otNetworkKey masterkey_old;
    uint8_t masterkey_old_uint8[16];
    OpenThreadDL_Manager::getInstance().mOtThreadGetNetworkKey(mOTInst, &masterkey_old);
    for (int i = 0; i < 16; i++)
        masterkey_old_uint8[i] = masterkey_old.m8[i];

    const otExtendedPanId * ePanId_old = OpenThreadDL_Manager::getInstance().mOtThreadGetExtendedPanId(mOTInst);
    ChipLogProgress(DeviceLayer, "-networkname:%s", networkName_old);
    ChipLogProgress(DeviceLayer, "-panid:0x%04X", panId_old);
    ChipLogProgress(DeviceLayer, "-masterkey:0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                    masterkey_old_uint8[0], masterkey_old_uint8[1], masterkey_old_uint8[2], masterkey_old_uint8[3],
                    masterkey_old_uint8[4], masterkey_old_uint8[5], masterkey_old_uint8[6], masterkey_old_uint8[7],
                    masterkey_old_uint8[8], masterkey_old_uint8[9], masterkey_old_uint8[10], masterkey_old_uint8[11],
                    masterkey_old_uint8[12], masterkey_old_uint8[13], masterkey_old_uint8[14], masterkey_old_uint8[15]);
    ChipLogProgress(DeviceLayer, "-extendpanid:0x%02X%02X%02X%02X%02X%02X%02X%02X", ePanId_old->m8[0], ePanId_old->m8[1],
                    ePanId_old->m8[2], ePanId_old->m8[3], ePanId_old->m8[4], ePanId_old->m8[5], ePanId_old->m8[6],
                    ePanId_old->m8[7]);

    // new wanted network information
    char networkName_new[17];
    uint16_t panId_new;
    uint8_t ePanId_new[8];
    uint8_t masterkey_new[16];
    // ByteSpan extPanId;
    newDataset.GetNetworkName(networkName_new);
    newDataset.GetPanId(panId_new);
    newDataset.GetMasterKey(masterkey_new);
    newDataset.GetExtendedPanId(ePanId_new);
    // memcpy(ePanId_new, extPanId.data(), extPanId.size());
    ChipLogProgress(DeviceLayer, "+networkname:%s", networkName_new);
    ChipLogProgress(DeviceLayer, "+panid:0x%04X", panId_new);
    ChipLogProgress(DeviceLayer, "+masterkey:0x%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", masterkey_new[0],
                    masterkey_new[1], masterkey_new[2], masterkey_new[3], masterkey_new[4], masterkey_new[5], masterkey_new[6],
                    masterkey_new[7], masterkey_new[8], masterkey_new[9], masterkey_new[10], masterkey_new[11], masterkey_new[12],
                    masterkey_new[13], masterkey_new[14], masterkey_new[15]);
    ChipLogProgress(DeviceLayer, "+extendpanid:0x%02X%02X%02X%02X%02X%02X%02X%02X", ePanId_new[0], ePanId_new[1], ePanId_new[2],
                    ePanId_new[3], ePanId_new[4], ePanId_new[5], ePanId_new[6], ePanId_new[7]);

    if (strcmp(networkName_old, networkName_new) == 0)
    {
        if (memcmp(ePanId_old, ePanId_new, 8) == 0)
        {
            if (panId_old == panId_new)
            {
                if (memcmp(masterkey_new, masterkey_old_uint8, 16) == 0)
                {
                    return true;
                }
            }
        }
    }
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread:_IsWantedNetwork : NOT same network");
    return false;
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::_OnThreadAttachFinished()
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_OnThreadAttachFinished");
    if (mpConnectCallback != nullptr)
    {
        DeviceLayer::SystemLayer().ScheduleLambda([this]() {
            VerifyOrReturn(mpConnectCallback != nullptr);
            mpConnectCallback->OnResult(NetworkCommissioning::Status::kSuccess, CharSpan(), 0);
            mpConnectCallback = nullptr;
        });
#if CHIP_SAMSUNG_UI_LOGGING
        IoTer::pipe_logging("step:9", IoTer::getSamsungDeviceNumber());
#endif
    }
}

template <class ImplClass>
CHIP_ERROR
GenericThreadStackManagerImpl_OpenThread<ImplClass>::_StartThreadScan(NetworkCommissioning::ThreadDriver::ScanCallback * callback)
{
    CHIP_ERROR error = CHIP_NO_ERROR;
#if CHIP_DEVICE_CONFIG_ENABLE_SED
    otLinkModeConfig linkMode;
#endif

    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_StartThreadScan");

    // If there is another ongoing scan request, reject the new one.
    VerifyOrReturnError(mpScanCallback == nullptr, CHIP_ERROR_INCORRECT_STATE);

    mpScanCallback = callback;

    Impl()->LockThreadStack();

    // Ensure that IPv6 interface is up when MLE Discovery is performed.
    if (!OpenThreadDL_Manager::getInstance().mOtIp6IsEnabled(mOTInst))
    {
        SuccessOrExit(error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtIp6SetEnabled(mOTInst, true)));
    }

#if CHIP_DEVICE_CONFIG_ENABLE_SED
    // Thread network discovery makes Sleepy End Devices detach from a network, so temporarily disable the SED mode.
    linkMode = OpenThreadDL_Manager::getInstance().mOtThreadGetLinkMode(mOTInst);

    if (!linkMode.mRxOnWhenIdle)
    {
        mTemporaryRxOnWhenIdle = true;
        linkMode.mRxOnWhenIdle = true;
        OpenThreadDL_Manager::getInstance().mOtThreadSetLinkMode(mOTInst, linkMode);
    }
#endif

    error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtThreadDiscover(
        mOTInst, 0,                       /* all channels */
        OT_PANID_BROADCAST, false, false, /* disable PAN ID, EUI64 and Joiner filtering */
        _OnNetworkScanFinished, this));

exit:
    Impl()->UnlockThreadStack();

    if (error != CHIP_NO_ERROR)
    {
        mpScanCallback = nullptr;
    }

    return error;
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::_OnNetworkScanFinished(otActiveScanResult * aResult, void * aContext)
{
    reinterpret_cast<GenericThreadStackManagerImpl_OpenThread *>(aContext)->_OnNetworkScanFinished(aResult);
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::_OnNetworkScanFinished(otActiveScanResult * aResult)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_OnNetworkScanFinished");

    if (aResult == nullptr) // scan completed
    {
#if CHIP_DEVICE_CONFIG_ENABLE_SED
        if (mTemporaryRxOnWhenIdle)
        {
            otLinkModeConfig linkMode = OpenThreadDL_Manager::getInstance().mOtThreadGetLinkMode(mOTInst);
            linkMode.mRxOnWhenIdle    = false;
            mTemporaryRxOnWhenIdle    = false;
            OpenThreadDL_Manager::getInstance().mOtThreadSetLinkMode(mOTInst, linkMode);
        }
#endif

        // If Thread scanning was done before commissioning, turn off the IPv6 interface.
        if (OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst) == OT_DEVICE_ROLE_DISABLED &&
            !OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(mOTInst))
        {
            DeviceLayer::SystemLayer().ScheduleLambda([this]() {
                Impl()->LockThreadStack();
                (void) !OpenThreadDL_Manager::getInstance().mOtIp6SetEnabled(mOTInst, false);
                Impl()->UnlockThreadStack();
            });
        }

        if (mpScanCallback != nullptr)
        {
            DeviceLayer::SystemLayer().ScheduleLambda([this]() {
                mpScanCallback->OnFinished(NetworkCommissioning::Status::kSuccess, CharSpan(), &mScanResponseIter);
                mpScanCallback = nullptr;
            });
        }
    }
    else
    {
        ChipLogProgress(DeviceLayer, "Thread Network: %s Panid 0x%x Channel %u RSSI %d LQI %u Version %u", aResult->mNetworkName.m8,
                        aResult->mPanId, aResult->mChannel, aResult->mRssi, aResult->mLqi, aResult->mVersion);

        NetworkCommissioning::ThreadScanResponse scanResponse = { 0 };

        scanResponse.panId           = aResult->mPanId;   // why is scanResponse.panID 64b
        scanResponse.channel         = aResult->mChannel; // why is scanResponse.channel 16b
        scanResponse.version         = aResult->mVersion;
        scanResponse.rssi            = aResult->mRssi;
        scanResponse.lqi             = aResult->mLqi;
        scanResponse.extendedAddress = Encoding::BigEndian::Get64(aResult->mExtAddress.m8);
        scanResponse.extendedPanId   = Encoding::BigEndian::Get64(aResult->mExtendedPanId.m8);
        static_assert(OT_NETWORK_NAME_MAX_SIZE <= UINT8_MAX, "Network name length won't fit");
        scanResponse.networkNameLen = static_cast<uint8_t>(strnlen(aResult->mNetworkName.m8, OT_NETWORK_NAME_MAX_SIZE));
        memcpy(scanResponse.networkName, aResult->mNetworkName.m8, scanResponse.networkNameLen);

        mScanResponseIter.Add(&scanResponse);
    }
}

template <class ImplClass>
ConnectivityManager::ThreadDeviceType GenericThreadStackManagerImpl_OpenThread<ImplClass>::_GetThreadDeviceType(void)
{
    ConnectivityManager::ThreadDeviceType deviceType;

    Impl()->LockThreadStack();

    const otLinkModeConfig linkMode = OpenThreadDL_Manager::getInstance().mOtThreadGetLinkMode(mOTInst);

#if CHIP_DEVICE_CONFIG_THREAD_FTD
    if (linkMode.mDeviceType && OpenThreadDL_Manager::getInstance().mOtThreadIsRouterEligible(mOTInst))
        ExitNow(deviceType = ConnectivityManager::kThreadDeviceType_Router);
    if (linkMode.mDeviceType)
        ExitNow(deviceType = ConnectivityManager::kThreadDeviceType_FullEndDevice);
#endif
    if (linkMode.mRxOnWhenIdle)
        ExitNow(deviceType = ConnectivityManager::kThreadDeviceType_MinimalEndDevice);

#if CHIP_DEVICE_CONFIG_THREAD_SSED
    if (OpenThreadDL_Manager::getInstance().mOtLinkCslGetPeriod(mOTInst) != 0)
        ExitNow(deviceType = ConnectivityManager::kThreadDeviceType_SynchronizedSleepyEndDevice);
#endif

    ExitNow(deviceType = ConnectivityManager::kThreadDeviceType_SleepyEndDevice);

exit:
    Impl()->UnlockThreadStack();

    return deviceType;
}

template <class ImplClass>
CHIP_ERROR
GenericThreadStackManagerImpl_OpenThread<ImplClass>::_SetThreadDeviceType(ConnectivityManager::ThreadDeviceType deviceType)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    otLinkModeConfig linkMode;

    switch (deviceType)
    {
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    case ConnectivityManager::kThreadDeviceType_Router:
    case ConnectivityManager::kThreadDeviceType_FullEndDevice:
#endif
    case ConnectivityManager::kThreadDeviceType_MinimalEndDevice:
    case ConnectivityManager::kThreadDeviceType_SleepyEndDevice:
#if CHIP_DEVICE_CONFIG_THREAD_SSED
    case ConnectivityManager::kThreadDeviceType_SynchronizedSleepyEndDevice:
#endif
        break;
    default:
        ExitNow(err = CHIP_ERROR_INVALID_ARGUMENT);
    }

#if CHIP_PROGRESS_LOGGING
    {
        const char * deviceTypeStr;
        switch (deviceType)
        {
        case ConnectivityManager::kThreadDeviceType_Router:
            deviceTypeStr = "ROUTER";
            break;
        case ConnectivityManager::kThreadDeviceType_FullEndDevice:
            deviceTypeStr = "FULL END DEVICE";
            break;
        case ConnectivityManager::kThreadDeviceType_MinimalEndDevice:
            deviceTypeStr = "MINIMAL END DEVICE";
            break;
        case ConnectivityManager::kThreadDeviceType_SleepyEndDevice:
            deviceTypeStr = "SLEEPY END DEVICE";
            break;
#if CHIP_DEVICE_CONFIG_THREAD_SSED
        case ConnectivityManager::kThreadDeviceType_SynchronizedSleepyEndDevice:
            deviceTypeStr = "SYNCHRONIZED SLEEPY END DEVICE";
            break;
#endif
        default:
            deviceTypeStr = "(unknown)";
            break;
        }
        ChipLogProgress(DeviceLayer, "Setting OpenThread device type to %s", deviceTypeStr);
    }

#endif // CHIP_PROGRESS_LOGGING

    Impl()->LockThreadStack();

    linkMode = OpenThreadDL_Manager::getInstance().mOtThreadGetLinkMode(mOTInst);

    switch (deviceType)
    {
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    case ConnectivityManager::kThreadDeviceType_Router:
    case ConnectivityManager::kThreadDeviceType_FullEndDevice:
        linkMode.mDeviceType   = true;
        linkMode.mRxOnWhenIdle = true;
        (void) !OpenThreadDL_Manager::getInstance().mOtThreadSetRouterEligible(
            mOTInst, deviceType == ConnectivityManager::kThreadDeviceType_Router);
        break;
#endif
    case ConnectivityManager::kThreadDeviceType_MinimalEndDevice:
        linkMode.mDeviceType   = false;
        linkMode.mRxOnWhenIdle = true;
        break;
    case ConnectivityManager::kThreadDeviceType_SleepyEndDevice:
    case ConnectivityManager::kThreadDeviceType_SynchronizedSleepyEndDevice:
        linkMode.mDeviceType   = false;
        linkMode.mRxOnWhenIdle = false;
        break;
    default:
        break;
    }

    (void) !OpenThreadDL_Manager::getInstance().mOtThreadSetLinkMode(mOTInst, linkMode);

    Impl()->UnlockThreadStack();

exit:
    return err;
}

template <class ImplClass>
bool GenericThreadStackManagerImpl_OpenThread<ImplClass>::_HaveMeshConnectivity(void)
{
    bool res;
    otDeviceRole curRole;

    Impl()->LockThreadStack();

    // Get the current Thread role.
    curRole = OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst);

    // If Thread is disabled, or the node is detached, then the node has no mesh connectivity.
    if (curRole == OT_DEVICE_ROLE_DISABLED || curRole == OT_DEVICE_ROLE_DETACHED)
    {
        res = false;
    }

    // If the node is a child, that implies the existence of a parent node which provides connectivity
    // to the mesh.
    else if (curRole == OT_DEVICE_ROLE_CHILD)
    {
        res = true;
    }

    // Otherwise, if the node is acting as a router, scan the Thread neighbor table looking for at least
    // one other node that is also acting as router.
    else
    {
        otNeighborInfoIterator neighborIter = OT_NEIGHBOR_INFO_ITERATOR_INIT;
        otNeighborInfo neighborInfo;

        res = false;

        while (OpenThreadDL_Manager::getInstance().mOtThreadGetNextNeighborInfo(mOTInst, &neighborIter, &neighborInfo) ==
               OT_ERROR_NONE)
        {
            if (!neighborInfo.mIsChild)
            {
                res = true;
                break;
            }
        }
    }

    Impl()->UnlockThreadStack();

    return res;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_GetAndLogThreadStatsCounters(void)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    otError otErr;
    otOperationalDataset activeDataset;

    Impl()->LockThreadStack();
#if CHIP_PROGRESS_LOGGING
    {
        otDeviceRole role;

        role = OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst);
        ChipLogProgress(DeviceLayer, "Thread Role:                  %d\n", role);
    }
#endif // CHIP_PROGRESS_LOGGING

    if (OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(mOTInst))
    {
        otErr = OpenThreadDL_Manager::getInstance().mOtDatasetGetActive(mOTInst, &activeDataset);
        VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

        if (activeDataset.mComponents.mIsChannelPresent)
        {
            ChipLogProgress(DeviceLayer, "Thread Channel:               %d\n", activeDataset.mChannel);
        }
    }

#if CHIP_PROGRESS_LOGGING
    {
        const otIpCounters * ipCounters;
        const otMacCounters * macCounters;

        macCounters = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst);

        ChipLogProgress(DeviceLayer,
                        "Rx Counters:\n"
                        "PHY Rx Total:                 %" PRIu32 "\n"
                        "MAC Rx Unicast:               %" PRIu32 "\n"
                        "MAC Rx Broadcast:             %" PRIu32 "\n"
                        "MAC Rx Data:                  %" PRIu32 "\n"
                        "MAC Rx Data Polls:            %" PRIu32 "\n"
                        "MAC Rx Beacons:               %" PRIu32 "\n"
                        "MAC Rx Beacon Reqs:           %" PRIu32 "\n"
                        "MAC Rx Other:                 %" PRIu32 "\n"
                        "MAC Rx Filtered Whitelist:    %" PRIu32 "\n"
                        "MAC Rx Filtered DestAddr:     %" PRIu32 "\n",
                        macCounters->mRxTotal, macCounters->mRxUnicast, macCounters->mRxBroadcast, macCounters->mRxData,
                        macCounters->mRxDataPoll, macCounters->mRxBeacon, macCounters->mRxBeaconRequest, macCounters->mRxOther,
                        macCounters->mRxAddressFiltered, macCounters->mRxDestAddrFiltered);

        ChipLogProgress(DeviceLayer,
                        "Tx Counters:\n"
                        "PHY Tx Total:                 %" PRIu32 "\n"
                        "MAC Tx Unicast:               %" PRIu32 "\n"
                        "MAC Tx Broadcast:             %" PRIu32 "\n"
                        "MAC Tx Data:                  %" PRIu32 "\n"
                        "MAC Tx Data Polls:            %" PRIu32 "\n"
                        "MAC Tx Beacons:               %" PRIu32 "\n"
                        "MAC Tx Beacon Reqs:           %" PRIu32 "\n"
                        "MAC Tx Other:                 %" PRIu32 "\n"
                        "MAC Tx Retry:                 %" PRIu32 "\n"
                        "MAC Tx CCA Fail:              %" PRIu32 "\n",
                        macCounters->mTxTotal, macCounters->mTxUnicast, macCounters->mTxBroadcast, macCounters->mTxData,
                        macCounters->mTxDataPoll, macCounters->mTxBeacon, macCounters->mTxBeaconRequest, macCounters->mTxOther,
                        macCounters->mTxRetry, macCounters->mTxErrCca);

        ChipLogProgress(DeviceLayer,
                        "Failure Counters:\n"
                        "MAC Rx Decrypt Fail:          %" PRIu32 "\n"
                        "MAC Rx No Frame Fail:         %" PRIu32 "\n"
                        "MAC Rx Unknown Neighbor Fail: %" PRIu32 "\n"
                        "MAC Rx Invalid Src Addr Fail: %" PRIu32 "\n"
                        "MAC Rx FCS Fail:              %" PRIu32 "\n"
                        "MAC Rx Other Fail:            %" PRIu32 "\n",
                        macCounters->mRxErrSec, macCounters->mRxErrNoFrame, macCounters->mRxErrUnknownNeighbor,
                        macCounters->mRxErrInvalidSrcAddr, macCounters->mRxErrFcs, macCounters->mRxErrOther);

        ipCounters = OpenThreadDL_Manager::getInstance().mOtThreadGetIp6Counters(mOTInst);

        ChipLogProgress(DeviceLayer,
                        "IP Counters:\n"
                        "IP Tx Success:                %" PRIu32 "\n"
                        "IP Rx Success:                %" PRIu32 "\n"
                        "IP Tx Fail:                   %" PRIu32 "\n"
                        "IP Rx Fail:                   %" PRIu32 "\n",
                        ipCounters->mTxSuccess, ipCounters->mRxSuccess, ipCounters->mTxFailure, ipCounters->mRxFailure);
    }
#endif // CHIP_PROGRESS_LOGGING
exit:
    Impl()->UnlockThreadStack();
    return err;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_GetAndLogThreadTopologyMinimal(void)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

#if CHIP_PROGRESS_LOGGING
    otError otErr;
    const otExtAddress * extAddress;
    uint16_t rloc16;
    uint16_t routerId;
    uint16_t leaderRouterId;
    uint32_t partitionId;
    int8_t parentAverageRssi;
    int8_t parentLastRssi;
    int8_t instantRssi;

    Impl()->LockThreadStack();

    rloc16 = OpenThreadDL_Manager::getInstance().mOtThreadGetRloc16(mOTInst);

    // Router ID is the top 6 bits of the RLOC
    routerId = (rloc16 >> 10) & 0x3f;

    leaderRouterId = OpenThreadDL_Manager::getInstance().mOtThreadGetLeaderRouterId(mOTInst);

    otErr = OpenThreadDL_Manager::getInstance().mOtThreadGetParentAverageRssi(mOTInst, &parentAverageRssi);
    VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

    otErr = OpenThreadDL_Manager::getInstance().mOtThreadGetParentLastRssi(mOTInst, &parentLastRssi);
    VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

    partitionId = OpenThreadDL_Manager::getInstance().mOtThreadGetPartitionId(mOTInst);

    extAddress = OpenThreadDL_Manager::getInstance().mOtLinkGetExtendedAddress(mOTInst);

    instantRssi = OpenThreadDL_Manager::getInstance().mOtPlatRadioGetRssi(mOTInst);

    ChipLogProgress(DeviceLayer,
                    "Thread Topology:\n"
                    "RLOC16:           %04X\n"
                    "Router ID:        %u\n"
                    "Leader Router ID: %u\n"
                    "Parent Avg RSSI:  %d\n"
                    "Parent Last RSSI: %d\n"
                    "Partition ID:     %" PRIu32 "\n",
                    rloc16, routerId, leaderRouterId, parentAverageRssi, parentLastRssi, partitionId);

    ChipLogProgress(DeviceLayer,
                    "Extended Address: %02X%02X:%02X%02X:%02X%02X:%02X%02X\n"
                    "Instant RSSI:     %d\n",
                    extAddress->m8[0], extAddress->m8[1], extAddress->m8[2], extAddress->m8[3], extAddress->m8[4],
                    extAddress->m8[5], extAddress->m8[6], extAddress->m8[7], instantRssi);

exit:
    Impl()->UnlockThreadStack();

    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "GetAndLogThreadTopologyMinimul failed: %" CHIP_ERROR_FORMAT, err.Format());
    }
#endif // CHIP_PROGRESS_LOGGING
    return err;
}

#define TELEM_NEIGHBOR_TABLE_SIZE (64)
#define TELEM_PRINT_BUFFER_SIZE (64)

#if CHIP_DEVICE_CONFIG_THREAD_FTD
template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_GetAndLogThreadTopologyFull()
{
    CHIP_ERROR err = CHIP_NO_ERROR;

#if CHIP_PROGRESS_LOGGING
    otError otErr;
    otIp6Address * leaderAddr    = NULL;
    uint8_t * networkData        = NULL;
    uint8_t * stableNetworkData  = NULL;
    uint8_t networkDataLen       = 0;
    uint8_t stableNetworkDataLen = 0;
    const otExtAddress * extAddress;
    otNeighborInfo neighborInfo[TELEM_NEIGHBOR_TABLE_SIZE];
    otNeighborInfoIterator iter;
    otNeighborInfoIterator iterCopy;
    char printBuf[TELEM_PRINT_BUFFER_SIZE] = { 0 };
    uint16_t rloc16;
    uint16_t routerId;
    uint16_t leaderRouterId;
    uint8_t leaderWeight;
    uint8_t leaderLocalWeight;
    uint32_t partitionId;
    int8_t instantRssi;
    uint8_t networkDataVersion;
    uint8_t stableNetworkDataVersion;
    uint16_t neighborTableSize = 0;
    uint16_t childTableSize    = 0;

    Impl()->LockThreadStack();

    rloc16 = OpenThreadDL_Manager::getInstance().mOtThreadGetRloc16(mOTInst);

    // Router ID is the top 6 bits of the RLOC
    routerId = (rloc16 >> 10) & 0x3f;

    leaderRouterId = OpenThreadDL_Manager::getInstance().mOtThreadGetLeaderRouterId(mOTInst);

    otErr = OpenThreadDL_Manager::getInstance().mOtThreadGetLeaderRloc(mOTInst, leaderAddr);
    VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

    leaderWeight = OpenThreadDL_Manager::getInstance().mOtThreadGetLeaderWeight(mOTInst);

    leaderLocalWeight = OpenThreadDL_Manager::getInstance().mOtThreadGetLocalLeaderWeight(mOTInst);

    otErr = OpenThreadDL_Manager::getInstance().mOtNetDataGet(mOTInst, false, networkData, &networkDataLen);
    VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

    networkDataVersion = OpenThreadDL_Manager::getInstance().mOtNetDataGetVersion(mOTInst);

    otErr = OpenThreadDL_Manager::getInstance().mOtNetDataGet(mOTInst, true, stableNetworkData, &stableNetworkDataLen);
    VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

    stableNetworkDataVersion = OpenThreadDL_Manager::getInstance().mOtNetDataGetStableVersion(mOTInst);

    extAddress = OpenThreadDL_Manager::getInstance().mOtLinkGetExtendedAddress(mOTInst);

    partitionId = OpenThreadDL_Manager::getInstance().mOtThreadGetPartitionId(mOTInst);

    instantRssi = OpenThreadDL_Manager::getInstance().mOtPlatRadioGetRssi(mOTInst);

    iter              = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    iterCopy          = OT_NEIGHBOR_INFO_ITERATOR_INIT;
    neighborTableSize = 0;
    childTableSize    = 0;

    while (OpenThreadDL_Manager::getInstance().mOtThreadGetNextNeighborInfo(mOTInst, &iter, &neighborInfo[iter]) == OT_ERROR_NONE)
    {
        neighborTableSize++;
        if (neighborInfo[iterCopy].mIsChild)
        {
            childTableSize++;
        }
        iterCopy = iter;
    }

    snprintf(printBuf, TELEM_PRINT_BUFFER_SIZE, "%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X:%02X%02X",
             leaderAddr->mFields.m8[0], leaderAddr->mFields.m8[1], leaderAddr->mFields.m8[2], leaderAddr->mFields.m8[3],
             leaderAddr->mFields.m8[4], leaderAddr->mFields.m8[5], leaderAddr->mFields.m8[6], leaderAddr->mFields.m8[7],
             leaderAddr->mFields.m8[8], leaderAddr->mFields.m8[9], leaderAddr->mFields.m8[10], leaderAddr->mFields.m8[11],
             leaderAddr->mFields.m8[12], leaderAddr->mFields.m8[13], leaderAddr->mFields.m8[14], leaderAddr->mFields.m8[15]);

    ChipLogProgress(DeviceLayer,
                    "Thread Topology:\n"
                    "RLOC16:                        %04X\n"
                    "Router ID:                     %u\n"
                    "Leader Router ID:              %u\n"
                    "Leader Address:                %s\n"
                    "Leader Weight:                 %d\n"
                    "Local Leader Weight:           %d\n"
                    "Network Data Len:              %d\n"
                    "Network Data Version:          %d\n"
                    "Stable Network Data Version:   %d\n",
                    rloc16, routerId, leaderRouterId, printBuf, leaderWeight, leaderLocalWeight, networkDataLen, networkDataVersion,
                    stableNetworkDataVersion);

    memset(printBuf, 0x00, TELEM_PRINT_BUFFER_SIZE);

    ChipLogProgress(DeviceLayer,
                    "Extended Address:              %02X%02X:%02X%02X:%02X%02X:%02X%02X\n"
                    "Partition ID:                  %" PRIx32 "\n"
                    "Instant RSSI:                  %d\n"
                    "Neighbor Table Length:         %d\n"
                    "Child Table Length:            %d\n",
                    extAddress->m8[0], extAddress->m8[1], extAddress->m8[2], extAddress->m8[3], extAddress->m8[4],
                    extAddress->m8[5], extAddress->m8[6], extAddress->m8[7], partitionId, instantRssi, neighborTableSize,
                    childTableSize);

    // Handle each neighbor event seperatly.
    for (uint32_t i = 0; i < neighborTableSize; i++)
    {
        otNeighborInfo * neighbor = &neighborInfo[i];

        if (neighbor->mIsChild)
        {
            otChildInfo * child = NULL;
            otErr               = OpenThreadDL_Manager::getInstance().mOtThreadGetChildInfoById(mOTInst, neighbor->mRloc16, child);
            VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

            snprintf(printBuf, TELEM_PRINT_BUFFER_SIZE, ", Timeout: %10" PRIu32 " NetworkDataVersion: %3u", child->mTimeout,
                     child->mNetworkDataVersion);
        }
        else
        {
            printBuf[0] = 0;
        }

        ChipLogProgress(DeviceLayer,
                        "TopoEntry[%" PRIu32 "]:     %02X%02X:%02X%02X:%02X%02X:%02X%02X\n"
                        "RLOC:              %04X\n"
                        "Age:               %3" PRIu32 "\n"
                        "LQI:               %1d\n"
                        "AvgRSSI:           %3d\n"
                        "LastRSSI:          %3d\n",
                        i, neighbor->mExtAddress.m8[0], neighbor->mExtAddress.m8[1], neighbor->mExtAddress.m8[2],
                        neighbor->mExtAddress.m8[3], neighbor->mExtAddress.m8[4], neighbor->mExtAddress.m8[5],
                        neighbor->mExtAddress.m8[6], neighbor->mExtAddress.m8[7], neighbor->mRloc16, neighbor->mAge,
                        neighbor->mLinkQualityIn, neighbor->mAverageRssi, neighbor->mLastRssi);

        ChipLogProgress(DeviceLayer,
                        "LinkFrameCounter:  %10" PRIu32 "\n"
                        "MleFrameCounter:   %10" PRIu32 "\n"
                        "RxOnWhenIdle:      %c\n"
                        "FullFunction:      %c\n"
                        "FullNetworkData:   %c\n"
                        "IsChild:           %c%s\n",
                        neighbor->mLinkFrameCounter, neighbor->mMleFrameCounter, neighbor->mRxOnWhenIdle ? 'Y' : 'n',
                        neighbor->mFullThreadDevice ? 'Y' : 'n', neighbor->mFullNetworkData ? 'Y' : 'n',
                        neighbor->mIsChild ? 'Y' : 'n', printBuf);
    }

exit:
    Impl()->UnlockThreadStack();

    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "GetAndLogThreadTopologyFull failed: %s", ErrorStr(err));
    }
#endif // CHIP_PROGRESS_LOGGING
    return err;
}
#else // CHIP_DEVICE_CONFIG_THREAD_FTD
template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_GetAndLogThreadTopologyFull()
{
    return CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
}
#endif

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_GetPrimary802154MACAddress(uint8_t * buf)
{
    VerifyOrReturnError(mOpenthreadInitialized, CHIP_ERROR_WELL_UNINITIALIZED);

    const otExtAddress * extendedAddr = OpenThreadDL_Manager::getInstance().mOtLinkGetExtendedAddress(mOTInst);
    memcpy(buf, extendedAddr, sizeof(otExtAddress));
    return CHIP_NO_ERROR;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_GetExternalIPv6Address(chip::Inet::IPAddress & addr)
{
    VerifyOrReturnError(mOpenthreadInitialized, CHIP_ERROR_WELL_UNINITIALIZED);

    const otNetifAddress * otAddresses = OpenThreadDL_Manager::getInstance().mOtIp6GetUnicastAddresses(mOTInst);

    // Look only for the global unicast addresses, not internally assigned by Thread.
    for (const otNetifAddress * otAddress = otAddresses; otAddress != nullptr; otAddress = otAddress->mNext)
    {
        if (otAddress->mValid)
        {
            switch (otAddress->mAddressOrigin)
            {
            case OT_ADDRESS_ORIGIN_THREAD:
                break;
            case OT_ADDRESS_ORIGIN_SLAAC:
            case OT_ADDRESS_ORIGIN_DHCPV6:
            case OT_ADDRESS_ORIGIN_MANUAL:
                addr = ToIPAddress(otAddress->mAddress);
                return CHIP_NO_ERROR;
            default:
                break;
            }
        }
    }

    return CHIP_DEVICE_ERROR_CONFIG_NOT_FOUND;
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::_ResetThreadNetworkDiagnosticsCounts(void)
{
    // Based on the spec, only OverrunCount should be resetted.
    mOverrunCount = 0;
}
/*
 * @brief Get runtime value from the thread network based on the given attribute ID.
 *        The info is encoded via the AttributeValueEncoder.
 *
 * @param attributeId Id of the attribute for the requested info.
 * @param aEncoder Encoder to encode the attribute value.
 *
 * @return CHIP_NO_ERROR = Succes.
 *         CHIP_ERROR_NOT_IMPLEMENTED = Runtime value for this attribute to yet available to send as reply
 *                                      Use standard read.
 *         CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE = Is not a Runtime readable attribute. Use standard read
 *         All other errors should be treated as a read error and reported as such.
 */
template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_WriteThreadNetworkDiagnosticAttributeToTlv(
    AttributeId attributeId, app::AttributeValueEncoder & encoder)
{
    CHIP_ERROR err;

    namespace ThreadNetworkDiagnostics = app::Clusters::ThreadNetworkDiagnostics;

    switch (attributeId)
    {
    case ThreadNetworkDiagnostics::Attributes::Channel::Id: {
        uint16_t channel = OpenThreadDL_Manager::getInstance().mOtLinkGetChannel(mOTInst);
        err              = encoder.Encode(channel);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RoutingRole::Id: {
        ThreadNetworkDiagnostics::RoutingRole routingRole;
        otDeviceRole otRole = OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst);

        if (otRole == OT_DEVICE_ROLE_DISABLED)
        {
            routingRole = EMBER_ZCL_ROUTING_ROLE_UNSPECIFIED;
        }
        else if (otRole == OT_DEVICE_ROLE_DETACHED)
        {
            routingRole = EMBER_ZCL_ROUTING_ROLE_UNASSIGNED;
        }
        else if (otRole == OT_DEVICE_ROLE_ROUTER)
        {
            routingRole = EMBER_ZCL_ROUTING_ROLE_ROUTER;
        }
        else if (otRole == OT_DEVICE_ROLE_LEADER)
        {
            routingRole = EMBER_ZCL_ROUTING_ROLE_LEADER;
        }
        else if (otRole == OT_DEVICE_ROLE_CHILD)
        {
            otLinkModeConfig linkMode = OpenThreadDL_Manager::getInstance().mOtThreadGetLinkMode(mOTInst);

            if (linkMode.mRxOnWhenIdle)
            {
                routingRole = EMBER_ZCL_ROUTING_ROLE_END_DEVICE;
#if CHIP_DEVICE_CONFIG_THREAD_FTD
                if (OpenThreadDL_Manager::getInstance().mOtThreadIsRouterEligible(mOTInst))
                {
                    routingRole = EMBER_ZCL_ROUTING_ROLE_REED;
                }
#endif
            }
            else
            {
                routingRole = EMBER_ZCL_ROUTING_ROLE_SLEEPY_END_DEVICE;
            }
        }

        err = encoder.Encode(routingRole);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::NetworkName::Id: {
        const char * networkName = OpenThreadDL_Manager::getInstance().mOtThreadGetNetworkName(mOTInst);
        err                      = encoder.Encode(CharSpan::fromCharString(networkName));
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::PanId::Id: {
        uint16_t panId = OpenThreadDL_Manager::getInstance().mOtLinkGetPanId(mOTInst);
        err            = encoder.Encode(panId);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::ExtendedPanId::Id: {
        const otExtendedPanId * pExtendedPanid = OpenThreadDL_Manager::getInstance().mOtThreadGetExtendedPanId(mOTInst);
        err                                    = encoder.Encode(Encoding::BigEndian::Get64(pExtendedPanid->m8));
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::MeshLocalPrefix::Id: {
        uint8_t meshLocaPrefix[OT_MESH_LOCAL_PREFIX_SIZE + 1] = { 0 }; // + 1  to encode prefix Len in the octstr

        const otMeshLocalPrefix * pMeshLocalPrefix = OpenThreadDL_Manager::getInstance().mOtThreadGetMeshLocalPrefix(mOTInst);
        meshLocaPrefix[0]                          = OT_IP6_PREFIX_BITSIZE;

        memcpy(&meshLocaPrefix[1], pMeshLocalPrefix->m8, OT_MESH_LOCAL_PREFIX_SIZE);
        err = encoder.Encode(ByteSpan(meshLocaPrefix));
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::OverrunCount::Id: {
        uint64_t overrunCount = mOverrunCount;
        err                   = encoder.Encode(overrunCount);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::NeighborTable::Id: {
        err = encoder.EncodeList([this](const auto & aEncoder) -> CHIP_ERROR {
            constexpr uint16_t kFrameErrorRate100Percent   = 0xffff;
            constexpr uint16_t kMessageErrorRate100Percent = 0xffff;

            otNeighborInfo neighInfo;
            otNeighborInfoIterator iterator = OT_NEIGHBOR_INFO_ITERATOR_INIT;

            while (OpenThreadDL_Manager::getInstance().mOtThreadGetNextNeighborInfo(mOTInst, &iterator, &neighInfo) ==
                   OT_ERROR_NONE)
            {
                ThreadNetworkDiagnostics::Structs::NeighborTable::Type neighborTable;
                app::DataModel::Nullable<int8_t> averageRssi;
                app::DataModel::Nullable<int8_t> lastRssi;

                if (neighInfo.mAverageRssi == OT_RADIO_RSSI_INVALID)
                {
                    averageRssi.SetNull();
                }
                else
                {
                    // Thread average calculation already restrict mAverageRssi to be between -128 and 0
                    averageRssi.SetNonNull(neighInfo.mAverageRssi);
                }

                if (neighInfo.mLastRssi == OT_RADIO_RSSI_INVALID)
                {
                    lastRssi.SetNull();
                }
                else
                {
                    lastRssi.SetNonNull(min(static_cast<int8_t>(0), neighInfo.mLastRssi));
                }

                neighborTable.averageRssi      = averageRssi;
                neighborTable.lastRssi         = lastRssi;
                neighborTable.extAddress       = Encoding::BigEndian::Get64(neighInfo.mExtAddress.m8);
                neighborTable.age              = neighInfo.mAge;
                neighborTable.rloc16           = neighInfo.mRloc16;
                neighborTable.linkFrameCounter = neighInfo.mLinkFrameCounter;
                neighborTable.mleFrameCounter  = neighInfo.mMleFrameCounter;
                neighborTable.lqi              = neighInfo.mLinkQualityIn;
                neighborTable.frameErrorRate =
                    static_cast<uint8_t>((static_cast<uint32_t>(neighInfo.mFrameErrorRate) * 100) / kFrameErrorRate100Percent);
                neighborTable.messageErrorRate =
                    static_cast<uint8_t>((static_cast<uint32_t>(neighInfo.mMessageErrorRate) * 100) / kMessageErrorRate100Percent);
                neighborTable.rxOnWhenIdle     = neighInfo.mRxOnWhenIdle;
                neighborTable.fullThreadDevice = neighInfo.mFullThreadDevice;
                neighborTable.fullNetworkData  = neighInfo.mFullNetworkData;
                neighborTable.isChild          = neighInfo.mIsChild;

                ReturnErrorOnFailure(aEncoder.Encode(neighborTable));
            }

            return CHIP_NO_ERROR;
        });
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RouteTable::Id: {
        err = encoder.EncodeList([this](const auto & aEncoder) -> CHIP_ERROR {
            otRouterInfo routerInfo;

#if CHIP_DEVICE_CONFIG_THREAD_FTD
            uint8_t maxRouterId = OpenThreadDL_Manager::getInstance().mOtThreadGetMaxRouterId(mOTInst);
            CHIP_ERROR chipErr  = CHIP_ERROR_INCORRECT_STATE;

            for (uint8_t i = 0; i <= maxRouterId; i++)
            {
                if (OpenThreadDL_Manager::getInstance().mOtThreadGetRouterInfo(mOTInst, i, &routerInfo) == OT_ERROR_NONE)
                {
                    ThreadNetworkDiagnostics::Structs::RouteTable::Type routeTable;

                    routeTable.extAddress      = Encoding::BigEndian::Get64(routerInfo.mExtAddress.m8);
                    routeTable.rloc16          = routerInfo.mRloc16;
                    routeTable.routerId        = routerInfo.mRouterId;
                    routeTable.nextHop         = routerInfo.mNextHop;
                    routeTable.pathCost        = routerInfo.mPathCost;
                    routeTable.LQIIn           = routerInfo.mLinkQualityIn;
                    routeTable.LQIOut          = routerInfo.mLinkQualityOut;
                    routeTable.age             = routerInfo.mAge;
                    routeTable.allocated       = routerInfo.mAllocated;
                    routeTable.linkEstablished = routerInfo.mLinkEstablished;

                    ReturnErrorOnFailure(aEncoder.Encode(routeTable));
                    chipErr = CHIP_NO_ERROR;
                }
            }

            return chipErr;

#else // OPENTHREAD_MTD
            otError otErr = OpenThreadDL_Manager::getInstance().mOtThreadGetParentInfo(mOTInst, &routerInfo);
            ReturnErrorOnFailure(MapOpenThreadError(otErr));

            ThreadNetworkDiagnostics::Structs::RouteTable::Type routeTable;

            routeTable.extAddress      = Encoding::BigEndian::Get64(routerInfo.mExtAddress.m8);
            routeTable.rloc16          = routerInfo.mRloc16;
            routeTable.routerId        = routerInfo.mRouterId;
            routeTable.nextHop         = routerInfo.mNextHop;
            routeTable.pathCost        = routerInfo.mPathCost;
            routeTable.LQIIn           = routerInfo.mLinkQualityIn;
            routeTable.LQIOut          = routerInfo.mLinkQualityOut;
            routeTable.age             = routerInfo.mAge;
            routeTable.allocated       = routerInfo.mAllocated;
            routeTable.linkEstablished = routerInfo.mLinkEstablished;

            ReturnErrorOnFailure(aEncoder.Encode(routeTable));
            return CHIP_NO_ERROR;
#endif
        });
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::PartitionId::Id: {
        uint32_t partitionId = OpenThreadDL_Manager::getInstance().mOtThreadGetPartitionId(mOTInst);
        err                  = encoder.Encode(partitionId);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::Weighting::Id: {
        uint8_t weight = OpenThreadDL_Manager::getInstance().mOtThreadGetLeaderWeight(mOTInst);
        err            = encoder.Encode(weight);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::DataVersion::Id: {
        uint8_t dataVersion = OpenThreadDL_Manager::getInstance().mOtNetDataGetVersion(mOTInst);
        err                 = encoder.Encode(dataVersion);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::StableDataVersion::Id: {
        uint8_t stableVersion = OpenThreadDL_Manager::getInstance().mOtNetDataGetStableVersion(mOTInst);
        err                   = encoder.Encode(stableVersion);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::LeaderRouterId::Id: {
        uint8_t leaderRouterId = OpenThreadDL_Manager::getInstance().mOtThreadGetLeaderRouterId(mOTInst);
        err                    = encoder.Encode(leaderRouterId);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::DetachedRoleCount::Id: {
        uint16_t detachedRole = OpenThreadDL_Manager::getInstance().mOtThreadGetMleCounters(mOTInst)->mDetachedRole;
        err                   = encoder.Encode(detachedRole);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::ChildRoleCount::Id: {
        uint16_t childRole = OpenThreadDL_Manager::getInstance().mOtThreadGetMleCounters(mOTInst)->mChildRole;
        err                = encoder.Encode(childRole);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RouterRoleCount::Id: {
        uint16_t routerRole = OpenThreadDL_Manager::getInstance().mOtThreadGetMleCounters(mOTInst)->mRouterRole;
        err                 = encoder.Encode(routerRole);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::LeaderRoleCount::Id: {
        uint16_t leaderRole = OpenThreadDL_Manager::getInstance().mOtThreadGetMleCounters(mOTInst)->mLeaderRole;
        err                 = encoder.Encode(leaderRole);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::AttachAttemptCount::Id: {
        uint16_t attachAttempts = OpenThreadDL_Manager::getInstance().mOtThreadGetMleCounters(mOTInst)->mAttachAttempts;
        err                     = encoder.Encode(attachAttempts);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::PartitionIdChangeCount::Id: {
        uint16_t partitionIdChanges = OpenThreadDL_Manager::getInstance().mOtThreadGetMleCounters(mOTInst)->mPartitionIdChanges;
        err                         = encoder.Encode(partitionIdChanges);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::BetterPartitionAttachAttemptCount::Id: {
        uint16_t betterPartitionAttachAttempts =
            OpenThreadDL_Manager::getInstance().mOtThreadGetMleCounters(mOTInst)->mBetterPartitionAttachAttempts;
        err = encoder.Encode(betterPartitionAttachAttempts);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::ParentChangeCount::Id: {
        uint16_t parentChanges = OpenThreadDL_Manager::getInstance().mOtThreadGetMleCounters(mOTInst)->mParentChanges;
        err                    = encoder.Encode(parentChanges);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxTotalCount::Id: {
        uint32_t txTotal = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxTotal;
        err              = encoder.Encode(txTotal);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxUnicastCount::Id: {
        uint32_t txUnicast = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxUnicast;
        err                = encoder.Encode(txUnicast);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxBroadcastCount::Id: {
        uint32_t txBroadcast = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxBroadcast;
        err                  = encoder.Encode(txBroadcast);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxAckRequestedCount::Id: {
        uint32_t txAckRequested = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxAckRequested;
        err                     = encoder.Encode(txAckRequested);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxAckedCount::Id: {
        uint32_t txAcked = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxAcked;
        err              = encoder.Encode(txAcked);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxNoAckRequestedCount::Id: {
        uint32_t txNoAckRequested = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxNoAckRequested;
        err                       = encoder.Encode(txNoAckRequested);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxDataCount::Id: {
        uint32_t txData = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxData;
        err             = encoder.Encode(txData);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxDataPollCount::Id: {
        uint32_t txDataPoll = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxDataPoll;
        err                 = encoder.Encode(txDataPoll);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxBeaconCount::Id: {
        uint32_t txBeacon = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxBeacon;
        err               = encoder.Encode(txBeacon);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxBeaconRequestCount::Id: {
        uint32_t txBeaconRequest = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxBeaconRequest;
        err                      = encoder.Encode(txBeaconRequest);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxOtherCount::Id: {
        uint32_t txOther = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxOther;
        err              = encoder.Encode(txOther);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxRetryCount::Id: {
        uint32_t txRetry = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxRetry;
        err              = encoder.Encode(txRetry);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxDirectMaxRetryExpiryCount::Id: {
        uint32_t txDirectMaxRetryExpiry = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxDirectMaxRetryExpiry;
        err                             = encoder.Encode(txDirectMaxRetryExpiry);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxIndirectMaxRetryExpiryCount::Id: {
        uint32_t txIndirectMaxRetryExpiry =
            OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxIndirectMaxRetryExpiry;
        err = encoder.Encode(txIndirectMaxRetryExpiry);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxErrCcaCount::Id: {
        uint32_t txErrCca = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxErrCca;
        err               = encoder.Encode(txErrCca);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxErrAbortCount::Id: {
        uint32_t TxErrAbort = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxErrAbort;
        err                 = encoder.Encode(TxErrAbort);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::TxErrBusyChannelCount::Id: {
        uint32_t TxErrBusyChannel = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mTxErrBusyChannel;
        err                       = encoder.Encode(TxErrBusyChannel);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxTotalCount::Id: {
        uint32_t rxTotal = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxTotal;
        err              = encoder.Encode(rxTotal);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxUnicastCount::Id: {
        uint32_t rxUnicast = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxUnicast;
        err                = encoder.Encode(rxUnicast);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxBroadcastCount::Id: {
        uint32_t rxBroadcast = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxBroadcast;
        err                  = encoder.Encode(rxBroadcast);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxDataCount::Id: {
        uint32_t rxData = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxData;
        err             = encoder.Encode(rxData);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxDataPollCount::Id: {
        uint32_t rxDataPoll = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxDataPoll;
        err                 = encoder.Encode(rxDataPoll);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxBeaconCount::Id: {
        uint32_t rxBeacon = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxBeacon;
        err               = encoder.Encode(rxBeacon);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxBeaconRequestCount::Id: {
        uint32_t rxBeaconRequest = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxBeaconRequest;
        err                      = encoder.Encode(rxBeaconRequest);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxOtherCount::Id: {
        uint32_t rxOther = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxOther;
        err              = encoder.Encode(rxOther);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxAddressFilteredCount::Id: {
        uint32_t rxAddressFiltered = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxAddressFiltered;
        err                        = encoder.Encode(rxAddressFiltered);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxDestAddrFilteredCount::Id: {
        uint32_t rxDestAddrFiltered = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxDestAddrFiltered;
        err                         = encoder.Encode(rxDestAddrFiltered);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxDuplicatedCount::Id: {
        uint32_t rxDuplicated = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxDuplicated;
        err                   = encoder.Encode(rxDuplicated);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxErrNoFrameCount::Id: {
        uint32_t rxErrNoFrame = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxErrNoFrame;
        err                   = encoder.Encode(rxErrNoFrame);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxErrUnknownNeighborCount::Id: {
        uint32_t rxErrUnknownNeighbor = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxErrUnknownNeighbor;
        err                           = encoder.Encode(rxErrUnknownNeighbor);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxErrInvalidSrcAddrCount::Id: {
        uint32_t rxErrInvalidSrcAddr = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxErrInvalidSrcAddr;
        err                          = encoder.Encode(rxErrInvalidSrcAddr);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxErrSecCount::Id: {
        uint32_t rxErrSec = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxErrSec;
        err               = encoder.Encode(rxErrSec);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxErrFcsCount::Id: {
        uint32_t rxErrFcs = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxErrFcs;
        err               = encoder.Encode(rxErrFcs);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::RxErrOtherCount::Id: {
        uint32_t rxErrOther = OpenThreadDL_Manager::getInstance().mOtLinkGetCounters(mOTInst)->mRxErrOther;
        err                 = encoder.Encode(rxErrOther);
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::ActiveTimestamp::Id: {
        err = CHIP_ERROR_INCORRECT_STATE;
        if (OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(mOTInst))
        {
            otOperationalDataset activeDataset;
            otError otErr = OpenThreadDL_Manager::getInstance().mOtDatasetGetActive(mOTInst, &activeDataset);
            VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));
#if OPENTHREAD_API_VERSION >= 219
            uint64_t activeTimestamp = (activeDataset.mActiveTimestamp.mSeconds << 16) |
                (activeDataset.mActiveTimestamp.mTicks << 1) | activeDataset.mActiveTimestamp.mAuthoritative;
#else
            uint64_t activeTimestamp  = activeDataset.mActiveTimestamp;
#endif
            err = encoder.Encode(activeTimestamp);
        }
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::PendingTimestamp::Id: {
        err = CHIP_ERROR_INCORRECT_STATE;
        if (OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(mOTInst))
        {
            otOperationalDataset activeDataset;
            otError otErr = OpenThreadDL_Manager::getInstance().mOtDatasetGetActive(mOTInst, &activeDataset);
            VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));
#if OPENTHREAD_API_VERSION >= 219
            uint64_t pendingTimestamp = (activeDataset.mPendingTimestamp.mSeconds << 16) |
                (activeDataset.mPendingTimestamp.mTicks << 1) | activeDataset.mPendingTimestamp.mAuthoritative;
#else
            uint64_t pendingTimestamp = activeDataset.mPendingTimestamp;
#endif
            err = encoder.Encode(pendingTimestamp);
        }
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::Delay::Id: {
        err = CHIP_ERROR_INCORRECT_STATE;
        if (OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(mOTInst))
        {
            otOperationalDataset activeDataset;
            otError otErr = OpenThreadDL_Manager::getInstance().mOtDatasetGetActive(mOTInst, &activeDataset);
            VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));
            uint32_t delay = activeDataset.mDelay;
            err            = encoder.Encode(delay);
        }
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::SecurityPolicy::Id: {
        err = CHIP_ERROR_INCORRECT_STATE;

        if (OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(mOTInst))
        {
            otOperationalDataset activeDataset;
            otError otErr = OpenThreadDL_Manager::getInstance().mOtDatasetGetActive(mOTInst, &activeDataset);
            VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

            ThreadNetworkDiagnostics::Structs::SecurityPolicy::Type securityPolicy;
            static_assert(sizeof(securityPolicy) == sizeof(activeDataset.mSecurityPolicy),
                          "securityPolicy Struct do not match otSecurityPolicy");
            uint16_t policyAsInts[2];
            static_assert(sizeof(policyAsInts) == sizeof(activeDataset.mSecurityPolicy),
                          "We're missing some members of otSecurityPolicy?");
            memcpy(&policyAsInts, &activeDataset.mSecurityPolicy, sizeof(policyAsInts));
            securityPolicy.rotationTime = policyAsInts[0];
            securityPolicy.flags        = policyAsInts[1];

            err = encoder.Encode(securityPolicy);
        }
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::ChannelPage0Mask::Id: {
        err = CHIP_ERROR_INCORRECT_STATE;
        if (OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(mOTInst))
        {
            otOperationalDataset activeDataset;
            otError otErr = OpenThreadDL_Manager::getInstance().mOtDatasetGetActive(mOTInst, &activeDataset);
            VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

            // In the resultant Octet string, the most significant bit of the left-most byte indicates channel 0
            // We have to bitswap the entire uint32_t before converting to octet string
            uint32_t bitSwappedChannelMask = 0;
            for (int i = 0, j = 31; i < 32; i++, j--)
            {
                bitSwappedChannelMask |= ((activeDataset.mChannelMask >> j) & 1) << i;
            }

            uint8_t buffer[sizeof(uint32_t)] = { 0 };
            Encoding::BigEndian::Put32(buffer, bitSwappedChannelMask);
            err = encoder.Encode(ByteSpan(buffer));
        }
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::OperationalDatasetComponents::Id: {
        err = CHIP_ERROR_INCORRECT_STATE;
        if (OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(mOTInst))
        {
            otOperationalDataset activeDataset;
            otError otErr = OpenThreadDL_Manager::getInstance().mOtDatasetGetActive(mOTInst, &activeDataset);
            VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));
            ThreadNetworkDiagnostics::Structs::OperationalDatasetComponents::Type OpDatasetComponents;

            OpDatasetComponents.activeTimestampPresent  = activeDataset.mComponents.mIsActiveTimestampPresent;
            OpDatasetComponents.pendingTimestampPresent = activeDataset.mComponents.mIsPendingTimestampPresent;
            OpDatasetComponents.masterKeyPresent        = activeDataset.mComponents.mIsNetworkKeyPresent;
            OpDatasetComponents.networkNamePresent      = activeDataset.mComponents.mIsNetworkNamePresent;
            OpDatasetComponents.extendedPanIdPresent    = activeDataset.mComponents.mIsExtendedPanIdPresent;
            OpDatasetComponents.meshLocalPrefixPresent  = activeDataset.mComponents.mIsMeshLocalPrefixPresent;
            OpDatasetComponents.delayPresent            = activeDataset.mComponents.mIsDelayPresent;
            OpDatasetComponents.panIdPresent            = activeDataset.mComponents.mIsPanIdPresent;
            OpDatasetComponents.channelPresent          = activeDataset.mComponents.mIsChannelPresent;
            OpDatasetComponents.pskcPresent             = activeDataset.mComponents.mIsPskcPresent;
            OpDatasetComponents.securityPolicyPresent   = activeDataset.mComponents.mIsSecurityPolicyPresent;
            OpDatasetComponents.channelMaskPresent      = activeDataset.mComponents.mIsChannelMaskPresent;

            err = encoder.Encode(OpDatasetComponents);
        }
    }
    break;

    case ThreadNetworkDiagnostics::Attributes::ActiveNetworkFaultsList::Id: {
        err = encoder.EncodeList([](const auto & aEncoder) -> CHIP_ERROR {
            // TODO activeNetworkFaultsList isn't tracked. Encode the list of 4 entries at 0 none the less
            ThreadNetworkDiagnostics::NetworkFault activeNetworkFaultsList[4] = { ThreadNetworkDiagnostics::NetworkFault(0) };
            for (auto fault : activeNetworkFaultsList)
            {
                ReturnErrorOnFailure(aEncoder.Encode(fault));
            }

            return CHIP_NO_ERROR;
        });
    }
    break;

    default: {
        err = CHIP_ERROR_UNSUPPORTED_CHIP_FEATURE;
    }
    break;
    }

exit:
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "_WriteThreadNetworkDiagnosticAttributeToTlv failed: %s", ErrorStr(err));
    }
    return err;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_GetPollPeriod(uint32_t & buf)
{
    Impl()->LockThreadStack();
    buf = OpenThreadDL_Manager::getInstance().mOtLinkGetPollPeriod(mOTInst);
    Impl()->UnlockThreadStack();
    return CHIP_NO_ERROR;
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::_SetRouterPromotion(bool val)
{
#if CHIP_DEVICE_CONFIG_THREAD_FTD
    Impl()->LockThreadStack();
    if (OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(DeviceLayer::ThreadStackMgrImpl().OTInstance()) !=
        OT_DEVICE_ROLE_ROUTER)
    {
        (void) !OpenThreadDL_Manager::getInstance().mOtThreadSetRouterEligible(DeviceLayer::ThreadStackMgrImpl().OTInstance(), val);
    }
    Impl()->UnlockThreadStack();
#endif
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::DoInit(otInstance * otInst)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::DoInit");

    CHIP_ERROR err = CHIP_NO_ERROR;
    otError otErr  = OT_ERROR_NONE;

    // Arrange for OpenThread errors to be translated to text.
    RegisterOpenThreadErrorFormatter();

    mOTInst = NULL;

    // If an OpenThread instance hasn't been supplied, call otInstanceInitSingle() to
    // create or acquire a singleton instance of OpenThread.
    if (otInst == NULL)
    {
        otInst = OpenThreadDL_Manager::getInstance().mOtInstanceInitSingle();
        VerifyOrExit(otInst != NULL, err = MapOpenThreadError(OT_ERROR_FAILED));
    }

#if !defined(PW_RPC_ENABLED) && CHIP_DEVICE_CONFIG_THREAD_ENABLE_CLI
    otAppCliInit(otInst);
#endif

    mOTInst                = otInst;
    mOpenthreadInitialized = true;

#if CHIP_DEVICE_CONFIG_ENABLE_SED
    ConnectivityManager::SEDIntervalsConfig sedIntervalsConfig;
    using namespace System::Clock::Literals;
    sedIntervalsConfig.ActiveIntervalMS = CHIP_DEVICE_CONFIG_SED_ACTIVE_INTERVAL;
    sedIntervalsConfig.IdleIntervalMS   = CHIP_DEVICE_CONFIG_SED_IDLE_INTERVAL;
    err                                 = _SetSEDIntervalsConfig(sedIntervalsConfig);
    if (err != CHIP_NO_ERROR)
    {
        ChipLogError(DeviceLayer, "Failed to set sleepy end device intervals: %s", ErrorStr(err));
    }
    SuccessOrExit(err);
#endif

    // Arrange for OpenThread to call the OnOpenThreadStateChange method whenever a
    // state change occurs.  Note that we reference the OnOpenThreadStateChange method
    // on the concrete implementation class so that that class can override the default
    // method implementation if it chooses to.
    otErr = OpenThreadDL_Manager::getInstance().mOtSetStateChangedCallback(otInst, ImplClass::OnOpenThreadStateChange, mOTInst);
    VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

    // Enable automatic assignment of Thread advertised addresses.
#if OPENTHREAD_CONFIG_IP6_SLAAC_ENABLE
    OpenThreadDL_Manager::getInstance().mOtIp6SetSlaacEnabled(otInst, true);
#endif

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD_SRP_CLIENT
    OpenThreadDL_Manager::getInstance().mOtSrpClientSetCallback(mOTInst, &OnSrpClientNotification, nullptr);
    OpenThreadDL_Manager::getInstance().mOtSrpClientEnableAutoStartMode(mOTInst, &OnSrpClientStateChange, nullptr);
    memset(&mSrpClient, 0, sizeof(mSrpClient));
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD_SRP_CLIENT

    // If the Thread stack has been provisioned, but is not currently enabled, enable it now.
    if (OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst) == OT_DEVICE_ROLE_DISABLED &&
        OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(otInst))
    {
        // Enable the Thread IPv6 interface.
        otErr = OpenThreadDL_Manager::getInstance().mOtIp6SetEnabled(otInst, true);
        VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

        otErr = OpenThreadDL_Manager::getInstance().mOtThreadSetEnabled(otInst, true);
        VerifyOrExit(otErr == OT_ERROR_NONE, err = MapOpenThreadError(otErr));

        ChipLogProgress(DeviceLayer, "OpenThread ifconfig up and thread start");
    }

    initNetworkCommissioningThreadDriver();

exit:

    ChipLogProgress(DeviceLayer, "OpenThread started: %s", OpenThreadDL_Manager::getInstance().mOtThreadErrorToString(otErr));
    return err;
}

template <class ImplClass>
bool GenericThreadStackManagerImpl_OpenThread<ImplClass>::IsThreadAttachedNoLock(void)
{

    otDeviceRole curRole = OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst);
    return (curRole != OT_DEVICE_ROLE_DISABLED && curRole != OT_DEVICE_ROLE_DETACHED);
}

template <class ImplClass>
bool GenericThreadStackManagerImpl_OpenThread<ImplClass>::IsThreadInterfaceUpNoLock(void)
{

    return OpenThreadDL_Manager::getInstance().mOtIp6IsEnabled(mOTInst);
}

#if CHIP_DEVICE_CONFIG_ENABLE_SED
template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_GetSEDIntervalsConfig(
    ConnectivityManager::SEDIntervalsConfig & intervalsConfig)
{
    intervalsConfig = mIntervalsConfig;
    return CHIP_NO_ERROR;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_SetSEDIntervalsConfig(
    const ConnectivityManager::SEDIntervalsConfig & intervalsConfig)
{
    using namespace System::Clock::Literals;
    if ((intervalsConfig.IdleIntervalMS < intervalsConfig.ActiveIntervalMS) || (intervalsConfig.IdleIntervalMS == 0_ms32) ||
        (intervalsConfig.ActiveIntervalMS == 0_ms32))
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    mIntervalsConfig = intervalsConfig;

    CHIP_ERROR err = SetSEDIntervalMode(mIntervalsMode);

    if (err == CHIP_NO_ERROR)
    {
        ChipDeviceEvent event;
        event.Type = DeviceEventType::kSEDIntervalChange;
        err        = chip::DeviceLayer::PlatformMgr().PostEvent(&event);
    }

    return err;
}

template <class ImplClass>
CHIP_ERROR
GenericThreadStackManagerImpl_OpenThread<ImplClass>::SetSEDIntervalMode(ConnectivityManager::SEDIntervalMode intervalType)
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    System::Clock::Milliseconds32 interval;

    if (intervalType == ConnectivityManager::SEDIntervalMode::Idle)
    {
        interval = mIntervalsConfig.IdleIntervalMS;
    }
    else if (intervalType == ConnectivityManager::SEDIntervalMode::Active)
    {
        interval = mIntervalsConfig.ActiveIntervalMS;
    }
    else
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }

    Impl()->LockThreadStack();

    mIntervalsMode = intervalType;

// For Thread devices, the intervals are defined as:
// * poll period for SED devices that poll the parent for data
// * CSL period for SSED devices that listen for messages in scheduled time slots.
#if CHIP_DEVICE_CONFIG_THREAD_SSED
    // Get CSL period in units of 10 symbols, convert it to microseconds and divide by 1000 to get milliseconds.
    uint32_t curIntervalMS = otLinkCslGetPeriod(mOTInst) * OT_US_PER_TEN_SYMBOLS / 1000;
#else
    uint32_t curIntervalMS = otLinkGetPollPeriod(mOTInst);
#endif
    otError otErr = OT_ERROR_NONE;
    if (interval.count() != curIntervalMS)
    {
#if CHIP_DEVICE_CONFIG_THREAD_SSED
        // Set CSL period in units of 10 symbols, convert it to microseconds and divide by 1000 to get milliseconds.
        otErr         = otLinkCslSetPeriod(mOTInst, interval.count() * 1000 / OT_US_PER_TEN_SYMBOLS);
        curIntervalMS = otLinkCslGetPeriod(mOTInst) * OT_US_PER_TEN_SYMBOLS / 1000;
#else
        otErr         = otLinkSetPollPeriod(mOTInst, interval.count());
        curIntervalMS = otLinkGetPollPeriod(mOTInst);
#endif
        err = MapOpenThreadError(otErr);
    }

    Impl()->UnlockThreadStack();

    if (otErr != OT_ERROR_NONE)
    {
        ChipLogError(DeviceLayer, "Failed to set SED interval to %" PRId32 "ms. Defaulting to %" PRId32 "ms", interval.count(),
                     curIntervalMS);
    }
    else
    {
        ChipLogProgress(DeviceLayer, "OpenThread SED interval is %" PRId32 "ms", curIntervalMS);
    }

    return err;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_RequestSEDActiveMode(bool onOff, bool delayIdle)
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    if (onOff)
    {
        mActiveModeConsumers++;
    }
    else
    {
        if (mActiveModeConsumers > 0)
            mActiveModeConsumers--;
    }

    if (!onOff && delayIdle && CHIP_DEVICE_CONFIG_SED_ACTIVE_THRESHOLD.count() != 0)
    {
        // StartTimer will cancel a timer if the same callback & context is used.
        // This will have the effect of canceling the previous one (if any) and starting
        // a new timer of the same duration. This effectively prolongs the active threshold
        // without consuming additional resources.
        err = DeviceLayer::SystemLayer().StartTimer(CHIP_DEVICE_CONFIG_SED_ACTIVE_THRESHOLD, RequestSEDModeUpdate, this);
        if (CHIP_NO_ERROR == err)
        {
            if (!mDelayIdleTimerRunning)
            {
                mDelayIdleTimerRunning = true;
                mActiveModeConsumers++;
            }
            return err;
        }

        ChipLogError(DeviceLayer, "Failed to postpone Idle Mode with error %" CHIP_ERROR_FORMAT, err.Format());
    }

    return SEDUpdateMode();
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::SEDUpdateMode()
{
    CHIP_ERROR err = CHIP_NO_ERROR;
    ConnectivityManager::SEDIntervalMode mode;

    mode = mActiveModeConsumers > 0 ? ConnectivityManager::SEDIntervalMode::Active : ConnectivityManager::SEDIntervalMode::Idle;

    if (mIntervalsMode != mode)
        err = SetSEDIntervalMode(mode);

    return err;
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::RequestSEDModeUpdate(chip::System::Layer * apSystemLayer,
                                                                               void * apAppState)
{
    if (apAppState != nullptr)
    {
        GenericThreadStackManagerImpl_OpenThread * obj = static_cast<GenericThreadStackManagerImpl_OpenThread *>(apAppState);
        if (obj->mActiveModeConsumers > 0)
        {
            obj->mActiveModeConsumers--;
        }

        obj->mDelayIdleTimerRunning = false;

        obj->SEDUpdateMode();
    }
}
#endif

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::_ErasePersistentInfo(void)
{
    ChipLogProgress(DeviceLayer, "Erasing Thread persistent info...");

    VerifyOrReturn(mOpenthreadInitialized);

    Impl()->LockThreadStack();
    (void) !OpenThreadDL_Manager::getInstance().mOtThreadSetEnabled(mOTInst, false);
    (void) !OpenThreadDL_Manager::getInstance().mOtInstanceErasePersistentInfo(mOTInst);
    Impl()->UnlockThreadStack();
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::OnJoinerComplete(otError aError, void * aContext)
{
    static_cast<GenericThreadStackManagerImpl_OpenThread *>(aContext)->OnJoinerComplete(aError);
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::OnJoinerComplete(otError aError)
{
#if CHIP_PROGRESS_LOGGING

    ChipLogProgress(DeviceLayer, "Join Thread network: %s", OpenThreadDL_Manager::getInstance().mOtThreadErrorToString(aError));

    if (aError == OT_ERROR_NONE)
    {
        otError error = OpenThreadDL_Manager::getInstance().mOtThreadSetEnabled(mOTInst, true);

        ChipLogProgress(DeviceLayer, "Start Thread network: %s", OpenThreadDL_Manager::getInstance().mOtThreadErrorToString(error));
    }
#endif // CHIP_PROGRESS_LOGGING
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_JoinerStart(void)
{
    CHIP_ERROR error = CHIP_NO_ERROR;

    Impl()->LockThreadStack();
    VerifyOrExit(!OpenThreadDL_Manager::getInstance().mOtDatasetIsCommissioned(mOTInst) &&
                     OpenThreadDL_Manager::getInstance().mOtThreadGetDeviceRole(mOTInst) == OT_DEVICE_ROLE_DISABLED,
                 error = MapOpenThreadError(OT_ERROR_INVALID_STATE));
    VerifyOrExit(OpenThreadDL_Manager::getInstance().mOtJoinerGetState(mOTInst) == OT_JOINER_STATE_IDLE,
                 error = MapOpenThreadError(OT_ERROR_BUSY));

    if (!OpenThreadDL_Manager::getInstance().mOtIp6IsEnabled(mOTInst))
    {
        SuccessOrExit(error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtIp6SetEnabled(mOTInst, true)));
    }

    {
        otJoinerDiscerner discerner;
        // This is dead code to remove, so the placeholder value is OK.
        // See ThreadStackManagerImpl.
        uint16_t discriminator = 3840;

        discerner.mLength = 12;
        discerner.mValue  = discriminator;

        ChipLogProgress(DeviceLayer, "Joiner Discerner: %u", discriminator);
        (void) !OpenThreadDL_Manager::getInstance().mOtJoinerSetDiscerner(mOTInst, &discerner);
    }

    {
        otJoinerPskd pskd;
        // This is dead code to remove, so the placeholder value is OK.d
        // See ThreadStackManagerImpl.
        uint32_t pincode = 20202021;

        snprintf(pskd.m8, sizeof(pskd.m8) - 1, "%09" PRIu32, pincode);

        ChipLogProgress(DeviceLayer, "Joiner PSKd: %s", pskd.m8);
        error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtJoinerStart(
            mOTInst, pskd.m8, NULL, NULL, NULL, NULL, NULL, &GenericThreadStackManagerImpl_OpenThread::OnJoinerComplete, this));
    }

exit:
    Impl()->UnlockThreadStack();

    ChipLogProgress(DeviceLayer, "Joiner start: %s", chip::ErrorStr(error));

    return error;
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::_UpdateNetworkStatus()
{
    // Thread is not enabled, then we are not trying to connect to the network.
    VerifyOrReturn(ThreadStackMgrImpl().IsThreadEnabled() && mpStatusChangeCallback != nullptr);

    // ByteSpan datasetTLV;
    Thread::OperationalDataset dataset;
    ByteSpan extpanid;

    // If we have not provisioned any Thread network, return the status from last network scan,
    // If we have provisioned a network, we assume the ot-br-posix is activitely connecting to that network.
    ReturnOnFailure(ThreadStackMgrImpl().GetThreadProvision(dataset));
    // The Thread network is not enabled, but has a different extended pan id.
    ReturnOnFailure(dataset.GetExtendedPanIdAsByteSpan(extpanid));
    // If we don't have a valid dataset, we are not attempting to connect the network.

    // We have already connected to the network, thus return success.
    if (ThreadStackMgrImpl().IsThreadAttached())
    {
        mpStatusChangeCallback->OnNetworkingStatusChange(NetworkCommissioning::Status::kSuccess, MakeOptional(extpanid),
                                                         NullOptional);
    }
    else
    {
        mpStatusChangeCallback->OnNetworkingStatusChange(NetworkCommissioning::Status::kNetworkNotFound, MakeOptional(extpanid),
                                                         MakeOptional(static_cast<int32_t>(OT_ERROR_DETACHED)));
    }
}

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD_SRP_CLIENT

static_assert(OPENTHREAD_API_VERSION >= 156, "SRP Client requires a more recent OpenThread version");

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::OnSrpClientNotification(otError aError,
                                                                                  const otSrpClientHostInfo * aHostInfo,
                                                                                  const otSrpClientService * aServices,
                                                                                  const otSrpClientService * aRemovedServices,
                                                                                  void * aContext)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::OnSrpClientNotification");

    const char * errorStr = nullptr;

    switch (aError)
    {
    case OT_ERROR_NONE: {
        ChipLogDetail(DeviceLayer, "SRP update succeeded");

        if (aHostInfo)
        {
            if (aHostInfo->mState == OT_SRP_CLIENT_ITEM_STATE_REMOVED)
            {
                // Clear memory for removed host
                memset(ThreadStackMgrImpl().mSrpClient.mHostName, 0, sizeof(ThreadStackMgrImpl().mSrpClient.mHostName));

                ThreadStackMgrImpl().mSrpClient.mIsInitialized = true;
                ThreadStackMgrImpl().mSrpClient.mInitializedCallback(ThreadStackMgrImpl().mSrpClient.mCallbackContext,
                                                                     CHIP_NO_ERROR);
            }
        }

        if (aRemovedServices)
        {
            otSrpClientService * otService = const_cast<otSrpClientService *>(aRemovedServices);
            otSrpClientService * next      = nullptr;
            using Service                  = typename SrpClient::Service;

            // Clear memory for all removed services.
            do
            {
                next         = otService->mNext;
                auto service = reinterpret_cast<Service *>(reinterpret_cast<size_t>(otService) - offsetof(Service, mService));
                memset(service, 0, sizeof(Service));
                otService = next;
            } while (otService);
        }
        break;
    }
    case OT_ERROR_PARSE:
        errorStr = "parsing operation failed";
        break;
    case OT_ERROR_NOT_FOUND:
        errorStr = "domain name or RRset does not exist";
        break;
    case OT_ERROR_NOT_IMPLEMENTED:
        errorStr = "server does not support query type";
        break;
    case OT_ERROR_SECURITY:
        errorStr = "operation refused for security reasons";
        break;
    case OT_ERROR_DUPLICATED:
        errorStr = "domain name or RRset is duplicated";
        break;
    case OT_ERROR_RESPONSE_TIMEOUT:
        errorStr = "timed out waiting on server response";
        break;
    case OT_ERROR_INVALID_ARGS:
        errorStr = "invalid service structure detected";
        break;
    case OT_ERROR_NO_BUFS:
        errorStr = "insufficient buffer to handle message";
        break;
    case OT_ERROR_FAILED:
        errorStr = "internal server error";
        break;
    default:
        errorStr = "unknown error";
        break;
    }

    if (errorStr != nullptr)
    {
        ChipLogError(DeviceLayer, "SRP update error: %s", errorStr);
    }
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::OnSrpClientStateChange(const otSockAddr * aServerSockAddr,
                                                                                 void * aContext)
{
    if (aServerSockAddr)
    {
        ChipLogProgress(DeviceLayer, "SRP Client was started, detected server: %04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x",
                        Encoding::BigEndian::HostSwap16(aServerSockAddr->mAddress.mFields.m16[0]),
                        Encoding::BigEndian::HostSwap16(aServerSockAddr->mAddress.mFields.m16[1]),
                        Encoding::BigEndian::HostSwap16(aServerSockAddr->mAddress.mFields.m16[2]),
                        Encoding::BigEndian::HostSwap16(aServerSockAddr->mAddress.mFields.m16[3]),
                        Encoding::BigEndian::HostSwap16(aServerSockAddr->mAddress.mFields.m16[4]),
                        Encoding::BigEndian::HostSwap16(aServerSockAddr->mAddress.mFields.m16[5]),
                        Encoding::BigEndian::HostSwap16(aServerSockAddr->mAddress.mFields.m16[6]),
                        Encoding::BigEndian::HostSwap16(aServerSockAddr->mAddress.mFields.m16[7]));

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD_DNS_CLIENT
        // Set DNS server config to be set at the SRP server address
        otDnsQueryConfig dnsConfig =
            *OpenThreadDL_Manager::getInstance().mOtDnsClientGetDefaultConfig(ThreadStackMgrImpl().OTInstance());
        dnsConfig.mServerSockAddr.mAddress = aServerSockAddr->mAddress;
        OpenThreadDL_Manager::getInstance().mOtDnsClientSetDefaultConfig(ThreadStackMgrImpl().OTInstance(), &dnsConfig);
#endif
    }
    else
    {
        ChipLogProgress(DeviceLayer, "SRP Client was stopped, because current server is no longer detected.");
    }
}

template <class ImplClass>
bool GenericThreadStackManagerImpl_OpenThread<ImplClass>::SrpClient::Service::Matches(const char * instanceName,
                                                                                      const char * name) const
{
    return IsUsed() && (strcmp(mService.mInstanceName, instanceName) == 0) && (strcmp(mService.mName, name) == 0);
}

template <class ImplClass>
bool GenericThreadStackManagerImpl_OpenThread<ImplClass>::SrpClient::Service::Matches(
    const char * instanceName, const char * name, uint16_t port, const Span<const char * const> & subTypes,
    const Span<const Dnssd::TextEntry> & txtEntries) const
{
    size_t myNumSubTypes = 0;

    for (const char * const * mySubType = mService.mSubTypeLabels; (mySubType != nullptr) && (*mySubType != nullptr); ++mySubType)
    {
        myNumSubTypes++;
    }

    VerifyOrReturnError(Matches(instanceName, name) && mService.mPort == port, false);
    VerifyOrReturnError(myNumSubTypes == subTypes.size() && mService.mNumTxtEntries == txtEntries.size(), false);

    const char * const * mySubType = mService.mSubTypeLabels;

    for (const char * subType : subTypes)
    {
        VerifyOrReturnError(strcmp(*mySubType, subType) == 0, false);
        ++mySubType;
    }

    const otDnsTxtEntry * myTxtEntry = mService.mTxtEntries;

    for (const Dnssd::TextEntry & txtEntry : txtEntries)
    {
        VerifyOrReturnError(strcmp(myTxtEntry->mKey, txtEntry.mKey) == 0, false);
        VerifyOrReturnError(
            ByteSpan(myTxtEntry->mValue, myTxtEntry->mValueLength).data_equal(ByteSpan(txtEntry.mData, txtEntry.mDataSize)), false);
        ++myTxtEntry;
    }

    return true;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_AddSrpService(const char * aInstanceName, const char * aName,
                                                                               uint16_t aPort,
                                                                               const Span<const char * const> & aSubTypes,
                                                                               const Span<const Dnssd::TextEntry> & aTxtEntries,
                                                                               uint32_t aLeaseInterval, uint32_t aKeyLeaseInterval)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_AddSrpService");

    CHIP_ERROR error                         = CHIP_NO_ERROR;
    typename SrpClient::Service * srpService = nullptr;
    size_t entryId                           = 0;
    FixedBufferAllocator alloc;

    VerifyOrReturnError(mSrpClient.mIsInitialized, CHIP_ERROR_WELL_UNINITIALIZED);
    VerifyOrReturnError(aInstanceName, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(aName, CHIP_ERROR_INVALID_ARGUMENT);

    Impl()->LockThreadStack();

    // Try to find an empty slot in array for the new service and
    // remove the possible existing entry from anywhere in the list
    for (typename SrpClient::Service & service : mSrpClient.mServices)
    {
        if (service.Matches(aInstanceName, aName, aPort, aSubTypes, aTxtEntries))
        {
            // Re-adding existing service without any changes
            service.mIsInvalid = false;
            ExitNow();
        }

        if (service.Matches(aInstanceName, aName))
        {
            // Updating existing service
            SuccessOrExit(error = MapOpenThreadError(
                              OpenThreadDL_Manager::getInstance().mOtSrpClientClearService(mOTInst, &service.mService)));
            // Clear memory immediately, as OnSrpClientNotification will not be called.
            memset(&service, 0, sizeof(service));
        }

        if ((srpService == nullptr) && !service.IsUsed())
        {
            // Assign first empty slot found in array for a new service.
            srpService = &service;

            // Keep looping to remove possible existing entry further in the list
        }
    }

    // Verify there is a slot found for the new service.
    VerifyOrExit(srpService != nullptr, error = CHIP_ERROR_BUFFER_TOO_SMALL);
    alloc.Init(srpService->mServiceBuffer);

    OpenThreadDL_Manager::getInstance().mOtSrpClientSetLeaseInterval(mOTInst, aLeaseInterval);
    OpenThreadDL_Manager::getInstance().mOtSrpClientSetKeyLeaseInterval(mOTInst, aKeyLeaseInterval);

    srpService->mService.mInstanceName = alloc.Clone(aInstanceName);
    srpService->mService.mName         = alloc.Clone(aName);
    srpService->mService.mPort         = aPort;

    VerifyOrExit(aSubTypes.size() < ArraySize(srpService->mSubTypes), error = CHIP_ERROR_BUFFER_TOO_SMALL);
    entryId = 0;

    for (const char * subType : aSubTypes)
    {
        srpService->mSubTypes[entryId++] = alloc.Clone(subType);
    }

    srpService->mSubTypes[entryId]      = nullptr;
    srpService->mService.mSubTypeLabels = srpService->mSubTypes;

    // Initialize TXT entries
    VerifyOrExit(aTxtEntries.size() <= ArraySize(srpService->mTxtEntries), error = CHIP_ERROR_BUFFER_TOO_SMALL);
    entryId = 0;

    for (const chip::Dnssd::TextEntry & entry : aTxtEntries)
    {
        using OtTxtValueLength = decltype(srpService->mTxtEntries[entryId].mValueLength);
        static_assert(SrpClient::kServiceBufferSize <= std::numeric_limits<OtTxtValueLength>::max(),
                      "DNS TXT value length may not fit in otDnsTxtEntry structure");

        // TXT entry keys are constants, so they don't need to be cloned
        srpService->mTxtEntries[entryId].mKey         = entry.mKey;
        srpService->mTxtEntries[entryId].mValue       = alloc.Clone(entry.mData, entry.mDataSize);
        srpService->mTxtEntries[entryId].mValueLength = static_cast<OtTxtValueLength>(entry.mDataSize);
        ++entryId;
    }

    using OtNumTxtEntries = decltype(srpService->mService.mNumTxtEntries);
    static_assert(ArraySize(srpService->mTxtEntries) <= std::numeric_limits<OtNumTxtEntries>::max(),
                  "Number of DNS TXT entries may not fit in otSrpClientService structure");
    srpService->mService.mNumTxtEntries = static_cast<OtNumTxtEntries>(aTxtEntries.size());
    srpService->mService.mTxtEntries    = srpService->mTxtEntries;

    VerifyOrExit(!alloc.AnyAllocFailed(), error = CHIP_ERROR_BUFFER_TOO_SMALL);

    ChipLogProgress(DeviceLayer, "advertising srp service: %s.%s", srpService->mService.mInstanceName, srpService->mService.mName);
    error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtSrpClientAddService(mOTInst, &(srpService->mService)));

exit:
    if (srpService != nullptr && error != CHIP_NO_ERROR)
    {
        memset(srpService, 0, sizeof(*srpService));
    }

    Impl()->UnlockThreadStack();
    return error;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_RemoveSrpService(const char * aInstanceName, const char * aName)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_RemoveSrpService");

    CHIP_ERROR error                         = CHIP_NO_ERROR;
    typename SrpClient::Service * srpService = nullptr;

    VerifyOrReturnError(mSrpClient.mIsInitialized, CHIP_ERROR_WELL_UNINITIALIZED);
    VerifyOrReturnError(aInstanceName, CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrReturnError(aName, CHIP_ERROR_INVALID_ARGUMENT);

    Impl()->LockThreadStack();

    // Check if service to remove exists.
    for (typename SrpClient::Service & service : mSrpClient.mServices)
    {
        if (service.Matches(aInstanceName, aName))
        {
            srpService = &service;
            break;
        }
    }

    VerifyOrExit(srpService, error = MapOpenThreadError(OT_ERROR_NOT_FOUND));

    ChipLogProgress(DeviceLayer, "removing srp service: %s.%s", aInstanceName, aName);
    error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtSrpClientRemoveService(mOTInst, &(srpService->mService)));

exit:
    Impl()->UnlockThreadStack();

    return error;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_InvalidateAllSrpServices()
{
    Impl()->LockThreadStack();

    for (typename SrpClient::Service & service : mSrpClient.mServices)
    {
        if (service.IsUsed())
        {
            service.mIsInvalid = true;
        }
    }

    Impl()->UnlockThreadStack();
    return CHIP_NO_ERROR;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_RemoveInvalidSrpServices()
{
    CHIP_ERROR error = CHIP_NO_ERROR;

    VerifyOrReturnError(mSrpClient.mIsInitialized, CHIP_ERROR_WELL_UNINITIALIZED);

    Impl()->LockThreadStack();

    for (typename SrpClient::Service & service : mSrpClient.mServices)
    {
        if (service.IsUsed() && service.mIsInvalid)
        {
            ChipLogProgress(DeviceLayer, "removing srp service: %s.%s", service.mService.mInstanceName, service.mService.mName);
            error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtSrpClientRemoveService(mOTInst, &service.mService));
            SuccessOrExit(error);
        }
    }

exit:
    Impl()->UnlockThreadStack();

    return error;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_SetupSrpHost(const char * aHostName)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_SetupSrpHost %s", aHostName);

    CHIP_ERROR error = CHIP_NO_ERROR;
#if OPENTHREAD_API_VERSION < 218
    Inet::IPAddress hostAddress;
#endif

    VerifyOrReturnError(mSrpClient.mIsInitialized, CHIP_ERROR_WELL_UNINITIALIZED);

    Impl()->LockThreadStack();

    VerifyOrExit(aHostName, error = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(strlen(aHostName) <= Dnssd::kHostNameMaxLength, error = CHIP_ERROR_INVALID_STRING_LENGTH);

    // Avoid adding the same host name multiple times
    if (strcmp(mSrpClient.mHostName, aHostName) != 0)
    {
        strcpy(mSrpClient.mHostName, aHostName);
        error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtSrpClientSetHostName(mOTInst, mSrpClient.mHostName));
        SuccessOrExit(error);

#if OPENTHREAD_API_VERSION >= 218
        error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtSrpClientEnableAutoHostAddress(mOTInst));
#endif
    }

#if OPENTHREAD_API_VERSION < 218
    // Check if device has any external IPv6 assigned. If not, host will be set without IPv6 addresses
    // and updated later on.
    if (ThreadStackMgr().GetExternalIPv6Address(hostAddress) == CHIP_NO_ERROR)
    {
        memcpy(&mSrpClient.mHostAddress.mFields.m32, hostAddress.Addr, sizeof(hostAddress.Addr));
        error = MapOpenThreadError(
            OpenThreadDL_Manager::getInstance().mOtSrpClientSetHostAddresses(mOTInst, &mSrpClient.mHostAddress, 1));
    }
#endif

exit:
    Impl()->UnlockThreadStack();

    return error;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_ClearSrpHost(const char * aHostName)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_ClearSrpHost %s", aHostName);

    CHIP_ERROR error = CHIP_NO_ERROR;

    Impl()->LockThreadStack();

    VerifyOrExit(aHostName, error = CHIP_ERROR_INVALID_ARGUMENT);
    VerifyOrExit(strlen(aHostName) <= Dnssd::kHostNameMaxLength, error = CHIP_ERROR_INVALID_STRING_LENGTH);
    VerifyOrExit(mSrpClient.mInitializedCallback, error = CHIP_ERROR_INCORRECT_STATE);

    // Add host and remove it with notifying SRP server to clean old information related to the host.
    // Avoid adding the same host name multiple times
    if (strcmp(mSrpClient.mHostName, aHostName) != 0)
    {
        strcpy(mSrpClient.mHostName, aHostName);
        error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtSrpClientSetHostName(mOTInst, mSrpClient.mHostName));
        SuccessOrExit(error);
    }
    error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtSrpClientRemoveHostAndServices(mOTInst, false, true));

exit:
    Impl()->UnlockThreadStack();

    return error;
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_SetSrpDnsCallbacks(DnsAsyncReturnCallback aInitCallback,
                                                                                    DnsAsyncReturnCallback aErrorCallback,
                                                                                    void * aContext)
{
    ChipLogProgress(DeviceLayer, "GenericThreadStackManagerImpl_OpenThread::_SetSrpDnsCallbacks");

    mSrpClient.mInitializedCallback = aInitCallback;
    mSrpClient.mCallbackContext     = aContext;
    return CHIP_NO_ERROR;
}

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD_DNS_CLIENT
template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::FromOtDnsResponseToMdnsData(
    otDnsServiceInfo & serviceInfo, const char * serviceType, chip::Dnssd::DnssdService & mdnsService,
    DnsServiceTxtEntries & serviceTxtEntries)
{
    char protocol[chip::Dnssd::kDnssdProtocolTextMaxSize + 1];

    if (strchr(serviceInfo.mHostNameBuffer, '.') == nullptr)
        return CHIP_ERROR_INVALID_ARGUMENT;

    // Extract from the <hostname>.<domain-name>. the <hostname> part.
    size_t substringSize = strchr(serviceInfo.mHostNameBuffer, '.') - serviceInfo.mHostNameBuffer;
    if (substringSize >= ArraySize(mdnsService.mHostName))
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    Platform::CopyString(mdnsService.mHostName, serviceInfo.mHostNameBuffer);

    if (strchr(serviceType, '.') == nullptr)
        return CHIP_ERROR_INVALID_ARGUMENT;

    // Extract from the <type>.<protocol>.<domain-name>. the <type> part.
    substringSize = strchr(serviceType, '.') - serviceType;
    if (substringSize >= ArraySize(mdnsService.mType))
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    Platform::CopyString(mdnsService.mType, serviceType);

    // Extract from the <type>.<protocol>.<domain-name>. the <protocol> part.
    const char * protocolSubstringStart = serviceType + substringSize + 1;

    if (strchr(protocolSubstringStart, '.') == nullptr)
        return CHIP_ERROR_INVALID_ARGUMENT;

    substringSize = strchr(protocolSubstringStart, '.') - protocolSubstringStart;
    if (substringSize >= ArraySize(protocol))
    {
        return CHIP_ERROR_INVALID_ARGUMENT;
    }
    Platform::CopyString(protocol, protocolSubstringStart);

    if (strncmp(protocol, "_udp", chip::Dnssd::kDnssdProtocolTextMaxSize) == 0)
    {
        mdnsService.mProtocol = chip::Dnssd::DnssdServiceProtocol::kDnssdProtocolUdp;
    }
    else if (strncmp(protocol, "_tcp", chip::Dnssd::kDnssdProtocolTextMaxSize) == 0)
    {
        mdnsService.mProtocol = chip::Dnssd::DnssdServiceProtocol::kDnssdProtocolTcp;
    }
    else
    {
        mdnsService.mProtocol = chip::Dnssd::DnssdServiceProtocol::kDnssdProtocolUnknown;
    }
    mdnsService.mPort        = serviceInfo.mPort;
    mdnsService.mInterface   = Inet::InterfaceId::Null();
    mdnsService.mAddressType = Inet::IPAddressType::kIPv6;
    mdnsService.mAddress     = chip::Optional<chip::Inet::IPAddress>(ToIPAddress(serviceInfo.mHostAddress));

    otDnsTxtEntryIterator iterator;
    OpenThreadDL_Manager::getInstance().mOtDnsInitTxtEntryIterator(&iterator, serviceInfo.mTxtData, serviceInfo.mTxtDataSize);

    otDnsTxtEntry txtEntry;
    FixedBufferAllocator alloc(serviceTxtEntries.mBuffer);

    uint8_t entryIndex = 0;
    while ((OpenThreadDL_Manager::getInstance().mOtDnsGetNextTxtEntry(&iterator, &txtEntry) == OT_ERROR_NONE) &&
           entryIndex < kMaxDnsServiceTxtEntriesNumber)
    {
        if (txtEntry.mKey == nullptr || txtEntry.mValue == nullptr)
            continue;

        serviceTxtEntries.mTxtEntries[entryIndex].mKey      = alloc.Clone(txtEntry.mKey);
        serviceTxtEntries.mTxtEntries[entryIndex].mData     = alloc.Clone(txtEntry.mValue, txtEntry.mValueLength);
        serviceTxtEntries.mTxtEntries[entryIndex].mDataSize = txtEntry.mValueLength;
        entryIndex++;
    }

    ReturnErrorCodeIf(alloc.AnyAllocFailed(), CHIP_ERROR_BUFFER_TOO_SMALL);

    mdnsService.mTextEntries   = serviceTxtEntries.mTxtEntries;
    mdnsService.mTextEntrySize = entryIndex;

    return CHIP_NO_ERROR;
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::DispatchResolve(intptr_t context)
{
    DnsResult * dnsResult         = reinterpret_cast<DnsResult *>(context);
    Dnssd::DnssdService & service = dnsResult->mMdnsService;
    Span<Inet::IPAddress> ipAddrs;

    if (service.mAddress.HasValue())
    {
        ipAddrs = Span<Inet::IPAddress>(&service.mAddress.Value(), 1);
    }

    ThreadStackMgrImpl().mDnsResolveCallback(dnsResult->context, &service, ipAddrs, dnsResult->error);
    Platform::Delete<DnsResult>(dnsResult);
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::DispatchBrowseEmpty(intptr_t context)
{
    auto * dnsResult = reinterpret_cast<DnsResult *>(context);
    ThreadStackMgrImpl().mDnsBrowseCallback(dnsResult->context, nullptr, 0, true, dnsResult->error);
    Platform::Delete<DnsResult>(dnsResult);
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::DispatchBrowse(intptr_t context)
{
    auto * dnsResult = reinterpret_cast<DnsResult *>(context);
    ThreadStackMgrImpl().mDnsBrowseCallback(dnsResult->context, &dnsResult->mMdnsService, 1, false, dnsResult->error);
    Platform::Delete<DnsResult>(dnsResult);
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::OnDnsBrowseResult(otError aError, const otDnsBrowseResponse * aResponse,
                                                                            void * aContext)
{
    CHIP_ERROR error;
    // type buffer size is kDnssdTypeAndProtocolMaxSize + . + kMaxDomainNameSize + . + termination character
    char type[Dnssd::kDnssdTypeAndProtocolMaxSize + SrpClient::kMaxDomainNameSize + 3];
    // hostname buffer size is kHostNameMaxLength + . + kMaxDomainNameSize + . + termination character
    char hostname[Dnssd::kHostNameMaxLength + SrpClient::kMaxDomainNameSize + 3];
    // secure space for the raw TXT data in the worst-case scenario relevant for Matter:
    // each entry consists of txt_entry_size (1B) + txt_entry_key + "=" + txt_entry_data
    uint8_t txtBuffer[kMaxDnsServiceTxtEntriesNumber + kTotalDnsServiceTxtBufferSize];
    otDnsServiceInfo serviceInfo;
    uint16_t index = 0;

    if (ThreadStackMgrImpl().mDnsBrowseCallback == nullptr)
    {
        ChipLogError(DeviceLayer, "Invalid dns browse callback");
        return;
    }

    VerifyOrExit(aError == OT_ERROR_NONE, error = MapOpenThreadError(aError));

    error =
        MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtDnsBrowseResponseGetServiceName(aResponse, type, sizeof(type)));

    VerifyOrExit(error == CHIP_NO_ERROR, );

    char serviceName[Dnssd::Common::kInstanceNameMaxLength + 1];
    while (OpenThreadDL_Manager::getInstance().mOtDnsBrowseResponseGetServiceInstance(aResponse, index, serviceName,
                                                                                      sizeof(serviceName)) == OT_ERROR_NONE)
    {
        serviceInfo.mHostNameBuffer     = hostname;
        serviceInfo.mHostNameBufferSize = sizeof(hostname);
        serviceInfo.mTxtData            = txtBuffer;
        serviceInfo.mTxtDataSize        = sizeof(txtBuffer);

        error = MapOpenThreadError(
            OpenThreadDL_Manager::getInstance().mOtDnsBrowseResponseGetServiceInfo(aResponse, serviceName, &serviceInfo));

        VerifyOrExit(error == CHIP_NO_ERROR, );

        DnsResult * dnsResult = Platform::New<DnsResult>(aContext, CHIP_NO_ERROR);
        error = FromOtDnsResponseToMdnsData(serviceInfo, type, dnsResult->mMdnsService, dnsResult->mServiceTxtEntry);
        if (CHIP_NO_ERROR == error)
        {
            // Invoke callback for every service one by one instead of for the whole
            // list due to large memory size needed to allocate on stack.
            static_assert(ArraySize(dnsResult->mMdnsService.mName) >= ArraySize(serviceName),
                          "The target buffer must be big enough");
            Platform::CopyString(dnsResult->mMdnsService.mName, serviceName);
            DeviceLayer::PlatformMgr().ScheduleWork(DispatchBrowse, reinterpret_cast<intptr_t>(dnsResult));
        }
        else
        {
            Platform::Delete<DnsResult>(dnsResult);
        }
        index++;
    }

exit:
    // Invoke callback to notify about end-of-browse or failure
    DnsResult * dnsResult = Platform::New<DnsResult>(aContext, error);
    DeviceLayer::PlatformMgr().ScheduleWork(DispatchBrowseEmpty, reinterpret_cast<intptr_t>(dnsResult));
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_DnsBrowse(const char * aServiceName, DnsBrowseCallback aCallback,
                                                                           void * aContext)
{
    CHIP_ERROR error = CHIP_NO_ERROR;

    Impl()->LockThreadStack();

    VerifyOrExit(aServiceName, error = CHIP_ERROR_INVALID_ARGUMENT);

    mDnsBrowseCallback = aCallback;

    // Append default SRP domain name to the service name.
    // fullServiceName buffer size is kDnssdFullTypeAndProtocolMaxSize + . + kDefaultDomainNameSize + null-terminator.
    char fullServiceName[Dnssd::kDnssdFullTypeAndProtocolMaxSize + 1 + SrpClient::kDefaultDomainNameSize + 1];
    snprintf(fullServiceName, sizeof(fullServiceName), "%s.%s", aServiceName, SrpClient::kDefaultDomainName);

    error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtDnsClientBrowse(mOTInst, fullServiceName, OnDnsBrowseResult,
                                                                                      aContext, /* config */ nullptr));

exit:

    Impl()->UnlockThreadStack();

    return error;
}

template <class ImplClass>
void GenericThreadStackManagerImpl_OpenThread<ImplClass>::OnDnsResolveResult(otError aError, const otDnsServiceResponse * aResponse,
                                                                             void * aContext)
{
    CHIP_ERROR error;
    DnsResult * dnsResult = Platform::New<DnsResult>(aContext, MapOpenThreadError(aError));
    // type buffer size is kDnssdTypeAndProtocolMaxSize + . + kMaxDomainNameSize + . + termination character
    char type[Dnssd::kDnssdTypeAndProtocolMaxSize + SrpClient::kMaxDomainNameSize + 3];
    // hostname buffer size is kHostNameMaxLength + . + kMaxDomainNameSize + . + termination character
    char hostname[Dnssd::kHostNameMaxLength + SrpClient::kMaxDomainNameSize + 3];
    // secure space for the raw TXT data in the worst-case scenario relevant for Matter:
    // each entry consists of txt_entry_size (1B) + txt_entry_key + "=" + txt_entry_data
    uint8_t txtBuffer[kMaxDnsServiceTxtEntriesNumber + kTotalDnsServiceTxtBufferSize];
    otDnsServiceInfo serviceInfo;

    if (ThreadStackMgrImpl().mDnsResolveCallback == nullptr)
    {
        ChipLogError(DeviceLayer, "Invalid dns browse callback");
        return;
    }

    VerifyOrExit(aError == OT_ERROR_NONE, error = MapOpenThreadError(aError));

    error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtDnsServiceResponseGetServiceName(
        aResponse, dnsResult->mMdnsService.mName, sizeof(dnsResult->mMdnsService.mName), type, sizeof(type)));

    VerifyOrExit(error == CHIP_NO_ERROR, );

    serviceInfo.mHostNameBuffer     = hostname;
    serviceInfo.mHostNameBufferSize = sizeof(hostname);
    serviceInfo.mTxtData            = txtBuffer;
    serviceInfo.mTxtDataSize        = sizeof(txtBuffer);

    error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtDnsServiceResponseGetServiceInfo(aResponse, &serviceInfo));

    VerifyOrExit(error == CHIP_NO_ERROR, );

    error = FromOtDnsResponseToMdnsData(serviceInfo, type, dnsResult->mMdnsService, dnsResult->mServiceTxtEntry);

exit:

    dnsResult->error = error;
    DeviceLayer::PlatformMgr().ScheduleWork(DispatchResolve, reinterpret_cast<intptr_t>(dnsResult));
}

template <class ImplClass>
CHIP_ERROR GenericThreadStackManagerImpl_OpenThread<ImplClass>::_DnsResolve(const char * aServiceName, const char * aInstanceName,
                                                                            DnsResolveCallback aCallback, void * aContext)
{
    CHIP_ERROR error = CHIP_NO_ERROR;

    Impl()->LockThreadStack();
    const otDnsQueryConfig * defaultConfig = OpenThreadDL_Manager::getInstance().mOtDnsClientGetDefaultConfig(mOTInst);

    VerifyOrExit(aServiceName && aInstanceName, error = CHIP_ERROR_INVALID_ARGUMENT);

    mDnsResolveCallback = aCallback;

    // Append default SRP domain name to the service name.
    // fullServiceName buffer size is kDnssdTypeAndProtocolMaxSize + . separator + kDefaultDomainNameSize + termination character.
    char fullServiceName[Dnssd::kDnssdTypeAndProtocolMaxSize + 1 + SrpClient::kDefaultDomainNameSize + 1];
    snprintf(fullServiceName, sizeof(fullServiceName), "%s.%s", aServiceName, SrpClient::kDefaultDomainName);

    error = MapOpenThreadError(OpenThreadDL_Manager::getInstance().mOtDnsClientResolveService(
        mOTInst, aInstanceName, fullServiceName, OnDnsResolveResult, aContext, defaultConfig));

exit:

    Impl()->UnlockThreadStack();

    return error;
}
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD_DNS_CLIENT
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD_SRP_CLIENT

// Fully instantiate the generic implementation class in whatever compilation unit includes this file.
// NB: This must come after all templated class members are defined.
template class GenericThreadStackManagerImpl_OpenThread<ThreadStackManagerImpl>;

} // namespace Internal
} // namespace DeviceLayer
} // namespace chip

#endif // GENERIC_THREAD_STACK_MANAGER_IMPL_OPENTHREAD_IPP
