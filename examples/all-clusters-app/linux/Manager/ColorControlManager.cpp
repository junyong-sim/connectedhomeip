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
#include <app/clusters/color-control-server/color-control-server.h>
#include <app/util/af.h>

using namespace chip;

static constexpr size_t colorControlManagerSize = EMBER_AF_COLOR_CONTROL_CLUSTER_SERVER_ENDPOINT_COUNT;

namespace {
    ColorControlManager * colorControlManager[colorControlManagerSize] = { nullptr };
}

void ColorControlManager::InitManager(int endpoint, int _device_No)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::InitManager");
    uint16_t ep = emberAfFindClusterServerEndpointIndex(static_cast<chip::EndpointId>(endpoint), app::Clusters::ColorControl::Id);
    if (!(ep != kEmberInvalidEndpointIndex && ep < EMBER_AF_COLOR_CONTROL_CLUSTER_SERVER_ENDPOINT_COUNT)) {
        ChipLogError(Zcl, "samsung-all-clusters-app::ColorControl::InitManager: endpoint %d not found", endpoint);
        return;
    }
    if (colorControlManager[ep] != nullptr) {
        ChipLogError(Zcl, "samsung-all-clusters-app::ColorControl::InitManager: endpoint %d already has a manager", endpoint);
        return;
    }
    ColorControlManager * mgr = new ColorControlManager();
    colorControlManager[ep] = mgr;
    mgr->setDeviceNo(_device_No);
}

void ColorControlManager::setStartUpColorTemperature(int endpoint)
{
    ColorControlServer::Instance().startUpColorTempCommand(static_cast<chip::EndpointId>(endpoint));
}

static ColorControlManager * GetColorControlManager(EndpointId endpoint)
{
    uint16_t ep = emberAfFindClusterServerEndpointIndex(endpoint, app::Clusters::ColorControl::Id);
    if (ep == kEmberInvalidEndpointIndex || ep >= EMBER_AF_COLOR_CONTROL_CLUSTER_SERVER_ENDPOINT_COUNT)
        return nullptr;
    else
        return colorControlManager[ep];
}

void ColorControlManager::ReceiveEnhancedColorModeChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::ReceiveEnhancedColorModeChanged");
    ColorControlManager * mgr = GetColorControlManager(endpoint);
    if (mgr == nullptr) {
        ChipLogError(Zcl, "ColorControlManager null");
        return;
    }
    mgr->SetEnhancedColorModeChanged(value);
}

void ColorControlManager::ReceiveColorModeChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::ReceiveColorModeChanged");
    ColorControlManager * mgr = GetColorControlManager(endpoint);
    if (mgr == nullptr) {
        ChipLogError(Zcl, "ColorControlManager null");
        return;
    }
    mgr->SetColorModeChanged(value);
}

void ColorControlManager::ReceiveCurrentHueChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::ReceiveCurrentHueChanged");
    ColorControlManager * mgr = GetColorControlManager(endpoint);
    if (mgr == nullptr) {
        ChipLogError(Zcl, "ColorControlManager null");
        return;
    }
    mgr->SetCurrentHueChanged(value);
}

void ColorControlManager::ReceiveCurrentSaturationChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::ReceiveCurrentSaturationChanged");
    ColorControlManager * mgr = GetColorControlManager(endpoint);
    if (mgr == nullptr) {
        ChipLogError(Zcl, "ColorControlManager null");
        return;
    }
    mgr->SetCurrentSaturationChanged(value);
}

void ColorControlManager::ReceiveColorTemperatureChanged(chip::EndpointId endpoint, uint16_t value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: ColorControlManager::ReceiveColorTemperatureChanged");
    ColorControlManager * mgr = GetColorControlManager(endpoint);
    if (mgr == nullptr) {
        ChipLogError(Zcl, "ColorControlManager null");
        return;
    }
    mgr->SetColorTemperatureChanged(value);
}

void ColorControlManager::SetEnhancedColorModeChanged(int value)
{
    ChipLogProgress(Zcl, "ColorControlManager::SetEnhancedColorModeChanged");
    //TODO
}

void ColorControlManager::SetColorModeChanged(int value)
{
    ChipLogProgress(Zcl, "ColorControlManager::SetColorModeChanged");
    //TODO
}

void ColorControlManager::SetCurrentHueChanged(int value)
{
    ChipLogProgress(Zcl, "ColorControlManager::SetCurrentHueChanged");
    //TODO
}

void ColorControlManager::SetCurrentSaturationChanged(int value)
{
    ChipLogProgress(Zcl, "ColorControlManager::SetCurrentSaturationChanged");
    //TODO 
}

void ColorControlManager::SetColorTemperatureChanged(uint16_t value)
{
    ChipLogProgress(Zcl, "ColorControlManager::SetColorTemperatureChanged to %d", value);
    char cmd[100];
    if (device_No < 0 || device_No > 9)
        device_No = 0;
    sprintf(cmd, "echo 'colortemp:%d' > /tmp/chip_pipe_device%d", value, device_No);
    system(cmd);
}

bool ColorControlManager::SetColorTemperature(int endpoint, uint16_t value)
{
    EmberAfStatus status;
    if (value < 1 || value > 65279)
        status = EMBER_ZCL_STATUS_FAILURE;
    else
        status = chip::app::Clusters::ColorControl::Attributes::ColorTemperature::Set(static_cast<chip::EndpointId>(endpoint), value);
    return status;
}

void ColorControlManager::setDeviceNo(int num)
{
    device_No = num;
}