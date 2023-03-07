/*
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
#pragma once

#include <app-common/zap-generated/cluster-objects.h>

class WindowCoveringManager
{
public:
    static const std::string TARGET_POSITION;
    static const std::string CURRENT_POSITION;

    static void NewManager(chip::EndpointId endpoint, int dnum);
    static bool SetFeatureMap(chip::EndpointId endpoint, uint32_t value);
    static bool SetType(chip::EndpointId endpoint, chip::app::Clusters::WindowCovering::Type type);
    static bool SetCurrentPosition(chip::EndpointId endpoint, chip::Percent100ths value);
    static bool SetTargetPosition(chip::EndpointId endpoint, chip::Percent100ths value);
    static bool SetConfigStatus(chip::EndpointId endpoint, chip::app::Clusters::WindowCovering::ConfigStatus value);
    static bool SetOperationalStatus(chip::EndpointId endpoint, chip::app::Clusters::WindowCovering::OperationalStatus value);
    static bool SetEndProductType(chip::EndpointId endpoint, chip::app::Clusters::WindowCovering::EndProductType value);
    static bool SetMode(chip::EndpointId endpoint, chip::app::Clusters::WindowCovering::Mode value);
    static void PostTargetPositionChanged(chip::EndpointId endpoint, chip::Percent100ths value);
    static void PostCurrentPositionChanged(chip::EndpointId endpoint, chip::Percent100ths value);
    static void PostBatPercentRemainingChanged(chip::EndpointId endpoint, int value);

    void HandleTargetPositionChanged(chip::Percent100ths value);
    void HandleCurrentPositionChanged(chip::Percent100ths value);
    void HandleBatPercentRemainingChanged(int value);
    void SetDeviceNo(int num);
    void init(chip::EndpointId endpoint);

private:
    chip::app::Clusters::WindowCovering::Type mType; // The type of window covering
    int dev_num;
};
