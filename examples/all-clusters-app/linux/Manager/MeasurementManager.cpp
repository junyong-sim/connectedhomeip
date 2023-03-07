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
#include "MeasurementManager.h"
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/util/af.h>

using namespace chip;

static constexpr size_t measurementManagerSize = EMBER_AF_LEVEL_CONTROL_CLUSTER_SERVER_ENDPOINT_COUNT;

namespace {
    MeasurementManager * measurementManager[measurementManagerSize] = { nullptr };
}

void MeasurementManager::InitManager(int endpoint, int _device_No)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app : MeasurementManager::InitManager");
    uint16_t ep = emberAfFindClusterServerEndpointIndex(static_cast<chip::EndpointId>(endpoint), app::Clusters::TemperatureMeasurement::Id);
    if (!(ep != kEmberInvalidEndpointIndex && ep < EMBER_AF_LEVEL_CONTROL_CLUSTER_SERVER_ENDPOINT_COUNT)) {
        ChipLogError(Zcl, "samsung-all-clusters-app : MeasurementManager::InitManager endpoint %d not found", endpoint);
        return;
    }

    if (measurementManager[ep] != nullptr) {
        ChipLogError(Zcl, "samsung-all-clusters-app : MeasurementManager::InitManager endpoint %d already has a manager", endpoint);
        return;
    }
    MeasurementManager * manager = new MeasurementManager();
    measurementManager[ep] = manager;
    manager->setDeviceNo(_device_No);
}

bool MeasurementManager::SetMeasuredValue(int endpoint, int16_t type, int value)
{
    EmberAfStatus status;
    if (type == 1){
        ChipLogProgress(Zcl, "samsung-all-clusters-app : MeasurementManager::SetMeasuredValue Temp");
        status = chip::app::Clusters::TemperatureMeasurement::Attributes::MeasuredValue::Set(static_cast<chip::EndpointId>(endpoint), static_cast<int16_t>(value));
    }else if (type == 2){
        ChipLogProgress(Zcl, "samsung-all-clusters-app : MeasurementManager::SetMeasuredValue Humid");
        status = chip::app::Clusters::RelativeHumidityMeasurement::Attributes::MeasuredValue::Set(static_cast<chip::EndpointId>(endpoint), static_cast<uint16_t>(value));
    }else if (type == 3){
        ChipLogProgress(Zcl, "samsung-all-clusters-app : MeasurementManager::SetMeasuredValue Illum");
        status = chip::app::Clusters::IlluminanceMeasurement::Attributes::MeasuredValue::Set(static_cast<chip::EndpointId>(endpoint), static_cast<uint16_t>(value));
    }else {
        ChipLogProgress(Zcl, "samsung-all-clusters-app : MeasurementManager::SetMeasuredValue failed.. wrong type!");
        status = EMBER_ZCL_STATUS_FAILURE;
    }
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

void MeasurementManager::setDeviceNo(int num)
{
    device_No = num;
}