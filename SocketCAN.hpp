/**
 * My simple CAN interface for learning purposes.
 *
 * Note: This code was written while learning CAN interface.
 *       and may contain design or implementation issues.
 *
 * author: Kyungin Kim <myohancat@naver.com>
 */
#pragma once

#include "CanInterface.hpp"

#include "WorkerThread.hpp"
#include "Pool.hpp"
#include "CircularQueue.hpp"

#include <mutex>
#include <unistd.h>

class SocketCAN : public CanInterfaceBase, private IWorker
{
public:
    ~SocketCAN() override { close(); }

    bool open(const char* interfaceName, bool isCanFD = false) override;
    void close() override;

    bool write(const CanFrame& frame) override;

private:
    bool onPrepare() override;
    void onStopRequested() override;
    void onCleanup() override;
    void run() override;

    void requestPollOut();
    void terminate();

    void handleReadable();

    enum class WriteResult
    {
        Success,
        Failure,
        TryAgain
    };

    WriteResult writeNative(const CanFrame& frame);
    bool flushWrite();

private:
    static constexpr uint32_t MAX_TX_QUEUE_SIZE = 128;

    std::string mStrIF;
    bool mIsCanFD = false;

    int mFd = -1;
    int mPipeFd[2] { -1, -1};

    DataPool<CanFrame, MAX_TX_QUEUE_SIZE + 1> mFramePool;

    std::mutex mTxLock;
    CircularQueue<CanFrame*, MAX_TX_QUEUE_SIZE> mTxQueue;
    CanFrame* mCurrentFrame = nullptr;

    WorkerThread mThread;
};

inline void SocketCAN::requestPollOut()
{
    if (mPipeFd[1] >= 0)
    {
        char cmd = 'W';
        ::write(mPipeFd[1], &cmd, 1);
    }
}

inline void SocketCAN::terminate()
{
    if (mPipeFd[1] >= 0)
    {
        char cmd = 'T';
        ::write(mPipeFd[1], &cmd, 1);
    }
}
