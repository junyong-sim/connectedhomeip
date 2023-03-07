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
#include "BoolManager.h"
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/util/af.h>

using namespace chip;
using namespace chip::app::Clusters::BooleanState;

static constexpr size_t kBoolManagerTableSize = EMBER_AF_BOOLEAN_STATE_CLUSTER_SERVER_ENDPOINT_COUNT;

namespace {

BoolManager * gBoolManagerTable[kBoolManagerTableSize] = { nullptr };

}

void emberAfBoolManagerClusterInitCallback(EndpointId endpoint)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::BoolManager::PostClusterInit");
}

void BoolManager::NewManager(int endpoint, int _device_No)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::BoolManager::NewManager");
    uint16_t ep = emberAfFindClusterServerEndpointIndex(static_cast<chip::EndpointId>(endpoint), app::Clusters::BooleanState::Id);
    VerifyOrReturn(ep != kEmberInvalidEndpointIndex && ep < EMBER_AF_BOOLEAN_STATE_CLUSTER_SERVER_ENDPOINT_COUNT,
                   ChipLogError(Zcl, "samsung-all-clusters-app::BoolManager::NewManager: endpoint %d not found", endpoint));

    VerifyOrReturn(gBoolManagerTable[ep] == nullptr,
                   ChipLogError(Zcl, "samsung-all-clusters-app::BoolManager::NewManager: endpoint %d already has a manager", endpoint));
    BoolManager * mgr = new BoolManager();
    gBoolManagerTable[ep] = mgr;
    mgr->setDeviceNo(_device_No);
}

BoolManager * GetBoolManager(EndpointId endpoint)
{
    uint16_t ep = emberAfFindClusterServerEndpointIndex(endpoint, app::Clusters::BooleanState::Id);
    return ((ep == kEmberInvalidEndpointIndex || ep >= EMBER_AF_BOOLEAN_STATE_CLUSTER_SERVER_ENDPOINT_COUNT)
                ? nullptr
                : gBoolManagerTable[ep]);
}

bool BoolManager::SetFeatureMap(int endpoint, int value)
{
    EmberAfStatus status = app::Clusters::BooleanState::Attributes::FeatureMap::Set(static_cast<chip::EndpointId>(endpoint), static_cast<uint32_t>(value));
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool BoolManager::SetBatPercentRemaining(int endpoint, int value)
{
    EmberAfStatus status;

    status = app::Clusters::PowerSource::Attributes::BatPercentRemaining::Set(static_cast<chip::EndpointId>(endpoint), static_cast<uint8_t>(value));

    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool BoolManager::SetBooleanState(int endpoint, bool value)
{
    EmberAfStatus status = app::Clusters::BooleanState::Attributes::StateValue::Set(static_cast<chip::EndpointId>(endpoint), static_cast<bool>(value));
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

void BoolManager::PostBoolStateChanged(chip::EndpointId endpoint, bool value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::BoolManager::PostBoolStateChanged");
    BoolManager * mgr = GetBoolManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "BoolManager null"));

    mgr->HandleBoolStateChanged(value);
}

void BoolManager::HandleBoolStateChanged(bool value)
{
    uint8_t boolstate = (value == true) ? 1 : 0;
    char cmd[100];
    if (device_No < 0)
        device_No = 0;
    if (device_No > 9)
        device_No = 0;
    sprintf(cmd, "echo 'bool:%d' > /tmp/chip_pipe_device%d", boolstate, device_No);
    system(cmd);

}

void BoolManager::PostBatPercentRemainingChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::BoolManager::PostBatPercentRemainingChanged");
    BoolManager * mgr = GetBoolManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "BoolManager null"));

    mgr->HandleBatPercentRemainingChanged(value);
}

void BoolManager::HandleBatPercentRemainingChanged(int value)
{
    char cmd[100];
    if (device_No < 0)
        device_No = 0;
    if (device_No > 9)
        device_No = 0;
    sprintf(cmd, "echo 'perc:%d' > /tmp/chip_pipe_device%d", value, device_No);
    system(cmd);
}

void BoolManager::setDeviceNo(int num)
{
    device_No = num;
}
