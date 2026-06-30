/**
 * My simple CAN interface for learning purposes.
 *
 * Note: This code was written while learning CAN interface.
 *       and may contain design or implementation issues.
 *
 * author: Kyungin Kim <myohancat@naver.com>
 */
#include "SocketCAN.hpp"

#include "Log.hpp"

#include <sys/uio.h>

#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/can/raw.h>

#include <errno.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef SAFE_CLOSE
#define SAFE_CLOSE(fd)                  \
    do                                  \
    {                                   \
        if ((fd) >= 0)                  \
        {                               \
            ::close(fd);                \
            (fd) = -1;                  \
        }                               \
    } while (0)
#endif

bool SocketCAN::open(const char* interfaceName, bool isCanFD)
{
    if (interfaceName == nullptr || *interfaceName == '\0')
        return false;

    mStrIF = interfaceName;
    mIsCanFD = isCanFD;
    return mThread.start(*this);
}

void SocketCAN::close()
{
    mThread.stop();
}

bool SocketCAN::write(const CanFrame& frame)
{
    bool requestWriteEvent = false;

    std::lock_guard<std::mutex> lock(mTxLock);

    if (!mThread.shouldRun() || mFd < 0)
        return false;

    if (mCurrentFrame == nullptr && mTxQueue.isEmpty())
    {
        WriteResult result = writeNative(frame);
        if (result == WriteResult::Success)
            return true;

        if (result == WriteResult::Failure)
            return false;

        requestWriteEvent = true;
    }

    CanFrame* txFrame = mFramePool.alloc();
    if (!txFrame)
    {
        LOGE("Pool is empty.");
        return false;
    }

    *txFrame = frame;

    if (!mTxQueue.put(txFrame))
    {
        LOGE("Cannot put frame.");
        mFramePool.free(txFrame);
        return false;
    }

    if (requestWriteEvent)
        requestPollOut();

    return true;
}

bool SocketCAN::onPrepare()
{
    mFd = ::socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, CAN_RAW);
    if (mFd < 0)
    {
        LOGE("Failed to create CAN socket: %s", strerror(errno));
        return false;
    }

    struct ifreq ifr;
    std::strncpy(ifr.ifr_name, mStrIF.c_str(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0';

    if (::ioctl(mFd, SIOCGIFINDEX, &ifr) < 0)
    {
        LOGE("Failed to get interface index: %s", strerror(errno));
        SAFE_CLOSE(mFd);
        return false;
    }

    struct sockaddr_can addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    if (::bind(mFd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    {
        LOGE("Failed to bind CAN socket: %s", strerror(errno));
        SAFE_CLOSE(mFd);
        return false;
    }

    if (mIsCanFD)
    {
        int enable = 1;
        if (setsockopt(mFd, SOL_CAN_RAW, CAN_RAW_FD_FRAMES, &enable, sizeof(enable)) < 0)
        {
            LOGE("Failed to enable CAN FD. errno=%d(%s)", errno, std::strerror(errno));
            SAFE_CLOSE(mFd);
            return false;
        }
    }

    if (::pipe2(mPipeFd, O_NONBLOCK) < 0)
    {
        LOGE("Failed to create non-blocking pipe: %s", strerror(errno));
        SAFE_CLOSE(mFd);
        return false;
    }

    return true;
}

void SocketCAN::onStopRequested()
{
    terminate();
}

void SocketCAN::onCleanup()
{
    {
        std::lock_guard<std::mutex> lock(mTxLock);

        if (mCurrentFrame != nullptr)
        {
            mFramePool.free(mCurrentFrame);
            mCurrentFrame = nullptr;
        }

        mTxQueue.clear();
    }
    mFramePool.clear();

    SAFE_CLOSE(mFd);

    SAFE_CLOSE(mPipeFd[0]);
    SAFE_CLOSE(mPipeFd[1]);
}

bool SocketCAN::flushWrite()
{
    std::lock_guard<std::mutex> lock(mTxLock);

    CanFrame* frame = nullptr;

    if (mCurrentFrame != nullptr)
    {
        frame = mCurrentFrame;
        mCurrentFrame = nullptr;
    }
    else if (!mTxQueue.get(&frame))
    {
        return false;
    }

    WriteResult result = writeNative(*frame);

    switch(result)
    {
        case WriteResult::Success:
            mFramePool.free(frame);
            break;

        case WriteResult::TryAgain:
            mCurrentFrame = frame;
            break;

        case WriteResult::Failure:
            LOGE("Failed to flush CAN frame.");
            mFramePool.free(frame);
            break;
    }

    return mCurrentFrame != nullptr || !mTxQueue.isEmpty();
}

void SocketCAN::handleReadable()
{
    constexpr unsigned int MAX_RX_FRAMES = 10;

    struct canfd_frame frames[MAX_RX_FRAMES]{};
    struct iovec       iovs[MAX_RX_FRAMES]{};
    struct mmsghdr     messages[MAX_RX_FRAMES]{};

    for (unsigned int i = 0; i < MAX_RX_FRAMES; ++i)
    {
        iovs[i].iov_base = &frames[i];
        iovs[i].iov_len  = CANFD_MTU;

        messages[i].msg_hdr.msg_iov    = &iovs[i];
        messages[i].msg_hdr.msg_iovlen = 1;
    }

    const int count = ::recvmmsg(mFd, messages, MAX_RX_FRAMES, MSG_DONTWAIT, nullptr);
    if (count < 0)
    {
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;

        if (errno == EINTR)
            return;

        LOGE("recvmmsg failed. errno=%d(%s)", errno, strerror(errno));

        return;
    }

    for (int i = 0; i < count; ++i)
    {
        const unsigned int receivedSize = messages[i].msg_len;
        const struct canfd_frame& nativeFrame = frames[i];

        CanFrame frame{};

        if (receivedSize == sizeof(struct can_frame))
        {
            frame.set(nativeFrame.can_id, nativeFrame.data, nativeFrame.len, CanType::CLASSIC, 0);
        }
        else if (receivedSize == sizeof(struct canfd_frame))
        {
            frame.set(nativeFrame.can_id, nativeFrame.data, nativeFrame.len, CanType::FD, nativeFrame.flags);
        }
        else
        {
            LOGE("Unexpected CAN frame size: %u", receivedSize);
            continue;
        }

        notifyListeners(frame);
    }

}


SocketCAN::WriteResult SocketCAN::writeNative(const CanFrame& frame)
{
    ssize_t written = -1;
    ssize_t expected = 0;

    if (frame.isCanFD())
    {
        if (!mIsCanFD || frame.mLen > CanFrame::CANFD_MAX_DATA_LENGTH)
        {
            LOGE("Invalid CAN FD frame length: %u", frame.mLen);
            return WriteResult::Failure;
        }
    }
    else
    {
        if (frame.mLen > CanFrame::CAN_MAX_DATA_LENGTH)
        {
            LOGE("Invalid Classical CAN frame length: %u", frame.mLen);
            return WriteResult::Failure;
        }
    }

    if (frame.isCanFD())
    {
        struct canfd_frame nativeFrame{};

        nativeFrame.can_id = frame.mCanId;
        nativeFrame.len    = frame.mLen;
        nativeFrame.flags  = frame.mFlags;

        if (frame.mLen > 0)
            std::memcpy(nativeFrame.data, frame.mData, frame.mLen);

        expected = sizeof(struct canfd_frame);
        written = ::write(mFd, &nativeFrame, expected);
    }
    else
    {
        struct can_frame nativeFrame{};

        nativeFrame.can_id = frame.mCanId;
        nativeFrame.can_dlc = frame.mLen;

        if (frame.mLen > 0)
            std::memcpy(nativeFrame.data, frame.mData, frame.mLen);

        expected = sizeof(struct can_frame);
        written = ::write(mFd, &nativeFrame, CAN_MTU);
    }

    if (written == expected)
        return WriteResult::Success;

    if (written < 0 &&
       (errno == EAGAIN || errno == EWOULDBLOCK || errno == ENOBUFS))
    {
        return WriteResult::TryAgain;
    }

    LOGE("CAN write failed. errno=%d(%s)", errno, std::strerror(errno));

    return WriteResult::Failure;
}


void SocketCAN::run()
{
__TRACE__
    pollfd pfds[2];
    pfds[0] = {mPipeFd[0], POLLIN, 0};

    pfds[1].fd = mFd;
    pfds[1].events = POLLIN | POLLERR | POLLHUP | POLLNVAL;

    while (mThread.shouldRun())
    {
        int ret;
        do {

            pfds[0].revents = 0;
            pfds[1].revents = 0;

            ret = ::poll(pfds, 2, -1);
        } while (ret < 0 && errno == EINTR);

        if (ret < 0)
        {
            LOGE("poll failed. %s(errno=%d)", strerror(errno), errno);
            break;
        }

        if (pfds[0].revents & POLLIN)
        {
            std::uint8_t cmd[64];
            int n = ::read(mPipeFd[0], &cmd, sizeof(cmd));

            for (ssize_t ii = 0; ii < n; ++ii)
            {
                if (cmd[ii] == 'T')
                {
                    mThread.stop();
                    break;
                }
                else if (cmd[ii] == 'W')
                {
                    pfds[1].events |= POLLOUT;
                }
            }
            continue;
        }

        if (pfds[1].revents & POLLIN)
        {
            handleReadable();
        }

        if (pfds[1].revents & POLLOUT)
        {
            bool hasMoreData = flushWrite();
            if (!hasMoreData)
                pfds[1].events &= ~POLLOUT;
        }

        if (pfds[1].revents & (POLLERR | POLLHUP | POLLNVAL))
            break;
    }
}
