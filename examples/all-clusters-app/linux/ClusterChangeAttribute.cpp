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
#include "Manager/DoorLockManager.h"
#include "Manager/LevelManager.h"
#include "Manager/OnOffManager.h"
//#include "WindowCoveringManager.h"
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
    ChipLogProgress(Zcl, "[samsung-all-cluster-app] clusterId:(%d),attributeId:(%d),endpoint:%d", attributePath.mClusterId,
                    attributePath.mAttributeId, static_cast<int>(attributePath.mEndpointId));

    if (attributePath.mClusterId == OnOff::Id && attributePath.mAttributeId == OnOff::Attributes::OnOff::Id)
    {
        bool onoff = static_cast<bool>(*value);

        ChipLogProgress(Zcl, "Received on/off command endpoint %d value = %d", static_cast<int>(attributePath.mEndpointId), onoff);

        OnOffManager().PostOnOffChanged(attributePath.mEndpointId, onoff);
    }
    else if (attributePath.mClusterId == LevelControl::Id &&
             attributePath.mAttributeId == LevelControl::Attributes::CurrentLevel::Id)
    {
        uint8_t level = *value;
        ChipLogProgress(Zcl, "Received level command endpoint %d value = %d", static_cast<int>(attributePath.mEndpointId), level);

        LevelManager().PostLevelChanged(attributePath.mEndpointId, level);
    }
    else if (attributePath.mClusterId == DoorLock::Id && attributePath.mAttributeId == DoorLock::Attributes::LockState::Id)
    {
        uint8_t lockState = *value;

        ChipLogProgress(Zcl, "Received lock state command endpoint %d value = %d", static_cast<int>(attributePath.mEndpointId),
                        lockState);

        DoorLockManager().PostLockStateChanged(attributePath.mEndpointId, lockState);
    }
    else if (attributePath.mClusterId == BooleanState::Id && attributePath.mAttributeId == BooleanState::Attributes::StateValue::Id)
    {
        bool boolState = static_cast<bool>(*value);

        ChipLogProgress(Zcl, "Received boolean command endpoint %d value = %d", static_cast<int>(attributePath.mEndpointId),
                        boolState);

        BoolManager().PostBoolStateChanged(attributePath.mEndpointId, boolState);
    }
    /*else if (attributePath.mClusterId == WindowCovering::Id && (attributePath.mAttributeId ==
    WindowCovering::Attributes::TargetPositionLiftPercent100ths::Id || attributePath.mAttributeId ==
    WindowCovering::Attributes::TargetPositionTiltPercent100ths::Id))
    {
        uint32_t targetPosition = 0;
        memcpy(&targetPosition, value, size);

        ChipLogProgress(Zcl, "Received target position command endpoint %d value = %d", static_cast<int>(attributePath.mEndpointId),
    targetPosition);

        WindowCoveringManager().PostTargetPositionChanged(attributePath.mEndpointId, static_cast<int>(targetPosition));
    }
    else if (attributePath.mClusterId == WindowCovering::Id && (attributePath.mAttributeId ==
    WindowCovering::Attributes::CurrentPositionLiftPercent100ths::Id || attributePath.mAttributeId ==
    WindowCovering::Attributes::CurrentPositionTiltPercent100ths::Id))
    {
        uint32_t currentPosition = 0;
        memcpy(&currentPosition, value, size);

        ChipLogProgress(Zcl, "Received current position command endpoint %d value = %d",
    static_cast<int>(attributePath.mEndpointId), currentPosition);

        WindowCoveringManager().PostCurrentPositionChanged(attributePath.mEndpointId, static_cast<int>(currentPosition));
    }
    else if (attributePath.mClusterId == BooleanState::Id) {
        uint8_t booleanState = *value;

        ChipLogProgress(Zcl, "Received current boolean state command endpoint %d attribute = %d value = %d",
    static_cast<int>(attributePath.mEndpointId), attributePath.mAttributeId, booleanState);
    }*/
}
