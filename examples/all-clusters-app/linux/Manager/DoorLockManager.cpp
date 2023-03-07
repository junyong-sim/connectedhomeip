/**
 *
 *    Copyright (c) 2022 Project CHIP Authors
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
#include "DoorLockManager.h"
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/util/af.h>

using namespace chip;
using namespace chip::app::Clusters::DoorLock;

static constexpr size_t doorLockManagerSize = EMBER_AF_DOOR_LOCK_CLUSTER_SERVER_ENDPOINT_COUNT;

namespace {
    DoorLockManager * doorLockManager[doorLockManagerSize] = { nullptr };
}

bool emberAfPluginDoorLockOnDoorLockCommand(chip::EndpointId endpointId, const Optional<ByteSpan> & pinCode, DlOperationError & err)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::DoorLock::emberAfPluginDoorLockOnDoorLockCommand");
    return true;
}

bool emberAfPluginDoorLockOnDoorUnlockCommand(chip::EndpointId endpointId, const Optional<ByteSpan> & pinCode,
                                              DlOperationError & err)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::DoorLock::emberAfPluginDoorLockOnDoorUnlockCommand");
    return true;
}

void DoorLockManager::InitManager(int endpoint, int _device_No)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: DoorLockManager::InitManager");
    uint16_t ep = emberAfFindClusterServerEndpointIndex(static_cast<chip::EndpointId>(endpoint), app::Clusters::DoorLock::Id);
    if (!(ep != kEmberInvalidEndpointIndex && ep < EMBER_AF_DOOR_LOCK_CLUSTER_SERVER_ENDPOINT_COUNT)) {
        ChipLogError(Zcl, "samsung-all-clusters-app::DoorLock::NewManager: endpoint %d not found", endpoint);
        return;
    }
    if (doorLockManager[ep] != nullptr) {
        ChipLogError(Zcl, "samsung-all-clusters-app::DoorLock::NewManager: endpoint %d already has a manager", endpoint);
        return;
    }
    DoorLockManager * mgr = new DoorLockManager();
    doorLockManager[ep] = mgr;
    mgr->setDeviceNo(_device_No);
}

DoorLockManager * GetDoorLockManager(EndpointId endpoint)
{
    uint16_t ep = emberAfFindClusterServerEndpointIndex(endpoint, app::Clusters::DoorLock::Id);
    if (ep == kEmberInvalidEndpointIndex || ep >= EMBER_AF_DOOR_LOCK_CLUSTER_SERVER_ENDPOINT_COUNT)
        return nullptr;
    else
        return doorLockManager[ep];
}

bool DoorLockManager::SetLockType(int endpoint, int value)
{
    EmberAfStatus status = app::Clusters::DoorLock::Attributes::LockType::Set(static_cast<chip::EndpointId>(endpoint), static_cast<app::Clusters::DoorLock::DlLockType>(value));
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool DoorLockManager::SetLockState(int endpoint, int value)
{
    EmberAfStatus status = app::Clusters::DoorLock::Attributes::LockState::Set(static_cast<chip::EndpointId>(endpoint), static_cast<app::Clusters::DoorLock::DlLockState>(value));
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool DoorLockManager::SetActuatorEnabled(int endpoint, bool value)
{
    EmberAfStatus status = app::Clusters::DoorLock::Attributes::ActuatorEnabled::Set(static_cast<chip::EndpointId>(endpoint), value);
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool DoorLockManager::SetAutoRelockTime(int endpoint, int value)
{
    EmberAfStatus status = app::Clusters::DoorLock::Attributes::AutoRelockTime::Set(static_cast<chip::EndpointId>(endpoint), static_cast<uint32_t>(value));
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool DoorLockManager::SetOperatingMode(int endpoint, int value)
{
    EmberAfStatus status = app::Clusters::DoorLock::Attributes::OperatingMode::Set(static_cast<chip::EndpointId>(endpoint), static_cast<app::Clusters::DoorLock::DlOperatingMode>(value));
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool DoorLockManager::SetSupportedOperatingModes(int endpoint, int value)
{
    EmberAfStatus status = app::Clusters::DoorLock::Attributes::SupportedOperatingModes::Set(static_cast<chip::EndpointId>(endpoint), static_cast<app::Clusters::DoorLock::DlSupportedOperatingModes>(value));
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

void DoorLockManager::ReceiveLockStateChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: DoorLockManager::ReceiveLockStateChanged");
    DoorLockManager * mgr = GetDoorLockManager(endpoint);
    if (mgr == nullptr) {
        ChipLogError(Zcl, "DoorLockManager null");
        return;
    }
    mgr->SetLockStateChanged(value);
}

void DoorLockManager::SetLockStateChanged(int value)
{
    /*
    * 0x00 fully locked
    * 0x01 locked
    * 0x02 unlocked
    * 0x03 unknown
    */
    char cmd[100];
    if (device_No < 0 || device_No > 9)
        device_No = 0;
    sprintf(cmd, "echo 'lock:%d' > /tmp/chip_pipe_device%d", value, device_No);
    system(cmd);
}

void DoorLockManager::setDeviceNo(int num)
{
    device_No = num;
}
