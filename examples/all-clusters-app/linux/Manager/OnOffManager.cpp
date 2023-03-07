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
#include "OnOffManager.h"
#include <app/clusters/on-off-server/on-off-server.h>
#include <app-common/zap-generated/attributes/Accessors.h>
#include <app-common/zap-generated/ids/Clusters.h>
#include <app/util/af.h>

using namespace chip;

static constexpr size_t kOnffManagerTableSize = EMBER_AF_ON_OFF_CLUSTER_SERVER_ENDPOINT_COUNT;

namespace {

OnOffManager * gOnOffManagerTable[kOnffManagerTableSize] = { nullptr };

}

void emberAfOnOffClusterInitCallback(EndpointId endpoint)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app::OnOff::PostClusterInit");
}

void OnOffManager::NewManager(int endpoint, int _device_No)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: OnOffManager::NewManager");
    uint16_t ep = emberAfFindClusterServerEndpointIndex(static_cast<chip::EndpointId>(endpoint), app::Clusters::OnOff::Id);
    VerifyOrReturn(ep != kEmberInvalidEndpointIndex && ep < EMBER_AF_ON_OFF_CLUSTER_SERVER_ENDPOINT_COUNT,
                   ChipLogError(Zcl, "samsung-all-clusters-app::OnOff::NewManager: endpoint %d not found", endpoint));

    VerifyOrReturn(gOnOffManagerTable[ep] == nullptr,
                   ChipLogError(Zcl, "samsung-all-clusters-app::OnOff::NewManager: endpoint %d already has a manager", endpoint));
    OnOffManager * mgr = new OnOffManager();
    gOnOffManagerTable[ep] = mgr;
    mgr->setDeviceNo(_device_No);
}

OnOffManager * GetOnOffManager(EndpointId endpoint)
{
    uint16_t ep = emberAfFindClusterServerEndpointIndex(endpoint, app::Clusters::OnOff::Id);
    return ((ep == kEmberInvalidEndpointIndex || ep >= EMBER_AF_ON_OFF_CLUSTER_SERVER_ENDPOINT_COUNT)
                ? nullptr : gOnOffManagerTable[ep]);
}

OnOffManager * OnOffManager::GetOnOffManagerForAbroad(chip::EndpointId endpoint) {
    return GetOnOffManager(endpoint);
}

void OnOffManager::PostOnOffChanged(chip::EndpointId endpoint, bool value)
{
    ChipLogProgress(Zcl, "samsung-all-clusters-app: OnOffManager::PostOnOffChange");
    OnOffManager * mgr = GetOnOffManager(endpoint);
    VerifyOrReturn(mgr != nullptr, ChipLogError(Zcl, "OnOffManager null"));

    if (endpoint == 1)
    {
        mgr->HandleOnOffChanged(value);
    }
}

bool OnOffManager::SetOnOff(int endpoint, bool value)
{
    EmberAfStatus status = OnOffServer::Instance().setOnOffValue(static_cast<chip::EndpointId>(endpoint), value, false);
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

bool OnOffManager::GetOnOff(int endpoint, bool *value)
{
    EmberAfStatus status = OnOffServer::Instance().getOnOffValue(static_cast<chip::EndpointId>(endpoint), value);
    return status == EMBER_ZCL_STATUS_SUCCESS;
}

void OnOffManager::HandleOnOffChanged(bool value)
{
    uint8_t onoff = (value == true) ? 1 : 0;
    char cmd[100];
    if (device_No < 0)
        device_No = 0;
    if (device_No > 9)
        device_No = 0;
    sprintf(cmd, "echo 'onoff:%d' > /tmp/chip_pipe_device%d", onoff, device_No);
    system(cmd);
}

void OnOffManager::setDeviceNo(int num)
{
    device_No = num;
}