/**
 * My simple base library for embedded systems.
 *
 * Note: This code was written while learning modern C++
 *       and may contain design or implementation issues.
 *
 * author: Kyungin Kim <myohancat@naver.com>
 */
#include "WorkerThread.hpp"

#include "Log.hpp"

#include <chrono>
#include <cstdio>
#include <future>
#include <system_error>
#include <utility>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif


WorkerThread::WorkerThread(const std::string& name, int priority)
    : mName(name),
      mPriority(priority)
{
}


WorkerThread::~WorkerThread()
{
    ABORT_IF(isCurrentThread());
    stop();
}


bool WorkerThread::start(IWorker& worker)
{
    if (isCurrentThread())
    {
        LOGE("[%s] start() called from its own worker thread", mName.c_str());
        return false;
    }

    std::lock_guard<std::mutex> lock(mLifecycleMutex);

    if (mThread.joinable())
    {
        if (state() != State::Exited)
            return false;

        joinLocked();
    }

    if (state() != State::Idle)
        return false;

    if (!worker.onPrepare())
        return false;

    mWorker = &worker;
    mState.store(State::Starting, std::memory_order_release);

    std::promise<void> readyPromise;
    std::future<void> readyFuture = readyPromise.get_future();

    try
    {
        mThread = std::thread(
            [this, ready = std::move(readyPromise)]() mutable
            {
                sCurrentThread = this;

                setupCurrentThread();

                mState.store(State::Running, std::memory_order_release);

                ready.set_value();

                IWorker* worker = mWorker;

                if (worker != nullptr && shouldRun())
                    worker->run();

                mState.store(State::Exited, std::memory_order_release);
                mSleepCv.notify_all();

                sCurrentThread = nullptr;
            });
    }
    catch (const std::system_error&)
    {
        mState.store(State::Idle, std::memory_order_release);
        mWorker = nullptr;

        worker.onCleanup();
        return false;
    }

    readyFuture.wait();

    return true;
}


void WorkerThread::requestStop()
{
    State expected = State::Running;

    if (!mState.compare_exchange_strong(expected, State::Stopping
        , std::memory_order_acq_rel, std::memory_order_acquire))
    {
        return;
    }

    IWorker* worker = mWorker;

    if (worker != nullptr)
        worker->onStopRequested();

    mSleepCv.notify_all();
}


void WorkerThread::stop()
{
    if (isCurrentThread())
    {
        requestStop();
        return;
    }

    std::lock_guard<std::mutex> lock(mLifecycleMutex);

    requestStop();
    joinLocked();
}


void WorkerThread::join()
{
    if (isCurrentThread())
    {
        LOGE("[%s] join() called from its own worker thread", mName.c_str());
        return;
    }

    std::lock_guard<std::mutex> lock(mLifecycleMutex);
    joinLocked();
}


void WorkerThread::joinLocked()
{
    if (!mThread.joinable())
        return;

    mThread.join();

    IWorker* worker = mWorker;

    if (worker != nullptr)
        worker->onCleanup();

    mWorker = nullptr;
    mState.store(State::Idle, std::memory_order_release);
}


void WorkerThread::msleep(int milliseconds)
{
    if (milliseconds <= 0)
        return;

    if (!isCurrentThread())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        return;
    }

    std::unique_lock<std::mutex> lock(mSleepMutex);

    if (!shouldRun())
        return;

    mSleepCv.wait_for(lock, std::chrono::milliseconds(milliseconds), [this]() {
        return !shouldRun();
    });
}


void WorkerThread::sleep(int seconds)
{
    if (seconds <= 0)
        return;

    if (!isCurrentThread())
    {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        return;
    }

    std::unique_lock<std::mutex> lock(mSleepMutex);

    if (!shouldRun())
        return;

    mSleepCv.wait_for(lock, std::chrono::seconds(seconds), [this]() {
        return !shouldRun();
    });
}


void WorkerThread::setupCurrentThread()
{
#if defined(__linux__)
    char nameBuf[16];
    strncpy(nameBuf, mName.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = '\0';
    pthread_setname_np(pthread_self(), nameBuf);

    if (mPriority > 0)
    {
        sched_param param{};
        param.sched_priority = mPriority;

        pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    }
#endif
}
