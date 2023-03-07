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
#include "IoTer_logging.h"
#include "IoTer_json.h"

#include <lib/core/CHIPCore.h>
#include <lib/support/CHIPMem.h>
#include <lib/support/CodeUtils.h>
#include <lib/support/DLLUtil.h>
#include <lib/support/logging/CHIPLogging.h>

namespace chip {
namespace IoTer {

#if CHIP_SAMSUNG_UI_LOGGING
static IoTerJson *save = nullptr;
void init(const char * threadVersion, const char * comPort, uint16_t debug, uint16_t device_num){
    save = new IoTerJson(threadVersion, comPort, debug, device_num);
}

DLL_EXPORT void pipe_logging(const char * msg, uint16_t device_num) {
    char cmd[150];
    sprintf(cmd, "echo '%s' > /tmp/chip_pipe_device%d", msg, device_num);
    system(cmd);
}

uint16_t getSamsungThreadDebug() {
    return save->getThreadDebug();
}
uint16_t getSamsungDeviceNumber() {
    return save->getDeviceNumber();
}
const char * getSamsungThreadComport() {
    return save->getThreadComport();
}
const char * getSamsungThreadVersion() {
    return save->getThreadVersion();
}
#endif
} // namespace IoTer
} // namespace chip
