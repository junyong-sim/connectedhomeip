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
#include "LevelManager.h"
#include "OnOffManager.h"
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/util/af.h>

using namespace chip;

static constexpr size_t kLevelManagerTableSize = EMBER_AF_LEVEL_CONTROL_CLUSTER_SERVER_ENDPOINT_COUNT;

namespace {

LevelManager * gLevelManagerTable[kLevelManagerTableSize] = { nullptr };

}

/*void emberAfLevelControlClusterInitCallback(EndpointId endpoint)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::Level::PostClusterInit");
}*/

void LevelManager::NewManager(int endpoint, int _device_No)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: LevelManager::NewManager");
    uint16_t ep = emberAfFindClusterServerEndpointIndex(static_cast<chip::EndpointId>(endpoint), app::Clusters::LevelControl::Id);
    VerifyOrReturn(ep != kEmberInvalidEndpointIndex && ep < kLevelManagerTableSize,
                   ChipLogError(Zcl, "samsung-all-clusters-app::Level::NewManager: endpoint %d not found", endpoint));

    VerifyOrReturn(gLevelManagerTable[ep] == nullptr,
                   ChipLogError(Zcl, "samsung-all-clusters-app::Level::NewManager: endpoint %d already has a manager", endpoint));
    LevelManager * mgr = new LevelManager();
    gLevelManagerTable[ep] = mgr;
    mgr->setDeviceNo(_device_No);
}

LevelManager * GetLevelManager(EndpointId endpoint)
{
    uint16_t ep = emberAfFindClusterServerEndpointIndex(endpoint, app::Clusters::LevelControl::Id);
    return ((ep == kEmberInvalidEndpointIndex || ep >= kLevelManagerTableSize) ? nullptr : gLevelManagerTable[ep]);
}

void LevelManager::PostLevelChanged(chip::EndpointId endpoint, uint8_t value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: LevelManager::PostLevelChanged");
    LevelManager * mgr = GetLevelManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "LevelManager null"));

    mgr->HandleLevelChanged(value);
}

bool LevelManager::SetLevel(int endpoint, int value)
{
    bool onoffState = false;
    OnOffManager * onoffMgr = OnOffManager::GetOnOffManagerForAbroad(static_cast<chip::EndpointId>(endpoint));
    
    EmberAfStatus status = app::Clusters::LevelControl::Attributes::CurrentLevel::Set(static_cast<chip::EndpointId>(endpoint),
                                                                                      static_cast<uint8_t>(value));
    
    onoffMgr->GetOnOff(endpoint, &onoffState);
    if (value == 0 && onoffState == true) {
        app::Clusters::OnOff::Attributes::OnOff::Set(static_cast<chip::EndpointId>(endpoint), false);
    } else if (value > 0 && onoffState == false) {
        app::Clusters::OnOff::Attributes::OnOff::Set(static_cast<chip::EndpointId>(endpoint), true);
    }
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

void LevelManager::HandleLevelChanged(uint8_t value)
{
    char cmd[100];
    if (device_No < 0)
        device_No = 0;
    if (device_No > 9)
        device_No = 0;

    sprintf(cmd, "echo 'level:%d' > /tmp/chip_pipe_device%d", value, device_No);
    system(cmd);
}

void LevelManager::setDeviceNo(int num)
{
    device_No = num;
}