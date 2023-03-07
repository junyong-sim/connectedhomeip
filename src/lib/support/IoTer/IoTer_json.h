/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
 *    Copyright (c) 2013-2017 Nest Labs, Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, elib/support/IoTer/storage.hither express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */
#pragma once
#include <fstream>
#include <iostream>
#include <lib/core/CHIPCore.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/DLLUtil.h>
#include <lib/support/logging/CHIPLogging.h>
#include <cstring>
#include "json/json.h"

//namespace chip {
//namespace IoTer {

class DLL_EXPORT IoTerJson {
    public:
        IoTerJson (const char * version, const char * comPort, uint16_t debug, uint16_t number) {
            //sprintf(path, "/tmp/chip_device%d_config.json", number);
            ChipLogProgress(NotSpecified, "[CHIP-SAMSUNG] IoTerJson class creation");
            //ChipLogProgress(NotSpecified, "[CHIP-SAMSUNG] Path : %s", path);
            ChipLogProgress(NotSpecified, "[CHIP-SAMSUNG] Device number : %d", number);
            ChipLogProgress(NotSpecified, "[CHIP-SAMSUNG] Thread Version : %s", version);
            ChipLogProgress(NotSpecified, "[CHIP-SAMSUNG] Thread Com-port : %s", comPort);
            ChipLogProgress(NotSpecified, "[CHIP-SAMSUNG] Thread debug level : %d", debug);

            root["number"] = number;
            root["debug"] = debug;
            root["version"] = version;
            root["comport"] = comPort;
            /*Json::StyledWriter writer;
            std::ofstream output(path, std::ios::out);
            output << writer.write(root);
            output.close();*/
        }
        uint16_t getDeviceNumber();
        uint16_t getThreadDebug();
        const char * getThreadVersion();
        const char * getThreadComport();

        char path[100];
        Json::Value root;  
};

uint16_t IoTerJson::getThreadDebug() {
    uint16_t res = static_cast<uint16_t>(root["debug"].asUInt());
    return res;
}
const char * IoTerJson::getThreadVersion() {
    return root["version"].asCString();
}
const char * IoTerJson::getThreadComport() {
    return root["comport"].asCString();
}
uint16_t IoTerJson::getDeviceNumber() {
    uint16_t res = static_cast<uint16_t>(root["number"].asUInt());
    return res;
}

//} // namespace IoTer
//} // namespace chip