/**
 * My simple base library for embedded systems.
 *
 * Note: This code was written while learning modern C++
 *       and may contain design or implementation issues.
 *
 * author: Kyungin Kim <myohancat@naver.com>
 */
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

/*
 * Usage:
 *
 * class Foo : public IWorker
 * {
 * public:
 *     // Stop the worker before Foo's members are destroyed.
 *     ~Foo() { stop(); }
 *
 *     bool start() { return mThread.start(*this); }
 *     void stop()  { mThread.stop(); }
 *
 * protected:
 *     void run() noexcept override
 *     {
 *         while (mThread.shouldRun())
 *         {
 *             // TODO
 *             mThread.msleep(1000);
 *         }
 *     }
 *
 * private:
 *     // Members are destroyed in reverse declaration order.
 *     // Keep resources used by run() before WorkerThread so they outlive it.
 *     Buffer mBuffer;
 *     WorkerThread mThread;
 * };
 */

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

class IWorker
{
public:
    virtual ~IWorker() = default;

    virtual void run() = 0;

    virtual bool onPrepare() { return true; }

    virtual void onStopRequested() { }

    virtual void onCleanup() { }
};


class WorkerThread
{
public:
    enum class State : std::uint8_t
    {
        Idle,
        Starting,
        Running,
        Stopping,
        Exited
    };

public:
    explicit WorkerThread(const std::string& name = "Worker", int priority = -1);
    ~WorkerThread();

    WorkerThread(const WorkerThread&) = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;

    bool start(IWorker& worker);

    // requestStop() + join()
    void stop();

    // without requestStop(), only join
    void join();

    bool shouldRun() const;

    bool isCurrentThread() const;

    State state() const;

    void msleep(int milliseconds);
    void sleep(int seconds);

private:
    void requestStop();

    void joinLocked();
    void setupCurrentThread();

private:
    std::string mName;
    int mPriority;

    std::thread mThread;
    IWorker* mWorker = nullptr;

    std::atomic<State> mState{State::Idle};

    std::mutex mLifecycleMutex;

    std::mutex mSleepMutex;
    std::condition_variable mSleepCv;

    inline static thread_local WorkerThread* sCurrentThread = nullptr;
};


inline WorkerThread::State WorkerThread::state() const
{
    return mState.load(std::memory_order_acquire);
}


inline bool WorkerThread::shouldRun() const
{
    return state() == State::Running;
}


inline bool WorkerThread::isCurrentThread() const
{
    return sCurrentThread == this;
}
