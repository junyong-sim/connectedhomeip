/*
 *
 *    Copyright (c) 2020 Project CHIP Authors
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

#include <platform/internal/CHIPDeviceLayerInternal.h>

#ifdef SUPPORT_MULTIPLE_OT_LIBS
#include <platform/OpenThread/GenericThreadStackManagerImpl_OpenThread_DL.cpp>
#include <platform/OpenThread/OpenThreadDL_Manager.h>
#else
#include <platform/OpenThread/GenericThreadStackManagerImpl_OpenThread.cpp>
#endif
#if CHIP_SAMSUNG_UI_LOGGING
#include <lib/support/IoTer/IoTer_logging.h>
#endif

#include <inet/UDPEndPointImpl.h>
#include <lib/support/CodeUtils.h>
#include <platform/OpenThread/OpenThreadUtils.h>
#include <platform/ThreadStackManager.h>

extern "C" void otGetInstance(otInstance ** instance, pthread_t * instanceId);
extern "C" void otDestroyInstance();
extern "C" void otLock();
extern "C" void otUnlock();



namespace chip {
namespace DeviceLayer {

using namespace ::chip::DeviceLayer::Internal;
using namespace ::chip::Inet;

ThreadStackManagerImpl ThreadStackManagerImpl::sInstance;

#ifndef SUPPORT_MULTIPLE_OT_LIBS
CHIP_ERROR ThreadStackManagerImpl::_InitThreadStack()
{
    ChipLogProgress(DeviceLayer, "JYSIM ThreadStackManagerImpl::_InitThreadStack");

    if (instance == NULL)
    {
        otGetInstance(&instance, &threadId);
        ReturnErrorOnFailure(GenericThreadStackManagerImpl_OpenThread<ThreadStackManagerImpl>::DoInit(instance));
        ChipLogProgress(DeviceLayer, "Thread stack manager initialized..gthreadId[%ld]", threadId);
    }
    else
    {
        ChipLogProgress(DeviceLayer, "Thread stack manager already initialized");
    }

    return CHIP_NO_ERROR;
}
CHIP_ERROR ThreadStackManagerImpl::_DeinitThreadStack()
{
    ChipLogProgress(DeviceLayer, "JYSIM ThreadStackManagerImpl::_DeinitThreadStack");
    if (instance != NULL)
    {
        otDestroyInstance();
        instance = NULL;
        threadId = 0;
        ChipLogProgress(DeviceLayer, "Thread stack manager instance destroyed");
    }
    ChipLogProgress(DeviceLayer, "Thread stack manager successfully finished");
    return CHIP_NO_ERROR;
}
#else
CHIP_ERROR ThreadStackManagerImpl::_InitThreadStack()
{
    ChipLogProgress(DeviceLayer, "JYSIM ThreadStackManagerImpl::_InitThreadStack");
    char currentDir[200]             = { 0 };
    char fileName[300]               = { 0 };
    char threadSettingsFileName[100] = { 0 };
    uint64_t nodeId;
    getcwd(currentDir, 500);
    sprintf(fileName, "%s/lib/darwin/libopenthread-%s", currentDir, IoTer::getSamsungThreadVersion());
    OpenThreadDL_Manager::getInstance().open(fileName);
    OpenThreadDL_Manager::getInstance().mOtGetInstance(&instance, &threadId, IoTer::getSamsungThreadComport(),
                                                       IoTer::getSamsungThreadDebug());

    ReturnErrorOnFailure(GenericThreadStackManagerImpl_OpenThread<ThreadStackManagerImpl>::DoInit(instance));

    OpenThreadDL_Manager::getInstance().mOtPlatRadioGetIeeeEui64(instance, reinterpret_cast<uint8_t *>(&nodeId));
    nodeId = chip::Encoding::BigEndian::HostSwap64(nodeId);
    snprintf(threadSettingsFileName, sizeof(threadSettingsFileName), "0_%llx.data", nodeId);

    ChipLogProgress(DeviceLayer, "Thread stack manager initialized.. threadSettingsFileName[%s]", threadSettingsFileName);

#if CHIP_SAMSUNG_UI_LOGGING
    char pipeMsg[100] = { 0 };
    snprintf(pipeMsg, sizeof(pipeMsg), "threadsettingfile:%s", threadSettingsFileName);
    IoTer::pipe_logging(pipeMsg, IoTer::getSamsungDeviceNumber());
#endif

    return CHIP_NO_ERROR;
}
CHIP_ERROR ThreadStackManagerImpl::_DeinitThreadStack()
{
    ChipLogProgress(DeviceLayer, "JYSIM ThreadStackManagerImpl::_DeinitThreadStack");
    if (instance != NULL)
    {
        OpenThreadDL_Manager::getInstance().mOtDestroyInstance();
        instance = NULL;
        threadId = 0;
        OpenThreadDL_Manager::getInstance().close();
        ChipLogProgress(DeviceLayer, "Thread stack manager instance destroyed");
    }
    ChipLogProgress(DeviceLayer, "Thread stack manager successfully finished");
    return CHIP_NO_ERROR;
}
#endif

void ThreadStackManagerImpl::_LockThreadStack()
{
    ChipLogProgress(DeviceLayer, "_LockThreadStack");
    OpenThreadDL_Manager::getInstance().mOtLock();
}

bool ThreadStackManagerImpl::_TryLockThreadStack()
{
    ChipLogProgress(DeviceLayer, "TODO: _TryLockThreadStack");
    return false;
}

void ThreadStackManagerImpl::_UnlockThreadStack()
{
    ChipLogProgress(DeviceLayer, " _UnlockThreadStack");
    OpenThreadDL_Manager::getInstance().mOtUnlock();
}

} // namespace DeviceLayer
} // namespace chip
