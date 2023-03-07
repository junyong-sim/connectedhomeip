/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

#include "main-common.h"
#include "AllClustersCommandDelegate.h"
#include "WindowCoveringManager.h"
#include "include/tv-callbacks.h"
#include <app-common/zap-generated/att-storage.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app/CommandHandler.h>
#include <app/clusters/identify-server/identify-server.h>
#include <app/clusters/network-commissioning/network-commissioning.h>
#include <app/clusters/on-off-server/on-off-server.h>
#include <app/server/Server.h>
#include <app/util/af.h>
#include <app/util/attribute-storage.h>
#include <lib/support/CHIPMem.h>
#include <new>
#include <platform/DiagnosticDataProvider.h>
#include <platform/PlatformManager.h>
#include <system/SystemPacketBuffer.h>
#include <transport/SessionManager.h>
#include <transport/raw/PeerAddress.h>

#if CHIP_DEVICE_LAYER_TARGET_DARWIN
#include <platform/Darwin/NetworkCommissioningDriver.h>
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
#include <platform/Darwin/WiFi/NetworkCommissioningWiFiDriver.h>
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI
#endif // CHIP_DEVICE_LAYER_TARGET_DARWIN

#if CHIP_DEVICE_LAYER_TARGET_LINUX
#include <platform/Linux/NetworkCommissioningDriver.h>
#endif // CHIP_DEVICE_LAYER_TARGET_LINUX

#include <Options.h>

using namespace chip;
using namespace chip::app;
using namespace chip::DeviceLayer;

namespace {

constexpr const char kChipEventFifoPathPrefix[] = "/tmp/chip_all_clusters_fifo_device";
LowPowerManager sLowPowerManager;
NamedPipeCommands sChipNamedPipeCommands;
AllClustersCommandDelegate sAllClustersCommandDelegate;
chip::app::Clusters::WindowCovering::WindowCoveringManager sWindowCoveringManager;
EmberAfDeviceType gDeviceTypes[] = { { 0, 1 } };

} // namespace

void OnIdentifyStart(::Identify *)
{
    ChipLogProgress(Zcl, "OnIdentifyStart");
}

void OnIdentifyStop(::Identify *)
{
    ChipLogProgress(Zcl, "OnIdentifyStop");
}

void OnTriggerEffect(::Identify * identify)
{
    switch (identify->mCurrentEffectIdentifier)
    {
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BLINK:
        ChipLogProgress(Zcl, "EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BLINK");
        break;
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BREATHE:
        ChipLogProgress(Zcl, "EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_BREATHE");
        break;
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_OKAY:
        ChipLogProgress(Zcl, "EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_OKAY");
        break;
    case EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_CHANNEL_CHANGE:
        ChipLogProgress(Zcl, "EMBER_ZCL_IDENTIFY_EFFECT_IDENTIFIER_CHANNEL_CHANGE");
        break;
    default:
        ChipLogProgress(Zcl, "No identifier effect");
        return;
    }
}

void ServerClusterInit(int device_type_id)
{
    int d_num = static_cast<int>(LinuxDeviceOptions::GetInstance().device_num);
    switch (device_type_id)
    {
    case 269: // Extended Color Light
    case 268: // Color Temperature Light
        ColorControlManager::NewManager(1, d_num);
    case 257: // Dimming Light
        LevelManager::NewManager(1, d_num);
        // LevelManager::NewManager(2);
        // app::Clusters::LevelControl::Attributes::CurrentLevel::Set(1, 0);
        // app::Clusters::LevelControl::Attributes::CurrentLevel::Set(2, 0);
    case 256: // OnOff Light
        OnOffManager::NewManager(1, d_num);
        // OnOffManager::NewManager(2);
        // app::Clusters::OnOff::Attributes::OnOff::Set(1, false);
        OnOffServer::Instance().initOnOffServer(1);
        break;

    case 10:
        // doorlock server init
        DoorLockManager::NewManager(1, d_num);
        // DoorLockManager::NewManager(2);
        app::Clusters::DoorLock::Attributes::LockType::Set(1, app::Clusters::DoorLock::DlLockType::kMagnetic);
        app::Clusters::DoorLock::Attributes::LockState::Set(1, app::Clusters::DoorLock::DlLockState::kUnlocked);
        app::Clusters::DoorLock::Attributes::ActuatorEnabled::Set(1, true);
        app::Clusters::DoorLock::Attributes::AutoRelockTime::Set(1, 0);
        app::Clusters::DoorLock::Attributes::OperatingMode::Set(1, app::Clusters::DoorLock::OperatingModeEnum::kNormal);
        app::Clusters::DoorLock::Attributes::SupportedOperatingModes::Set(
            1, app::Clusters::DoorLock::DlSupportedOperatingModes::kNormal);
        break;

    case 21:
        // Contact Sensor server init
        BoolManager::NewManager(1, d_num);
    default:
        ChipLogProgress(Zcl, "Wrong device_type_id. Can't initialize Server Clusters");
        break;
    }
}

static Identify gIdentify0 = {
    chip::EndpointId{ 0 }, OnIdentifyStart, OnIdentifyStop, EMBER_ZCL_IDENTIFY_IDENTIFY_TYPE_VISIBLE_LED, OnTriggerEffect,
};

static Identify gIdentify1 = {
    chip::EndpointId{ 1 }, OnIdentifyStart, OnIdentifyStop, EMBER_ZCL_IDENTIFY_IDENTIFY_TYPE_VISIBLE_LED, OnTriggerEffect,
};

// Network commissioning
namespace {
// This file is being used by platforms other than Linux, so we need this check to disable related features since we only
// implemented them on linux.
constexpr EndpointId kNetworkCommissioningEndpointMain      = 0;
constexpr EndpointId kNetworkCommissioningEndpointSecondary = 0xFFFE;

#if CHIP_DEVICE_LAYER_TARGET_LINUX
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
NetworkCommissioning::LinuxThreadDriver sThreadDriver;
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
NetworkCommissioning::LinuxWiFiDriver sWiFiDriver;
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

NetworkCommissioning::LinuxEthernetDriver sEthernetDriver;
#endif // CHIP_DEVICE_LAYER_TARGET_LINUX

#if CHIP_DEVICE_LAYER_TARGET_DARWIN
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
NetworkCommissioning::DarwinWiFiDriver sWiFiDriver;
#endif // CHIP_DEVICE_CONFIG_ENABLE_WIFI

NetworkCommissioning::DarwinEthernetDriver sEthernetDriver;
#endif // CHIP_DEVICE_LAYER_TARGET_DARWIN

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
Clusters::NetworkCommissioning::Instance sThreadNetworkCommissioningInstance(kNetworkCommissioningEndpointMain, &sThreadDriver);
#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD

#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
Clusters::NetworkCommissioning::Instance sWiFiNetworkCommissioningInstance(kNetworkCommissioningEndpointSecondary, &sWiFiDriver);
#endif

Clusters::NetworkCommissioning::Instance sEthernetNetworkCommissioningInstance(kNetworkCommissioningEndpointMain, &sEthernetDriver);
} // namespace

void ApplicationInit()
{
    (void) kNetworkCommissioningEndpointMain;
    // Enable secondary endpoint only when we need it, this should be applied to all platforms.
    emberAfEndpointEnableDisable(kNetworkCommissioningEndpointSecondary, false);

    const bool kThreadEnabled = {
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        LinuxDeviceOptions::GetInstance().mThread
#else
        false
#endif
    };

    const bool kWiFiEnabled = {
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
        LinuxDeviceOptions::GetInstance().mWiFi
#else
        false
#endif
    };

    if (kThreadEnabled && kWiFiEnabled)
    {
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        sThreadNetworkCommissioningInstance.Init();
#endif
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
        sWiFiNetworkCommissioningInstance.Init();
#endif
        // Only enable secondary endpoint for network commissioning cluster when both WiFi and Thread are enabled.
        emberAfEndpointEnableDisable(kNetworkCommissioningEndpointSecondary, true);
    }
    else if (kThreadEnabled)
    {
#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
        sThreadNetworkCommissioningInstance.Init();
#endif
    }
    else if (kWiFiEnabled)
    {
#if CHIP_DEVICE_CONFIG_ENABLE_WIFI
        // If we only enable WiFi on this device, "move" WiFi instance to main NetworkCommissioning cluster endpoint.
        sWiFiNetworkCommissioningInstance.~Instance();
        new (&sWiFiNetworkCommissioningInstance)
            Clusters::NetworkCommissioning::Instance(kNetworkCommissioningEndpointMain, &sWiFiDriver);
        sWiFiNetworkCommissioningInstance.Init();
#endif
    }
    else
    {
        sEthernetNetworkCommissioningInstance.Init();
    }

    int device_type_id       = static_cast<int>(LinuxDeviceOptions::GetInstance().device_type_id);
    gDeviceTypes[0].deviceId = static_cast<uint16_t>(device_type_id);
    emberAfSetDeviceTypeList(1, Span<const EmberAfDeviceType>(gDeviceTypes));
    // emberAfSetDeviceTypeList(2, Span<const EmberAfDeviceType>(gDeviceTypes));

    ServerClusterInit(device_type_id);

    std::string path = kChipEventFifoPathPrefix + std::to_string(LinuxDeviceOptions::GetInstance().device_num);

    if (sChipNamedPipeCommands.Start(path, &sAllClustersCommandDelegate) != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Failed to start CHIP NamedPipeCommands");
        sChipNamedPipeCommands.Stop();
    }
}

void ApplicationExit()
{
    if (sChipNamedPipeCommands.Stop() != CHIP_NO_ERROR)
    {
        ChipLogError(NotSpecified, "Failed to stop CHIP NamedPipeCommands");
    }
}

void emberAfLowPowerClusterInitCallback(EndpointId endpoint)
{
    ChipLogProgress(NotSpecified, "Setting LowPower default delegate to global manager");
    chip::app::Clusters::LowPower::SetDefaultDelegate(endpoint, &sLowPowerManager);
}

void emberAfWindowCoveringClusterInitCallback(chip::EndpointId endpoint)
{
    sWindowCoveringManager.Init(endpoint);
    chip::app::Clusters::WindowCovering::SetDefaultDelegate(endpoint, &sWindowCoveringManager);
    chip::app::Clusters::WindowCovering::ConfigStatusUpdateFeatures(endpoint);
}
