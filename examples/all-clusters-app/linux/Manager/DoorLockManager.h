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

#pragma once

#include <app-common/zap-generated/cluster-objects.h>

class DoorLockManager
{
public:
    static void NewManager(int endpoint, int _device_No);
    static bool SetLockType(int endpoint, int value);
    static bool SetLockState(int endpoint, int value);
    static bool SetActuatorEnabled(int endpoint, bool value);
    static bool SetAutoRelockTime(int endpoint, int value);
    static bool SetOperatingMode(int endpoint, int value);
    static bool SetSupportedOperatingModes(int endpoint, int value);
    static void PostLockStateChanged(chip::EndpointId endpoint, int value);
    void HandleLockStateChanged(int value);
    void setDeviceNo(int num);
private:
    int device_No;
};
