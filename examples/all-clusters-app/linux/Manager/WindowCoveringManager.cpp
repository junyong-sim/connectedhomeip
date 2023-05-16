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
#include "WindowCoveringManager.h"
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/clusters/window-covering-server/window-covering-server.h>
#include <app/util/af.h>
#include <lib/support/CHIPJNIError.h>

using namespace chip;
using namespace chip::app::Clusters::WindowCovering;

static constexpr size_t kWindowCoveringManagerTableSize   = EMBER_AF_WINDOW_COVERING_CLUSTER_SERVER_ENDPOINT_COUNT;
const std::string WindowCoveringManager::TARGET_POSITION  = "target_position";
const std::string WindowCoveringManager::CURRENT_POSITION = "current_position";

namespace {

WindowCoveringManager * gWindowCoveringManagerTable[kWindowCoveringManagerTableSize] = { nullptr };

}

OperationalState computeOperationalState(chip::Percent100ths target, chip::Percent100ths current)
{
    OperationalState opState = OperationalState::Stall;

    if (current != target)
    {
        opState = (current > target) ? OperationalState::MovingDownOrClose : OperationalState::MovingUpOrOpen;
    }
    return opState;
}

void emberAfWindowCoveringClusterInitCallback(EndpointId endpoint)
{
    ChipLogProgress(Zcl, "emberAfWindowCoveringClusterInitCallback");
}

void WindowCoveringManager::init(EndpointId endpoint)
{
    chip::BitMask<Mode> mode;

    SetFeatureMap(endpoint, chip::to_underlying(WindowCoveringFeature::kPositionAwareLift) | chip::to_underlying(WindowCoveringFeature::kLift));
    SetType(endpoint, Type::kRollerShade);
    SetEndProductType(endpoint, EndProductType::kRollerShade);
    SetMode(endpoint, mode);
    ModePrint(mode);
    SetConfigStatus(endpoint, ConfigStatus::kOperational);
    OperationalStateSet(endpoint, OperationalStatus::kLift, OperationalState::Stall);
}

void WindowCoveringManager::NewManager(EndpointId endpoint, int dnum)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: WindowCoveringManager::NewManager");
    uint16_t ep = emberAfFindClusterServerEndpointIndex(static_cast<chip::EndpointId>(endpoint), app::Clusters::WindowCovering::Id);
    VerifyOrReturn(ep != kEmberInvalidEndpointIndex && ep < EMBER_AF_WINDOW_COVERING_CLUSTER_SERVER_ENDPOINT_COUNT,
                   ChipLogError(Zcl, "samsung-all-clusters-app:WindowCovering::NewManager: endpoint %d not found", endpoint));

    VerifyOrReturn(
        gWindowCoveringManagerTable[ep] == nullptr,
        ChipLogError(Zcl, "samsung-all-clusters-app::WindowCovering::NewManager: endpoint %d already has a manager", endpoint));

    WindowCoveringManager * mgr     = new WindowCoveringManager();
    gWindowCoveringManagerTable[ep] = mgr;
    mgr->SetDeviceNo(dnum);
    mgr->init(endpoint);
}

WindowCoveringManager * GetWindowCoveringManager(EndpointId endpoint)
{
    uint16_t ep = emberAfFindClusterServerEndpointIndex(endpoint, app::Clusters::WindowCovering::Id);
    return ((ep == kEmberInvalidEndpointIndex || ep >= EMBER_AF_WINDOW_COVERING_CLUSTER_SERVER_ENDPOINT_COUNT)
                ? nullptr
                : gWindowCoveringManagerTable[ep]);
}

bool WindowCoveringManager::SetFeatureMap(EndpointId endpoint, uint32_t value)
{
    EmberAfStatus status = app::Clusters::WindowCovering::Attributes::FeatureMap::Set(endpoint, value);
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool WindowCoveringManager::SetType(EndpointId endpoint, Type type)
{
    /*
        kRollerShade               = 0x00,
        kRollerShade2Motor         = 0x01,
        kRollerShadeExterior       = 0x02,
        kRollerShadeExterior2Motor = 0x03,
        kDrapery                   = 0x04,
        kAwning                    = 0x05,
        kShutter                   = 0x06,
        kTiltBlindTiltOnly         = 0x07,
        kTiltBlindLiftAndTilt      = 0x08,
        kProjectorScreen           = 0x09,
        kUnknown                   = 0xFF,
        kUnknownEnumValue          = 10,
    */
    EmberAfStatus status = app::Clusters::WindowCovering::Attributes::Type::Set(static_cast<chip::EndpointId>(endpoint), type);
    if (status == EMBER_ZCL_STATUS_SUCCESS)
    {
        WindowCoveringManager * mgr = GetWindowCoveringManager(endpoint);
        if (mgr != nullptr)
        {
            mgr->mType = type;
            return true;
        }
    }

    return false;
}

bool WindowCoveringManager::SetCurrentPosition(chip::EndpointId endpoint, chip::Percent100ths current)
{
    NPercent percent;
    percent.SetNonNull(static_cast<uint8_t>(current / 100));
    NPercent100ths target;
    EmberAfStatus status1, status2;
    OperationalState opTilt, opLift;
    ChipLogError(NotSpecified, "Current value: %d", current);

    if (HasFeaturePaTilt(endpoint))
    {
        app::Clusters::WindowCovering::Attributes::TargetPositionTiltPercent100ths::Get(endpoint, target);
        opTilt = computeOperationalState(target.Value(), current);
        OperationalStateSet(endpoint, OperationalStatus::kTilt, opTilt);

        status1 = app::Clusters::WindowCovering::Attributes::CurrentPositionTiltPercentage::Set(endpoint, percent);
        status2 = app::Clusters::WindowCovering::Attributes::CurrentPositionTiltPercent100ths::Set(endpoint, current);
    }
    if (HasFeaturePaLift(endpoint))
    {
        app::Clusters::WindowCovering::Attributes::TargetPositionTiltPercent100ths::Get(endpoint, target);
        opLift = computeOperationalState(target.Value(), current);
        OperationalStateSet(endpoint, OperationalStatus::kLift, opLift);

        status1 = app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercentage::Set(endpoint, percent);
        status2 = app::Clusters::WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Set(endpoint, current);
    }
    return (status1 == EMBER_ZCL_STATUS_SUCCESS && status2 == EMBER_ZCL_STATUS_SUCCESS);
}

bool WindowCoveringManager::SetTargetPosition(chip::EndpointId endpoint, chip::Percent100ths target)
{
    EmberAfStatus status;
    NPercent100ths current;
    OperationalState opTilt, opLift;
    ChipLogError(NotSpecified, "Target value: %d", target);

    if (HasFeature(endpoint, WindowCoveringFeature::kTilt))
    {
        app::Clusters::WindowCovering::Attributes::CurrentPositionTiltPercent100ths::Get(endpoint, current);
        opTilt = computeOperationalState(target, current.Value());
        OperationalStateSet(endpoint, OperationalStatus::kTilt, opTilt);

        status = app::Clusters::WindowCovering::Attributes::TargetPositionTiltPercent100ths::Set(endpoint, target);
    }
    if (HasFeature(endpoint, WindowCoveringFeature::kLift))
    {
        app::Clusters::WindowCovering::Attributes::CurrentPositionTiltPercent100ths::Get(endpoint, current);
        opLift = computeOperationalState(target, current.Value());
        OperationalStateSet(endpoint, OperationalStatus::kLift, opLift);

        status = app::Clusters::WindowCovering::Attributes::TargetPositionLiftPercent100ths::Set(endpoint, target);
    }
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool WindowCoveringManager::SetConfigStatus(EndpointId endpoint, ConfigStatus value)
{
    EmberAfStatus status =
        app::Clusters::WindowCovering::Attributes::ConfigStatus::Set(static_cast<chip::EndpointId>(endpoint), value);
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool WindowCoveringManager::SetOperationalStatus(EndpointId endpoint, OperationalStatus value)
{
    EmberAfStatus status = app::Clusters::WindowCovering::Attributes::OperationalStatus::Set(endpoint, value);
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool WindowCoveringManager::SetEndProductType(EndpointId endpoint, EndProductType value)
{
    EmberAfStatus status =
        app::Clusters::WindowCovering::Attributes::EndProductType::Set(static_cast<chip::EndpointId>(endpoint), value);
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool WindowCoveringManager::SetMode(EndpointId endpoint, Mode value)
{
    EmberAfStatus status = app::Clusters::WindowCovering::Attributes::Mode::Set(static_cast<chip::EndpointId>(endpoint), value);
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

void WindowCoveringManager::PostTargetPositionChanged(chip::EndpointId endpoint, chip::Percent100ths value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: WindowCoveringManager::PostTargetPositionChanged");
    WindowCoveringManager * mgr = GetWindowCoveringManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "WindowCoveringManager null"));

    mgr->HandleTargetPositionChanged(value);
}

void WindowCoveringManager::PostCurrentPositionChanged(chip::EndpointId endpoint, chip::Percent100ths value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: WindowCoveringManager::PostCurrentPositionChanged");
    WindowCoveringManager * mgr = GetWindowCoveringManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "WindowCoveringManager null"));

    mgr->HandleCurrentPositionChanged(value);
}

void WindowCoveringManager::HandleTargetPositionChanged(chip::Percent100ths value)
{
    char cmd[100];

    ChipLogProgress(Zcl, "WindowCoveringManager::HandleTargetPositionChanged");
    sprintf(cmd, "echo 'go-to-percentage:%d' > /tmp/chip_pipe_device%d", value, dev_num);
    system(cmd);
}

void WindowCoveringManager::HandleCurrentPositionChanged(chip::Percent100ths value)
{
    char cmd[100];

    ChipLogProgress(Zcl, "WindowCoveringManager::HandleCurrentPositionChanged");
    sprintf(cmd, "echo 'current-percentage:%d' > /tmp/chip_pipe_device%d", value, dev_num);
    system(cmd);
}

void WindowCoveringManager::PostBatPercentRemainingChanged(chip::EndpointId endpoint, int value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::GetWindowCoveringManager::PostBatPercentRemainingChanged");
    WindowCoveringManager * mgr = GetWindowCoveringManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "GetWindowCoveringManager null"));

    mgr->HandleBatPercentRemainingChanged(value);
}

void WindowCoveringManager::HandleBatPercentRemainingChanged(int value)
{
    char cmd[100];

    sprintf(cmd, "echo 'perc:%d' > /tmp/chip_pipe_device%d", value, dev_num);
    system(cmd);
}

void WindowCoveringManager::SetDeviceNo(int num)
{
    if (num < 0 || num > 9)
    {
        num = 0;
    }
    dev_num = num;
}
