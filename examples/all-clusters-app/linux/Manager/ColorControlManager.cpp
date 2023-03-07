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
#include "ColorControlManager.h"
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/util/af.h>

using namespace chip;

static constexpr size_t kColorControlManagerTableSize = EMBER_AF_COLOR_CONTROL_CLUSTER_SERVER_ENDPOINT_COUNT;

namespace {

ColorControlManager * gColorControlManagerTable[kColorControlManagerTableSize] = { nullptr };

}

void emberAfColorControlClusterInitCallback(EndpointId endpoint)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::ColorControl::PostClusterInit");
}

void ColorControlManager::NewManager(int endpoint, int _device_No)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::NewManager");
    uint16_t ep = emberAfFindClusterServerEndpointIndex(static_cast<chip::EndpointId>(endpoint), app::Clusters::ColorControl::Id);
    VerifyOrReturn(ep != kEmberInvalidEndpointIndex && ep < EMBER_AF_COLOR_CONTROL_CLUSTER_SERVER_ENDPOINT_COUNT,
                   ChipLogError(Zcl, "samsung-all-clusters-app::ColorControl::NewManager: endpoint %d not found", endpoint));

    VerifyOrReturn(gColorControlManagerTable[ep] == nullptr,
                   ChipLogError(Zcl, "samsung-all-clusters-app::ColorControl::NewManager: endpoint %d already has a manager", endpoint));
    ColorControlManager * mgr = new ColorControlManager();
    gColorControlManagerTable[ep] = mgr;
    mgr->setDeviceNo(_device_No);
}

static ColorControlManager * GetColorControlManager(EndpointId endpoint)
{
    uint16_t ep = emberAfFindClusterServerEndpointIndex(endpoint, app::Clusters::ColorControl::Id);
    return ((ep == kEmberInvalidEndpointIndex || ep >= EMBER_AF_COLOR_CONTROL_CLUSTER_SERVER_ENDPOINT_COUNT)
                ? nullptr : gColorControlManagerTable[ep]);
}

void ColorControlManager::PostEnhancedColorModeChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::PostEnhancedColorModeChanged");
    ColorControlManager * mgr = GetColorControlManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "ColorControlManager null"));

    mgr->HandleEnhancedColorModeChanged(value);
}

void ColorControlManager::PostColorModeChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::PostColorModeChanged");
    ColorControlManager * mgr = GetColorControlManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "ColorControlManager null"));

    mgr->HandleColorModeChanged(value);
}

void ColorControlManager::PostCurrentHueChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::PostCurrentHueChanged");
    ColorControlManager * mgr = GetColorControlManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "ColorControlManager null"));

    mgr->HandleCurrentHueChanged(value);
}

void ColorControlManager::PostCurrentSaturationChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::PostCurrentSaturationChanged");
    ColorControlManager * mgr = GetColorControlManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "ColorControlManager null"));

    mgr->HandleCurrentSaturationChanged(value);
}

void ColorControlManager::PostColorTemperatureChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::PostColorTemperatureChanged");
    ColorControlManager * mgr = GetColorControlManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "ColorControlManager null"));

    mgr->HandleColorTemperatureChanged(value);
}

void ColorControlManager::HandleEnhancedColorModeChanged(int value)
{
    ChipLogProgress(Zcl, "ColorControlManager::HandleEnhancedColorModeChanged");

    
}

void ColorControlManager::HandleColorModeChanged(int value)
{
    ChipLogProgress(Zcl, "ColorControlManager::HandleColorModeChanged");

}

void ColorControlManager::HandleCurrentHueChanged(int value)
{
    ChipLogProgress(Zcl, "ColorControlManager::HandleCurrentHueChanged");


}

void ColorControlManager::HandleCurrentSaturationChanged(int value)
{
    ChipLogProgress(Zcl, "ColorControlManager::HandleCurrentSaturationChanged");

    
}

void ColorControlManager::HandleColorTemperatureChanged(int value)
{
    ChipLogProgress(Zcl, "ColorControlManager::HandleColorTemperatureChanged");

}

void ColorControlManager::setDeviceNo(int num)
{
    device_No = num;
}