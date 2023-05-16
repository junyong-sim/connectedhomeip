/*
 *
 *    Copyright (c) 2021 Project CHIP Authors
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

#include "Manager/BoolManager.h"
#include "Manager/ColorControlManager.h"
#include "Manager/DoorLockManager.h"
#include "Manager/LevelManager.h"
#include "Manager/OnOffManager.h"
#include "Manager/WindowCoveringManager.h"
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Attributes.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/ConcreteAttributePath.h>
#include <lib/support/logging/CHIPLogging.h>

using namespace chip;
using namespace ::chip::app::Clusters;

void MatterPostAttributeChangeCallback(const app::ConcreteAttributePath & attributePath, uint8_t type, uint16_t size,
                                       uint8_t * value)
{
    EndpointId endpoint     = attributePath.mEndpointId;
    ClusterId clusterId     = attributePath.mClusterId;
    AttributeId attributeId = attributePath.mAttributeId;
    ChipLogProgress(Zcl, "MatterPostAttributeChangeCallback - clusterId: (%d), attributeId: (%d), endpoint: %d",
                    clusterId, attributeId, static_cast<int>(endpoint));
    
    ChipLogProgress(Zcl, "[samsung-all-cluster-app] clusterId:(%d),attributeId:(%d),endpoint:%d", attributePath.mClusterId,
                    attributePath.mAttributeId, static_cast<int>(attributePath.mEndpointId));

    if (clusterId == OnOff::Id && attributeId == OnOff::Attributes::OnOff::Id)
    {
        bool onoff = static_cast<bool>(*value);

        ChipLogProgress(Zcl, "Received on/off command endpoint %d value = %d", static_cast<int>(endpoint), onoff);

        OnOffManager().ReceiveOnOffChanged(endpoint, onoff);
    }
    else if (clusterId == LevelControl::Id && attributeId == LevelControl::Attributes::CurrentLevel::Id)
    {
        uint8_t level = *value;
        ChipLogProgress(Zcl, "Received level command endpoint %d value = %d", static_cast<int>(endpoint), level);

        LevelManager().ReceiveLevelChanged(endpoint, level);
    }
    else if (clusterId == DoorLock::Id && attributeId == DoorLock::Attributes::LockState::Id)
    {
        uint8_t lockState = *value;

        ChipLogProgress(Zcl, "Received lock state command endpoint %d value = %d", static_cast<int>(endpoint), lockState);

        DoorLockManager().ReceiveLockStateChanged(endpoint, lockState);
    }
    else if (clusterId == BooleanState::Id && attributeId == BooleanState::Attributes::StateValue::Id)
    {
        bool boolState = static_cast<bool>(*value);

        ChipLogProgress(Zcl, "Received boolean command endpoint %d value = %d", static_cast<int>(endpoint), boolState);

        BoolManager().ReceiveBoolStateChanged(endpoint, boolState);
    }
    else if (clusterId == ColorControl::Id && attributeId == ColorControl::Attributes::ColorTemperatureMireds::Id)
    {
        uint16_t colorTemp = 0;
        memcpy(&colorTemp, value, size);

        ChipLogProgress(Zcl, "Received ColorTemperature command endpoint %d value = %d", static_cast<int>(endpoint),
                        colorTemp);

        ColorControlManager().ReceiveColorTemperatureChanged(endpoint, colorTemp);
    }
    else if (clusterId == WindowCovering::Id &&
             ((attributeId == WindowCovering::Attributes::TargetPositionTiltPercent100ths::Id) ||
              (attributeId == WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id)))
    {
        chip::Percent100ths pos = 0;
        memcpy(&pos, value, size);

        ChipLogProgress(Zcl, "Received target position changed command endpoint %d value = %d", endpoint, pos);

        WindowCoveringManager().PostTargetPositionChanged(endpoint, pos);
    }
    else if (clusterId == WindowCovering::Id &&
             ((attributeId == WindowCovering::Attributes::CurrentPositionTiltPercent100ths::Id) ||
              (attributeId == WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id)))
    {
        chip::Percent100ths pos = 0;
        memcpy(&pos, value, size);

        ChipLogProgress(Zcl, "Received current position changed command endpoint %d value = %d", endpoint, pos);

        WindowCoveringManager().PostCurrentPositionChanged(endpoint, pos);
    }
    else
    {
        ChipLogProgress(Zcl, "Received unknown command endpoint %d cluster %d attribute = %d", static_cast<int>(endpoint),
                        static_cast<int>(clusterId), static_cast<int>(attributeId));
    }
}
