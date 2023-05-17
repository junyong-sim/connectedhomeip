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

#include <platform/NetworkCommissioning.h>

namespace chip {
namespace DeviceLayer {
namespace NetworkCommissioning {

class DarwinNetworkIterator : public NetworkIterator
{
public:
    DarwinNetworkIterator() {}

    ~DarwinNetworkIterator() override = default;

    size_t Count() override { return mInterfaceCount; }

    bool Next(Network & item) override
    {
        VerifyOrReturnError(mInterfaceCount, false);
        mInterfaceCount--;

        size_t interfaceNameLen = strlen(mInterfaceName);
        memcpy(item.networkID, mInterfaceName, interfaceNameLen);
        item.networkIDLen = static_cast<uint8_t>(interfaceNameLen);
        item.connected    = mInterfaceStatus;

        return true;
    }

    void Release() override { delete this; }

protected:
    size_t mInterfaceCount        = 0;
    char mInterfaceName[IFNAMSIZ] = {};
    bool mInterfaceStatus         = false;
};

class DarwinEthernetNetworkIterator final : public DarwinNetworkIterator
{
public:
    DarwinEthernetNetworkIterator()
    {
        auto & connectivityMgr = ConnectivityMgrImpl();
        ReturnOnFailure(connectivityMgr.GetEthernetInterfaceName(mInterfaceName, sizeof(mInterfaceName)));
        ReturnOnFailure(connectivityMgr.GetInterfaceStatus(mInterfaceName, &mInterfaceStatus));
        mInterfaceCount++;
    }
};

class DarwinEthernetDriver final : public EthernetDriver
{
public:
    // BaseDriver
    NetworkIterator * GetNetworks() override { return new DarwinEthernetNetworkIterator(); }

    // EthernetDriver
    uint8_t GetMaxNetworks() override { return 1; };
};

#if CHIP_DEVICE_CONFIG_ENABLE_THREAD
class DarwinThreadDriver final : public ThreadDriver
{
public:
    class ThreadNetworkIterator final : public NetworkIterator
    {
    public:
        ThreadNetworkIterator(DarwinThreadDriver * aDriver) : driver(aDriver) {}
        size_t Count() override;
        bool Next(Network & item) override;
        void Release() override { delete this; }
        ~ThreadNetworkIterator() override = default;

    private:
        DarwinThreadDriver * driver;
        bool exhausted = false;
    };

    // BaseDriver
    NetworkIterator * GetNetworks() override { return new ThreadNetworkIterator(this); }
    CHIP_ERROR Init(BaseDriver::NetworkStatusChangeCallback * networkStatusChangeCallback) override;
    void Shutdown() override;

    // WirelessDriver
    uint8_t GetMaxNetworks() override { return 1; }
    uint8_t GetScanNetworkTimeoutSeconds() override { return 10; }
    uint8_t GetConnectNetworkTimeoutSeconds() override { return 20; }

    CHIP_ERROR CommitConfiguration() override;
    CHIP_ERROR RevertConfiguration() override;

    Status RemoveNetwork(ByteSpan networkId, MutableCharSpan & outDebugText, uint8_t & outNetworkIndex) override;
    Status ReorderNetwork(ByteSpan networkId, uint8_t index, MutableCharSpan & outDebugText) override;
    void ConnectNetwork(ByteSpan networkId, ConnectCallback * callback) override;

    // ThreadDriver
    Status AddOrUpdateNetwork(ByteSpan operationalDataset, MutableCharSpan & outDebugText, uint8_t & outNetworkIndex) override;
    void ScanNetworks(ThreadDriver::ScanCallback * callback) override;

private:
    ThreadNetworkIterator mThreadIterator = ThreadNetworkIterator(this);
    Thread::OperationalDataset mSavedNetwork;
    Thread::OperationalDataset mStagingNetwork;
};

#endif // CHIP_DEVICE_CONFIG_ENABLE_THREAD

} // namespace NetworkCommissioning
} // namespace DeviceLayer
} // namespace chip
