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

class ColorControlManager
{
public:
    static void NewManager(int endpoint, int _device_No);
    static void PostEnhancedColorModeChanged(chip::EndpointId endpoint, int enhancedColorMode);
    static void PostColorModeChanged(chip::EndpointId endpoint, int colorMode);
    static void PostCurrentHueChanged(chip::EndpointId endpoint, int currentHue);
    static void PostCurrentSaturationChanged(chip::EndpointId endpoint, int currentSaturation);
    static void PostColorTemperatureChanged(chip::EndpointId endpoint, int colorTemperature);
    void HandleEnhancedColorModeChanged(int value);
    void HandleColorModeChanged(int value);
    void HandleCurrentHueChanged(int value);
    void HandleCurrentSaturationChanged(int value);
    void HandleColorTemperatureChanged(int value);
    void setDeviceNo(int num);
private:
    int device_No;
};