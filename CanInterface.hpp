/**
 * My simple CAN interface for learning purposes.
 *
 * Note: This code was written while learning CAN interface.
 *       and may contain design or implementation issues.
 *
 * author: Kyungin Kim <myohancat@naver.com>
 */
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum class CanType : uint8_t
{
    CLASSIC,
    FD
};

struct CanFrame
{
    static constexpr uint8_t CAN_MAX_DATA_LENGTH    = 8;
    static constexpr uint8_t CANFD_MAX_DATA_LENGTH  = 64;
    static constexpr uint8_t MAX_DATA_LENGTH = CANFD_MAX_DATA_LENGTH;

    uint32_t mCanId;
    uint8_t  mLen;
    CanType  mType;
    uint8_t  mFlags;
    uint8_t  mReserv;
    uint8_t  mData[MAX_DATA_LENGTH];

    void set(uint32_t id, const uint8_t* payload, uint8_t length, CanType type = CanType::CLASSIC, uint8_t flags = 0) noexcept
    {
        mCanId = id;
        mType = type;

        const uint8_t maxLength = type == (CanType::FD) ? CANFD_MAX_DATA_LENGTH : CAN_MAX_DATA_LENGTH;
        mLen = std::min(length, maxLength);
        mFlags = type == CanType::FD ? flags : 0;
        mReserv = 0;

        if (payload != nullptr && mLen > 0)
            std::memcpy(mData, payload, mLen);
    }

    bool isCanFD() const noexcept
    {
        return mType == CanType::FD;
    }
};


class ICanObserver
{
public:
    virtual ~ICanObserver() = default;

    virtual void onCanFrameReceived(const CanFrame& frame) = 0;
};


class ICanInterface
{
public:
    static constexpr uint32_t ANY_CAN_ID = 0xFFFFFFFFU;

    virtual ~ICanInterface() = default;

    virtual bool open(const char* interfaceName, bool isCanFD = false) = 0;
    virtual void close() = 0;

    virtual void addListener(uint32_t canId, ICanObserver* observer) = 0;
    virtual void removeListener(uint32_t canId, ICanObserver* observer) = 0;

    virtual bool write(const CanFrame& frame) = 0;
};


class CanInterfaceBase : public ICanInterface
{
public:
    void addListener(uint32_t canId, ICanObserver* observer) override
    {
        if (observer == nullptr)
            return;

        std::lock_guard<std::mutex> lock(mLock);

        if (canId == ANY_CAN_ID)
        {
            mAllFrameListeners.insert(observer);
            return;
        }

        auto& listeners = mListeners[canId];

        if (std::find(listeners.begin(), listeners.end(), observer) == listeners.end())
        {
            listeners.push_back(observer);
        }
    }

    void removeListener(uint32_t canId, ICanObserver* observer) override
    {
        if (observer == nullptr)
            return;

        std::lock_guard<std::mutex> lock(mLock);

        if (canId == ANY_CAN_ID)
        {
            mAllFrameListeners.erase(observer);
            return;
        }

        const auto mapIt = mListeners.find(canId);
        if (mapIt == mListeners.end())
            return;

        auto& listeners = mapIt->second;
        listeners.erase(std::remove(listeners.begin(), listeners.end(), observer), listeners.end());

        if (listeners.empty())
            mListeners.erase(mapIt);
    }

protected:
    void notifyListeners(const CanFrame& frame)
    {
        std::lock_guard<std::mutex> lock(mLock);

        for (ICanObserver* observer : mAllFrameListeners)
            observer->onCanFrameReceived(frame);

        const auto mapIt = mListeners.find(frame.mCanId);
        if (mapIt == mListeners.end())
            return;

        for (ICanObserver* observer : mapIt->second)
        {
            if (mAllFrameListeners.count(observer) != 0)
                continue;

            observer->onCanFrameReceived(frame);
        }
    }

private:
    std::mutex mLock;

    std::unordered_set<ICanObserver*> mAllFrameListeners;
    std::unordered_map<uint32_t, std::vector<ICanObserver*>> mListeners;
};
