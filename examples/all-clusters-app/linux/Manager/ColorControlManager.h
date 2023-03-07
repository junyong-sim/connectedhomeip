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
#include <cstdint>

class ColorControlManager
{
public:
    static void InitManager(int endpoint, int _device_No);
    static void ReceiveEnhancedColorModeChanged(chip::EndpointId endpoint, int enhancedColorMode);
    static void ReceiveColorModeChanged(chip::EndpointId endpoint, int colorMode);
    static void ReceiveCurrentHueChanged(chip::EndpointId endpoint, int currentHue);
    static void ReceiveCurrentSaturationChanged(chip::EndpointId endpoint, int currentSaturation);
    static void ReceiveColorTemperatureChanged(chip::EndpointId endpoint, uint16_t colorTemperature);
    static bool SetColorTemperature(int endpoint, uint16_t value);
    void SetEnhancedColorModeChanged(int value);
    void SetColorModeChanged(int value);
    void SetCurrentHueChanged(int value);
    void SetCurrentSaturationChanged(int value);
    void SetColorTemperatureChanged(uint16_t value);
    void setDeviceNo(int num);
    void setStartUpColorTemperature(int endpoint);
private:
    int device_No;
};