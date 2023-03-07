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
 *    limitations under the License.instance
 */
#include "MotionManager.h"
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/util/af.h>

using namespace chip;
using namespace chip::app::Clusters::OccupancySensing;

static constexpr size_t kMotionManagerTableSize = EMBER_AF_OCCUPANCY_SENSING_CLUSTER_SERVER_ENDPOINT_COUNT;
namespace {

MotionManager * gMotionManagerTable[kMotionManagerTableSize] = { nullptr };

}

void emberAfMotionClusterInitCallback(EndpointId endpoint)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::Motion::PostClusterInit");
}

void MotionManager::NewManager(int endpoint, int _device_No)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: MotionManager::NewManager");
    uint16_t ep = emberAfFindClusterServerEndpointIndex(static_cast<chip::EndpointId>(endpoint), app::Clusters::OccupancySensing::Id);
    VerifyOrReturn(ep != kEmberInvalidEndpointIndex && ep < EMBER_AF_OCCUPANCY_SENSING_CLUSTER_SERVER_ENDPOINT_COUNT,
                   ChipLogError(Zcl, "samsung-all-clusters-app::Motion::NewManager: endpoint %d not found", endpoint));

    VerifyOrReturn(gMotionManagerTable[ep] == nullptr,
                   ChipLogError(Zcl, "samsung-all-clusters-app::Motion::NewManager: endpoint %d already has a manager", endpoint));
    MotionManager * mgr = new MotionManager();
    gMotionManagerTable[ep] = mgr;
    mgr->setDeviceNo(_device_No);
}

MotionManager * GetMotionManager(EndpointId endpoint)
{
    uint16_t ep = emberAfFindClusterServerEndpointIndex(endpoint, app::Clusters::OccupancySensing::Id);
    return ((ep == kEmberInvalidEndpointIndex || ep >= EMBER_AF_OCCUPANCY_SENSING_CLUSTER_SERVER_ENDPOINT_COUNT)
                ? nullptr : gMotionManagerTable[ep]);
}

MotionManager * MotionManager::GetMotionManagerForAbroad(chip::EndpointId endpoint) {
    return GetMotionManager(endpoint);
}

bool MotionManager::SetSensorType(int endpoint, int value)
{
    EmberAfStatus status;
    auto ep = static_cast<chip::EndpointId>(endpoint);
    uint8_t deviceTypeBitmap = static_cast<uint8_t>(value);

    status = Attributes::OccupancySensorType::Set(ep, static_cast<uint8_t>(value));
    EmberAfStatus status2 = Attributes::OccupancySensorTypeBitmap::Set(ep, deviceTypeBitmap);

    return (status == EMBER_ZCL_STATUS_SUCCESS) && (status2 == EMBER_ZCL_STATUS_SUCCESS);
}

bool MotionManager::SetBatPercentRemaining(int endpoint, int value)
{
    EmberAfStatus status;

    status = app::Clusters::PowerSource::Attributes::BatPercentRemaining::Set(static_cast<chip::EndpointId>(endpoint), static_cast<uint8_t>(value));

    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool MotionManager::SetMotion(int endpoint, int value)
{
    EmberAfStatus status = Attributes::Occupancy::Set(static_cast<chip::EndpointId>(endpoint), static_cast<uint8_t>(value));
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool MotionManager::GetMotion(int endpoint, int *value)
{
    uint8_t v;
    EmberAfStatus status = Attributes::Occupancy::Get(static_cast<chip::EndpointId>(endpoint), &v);
    *value = static_cast<uint8_t>(v);
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

void MotionManager::setDeviceNo(int num)
{
    if (device_No < 0 || device_No > 9) {
        device_No = 0;
    } else {
        device_No = num;
    }
}
