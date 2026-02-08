#pragma once

#include <mutex>
#include <condition_variable>

class CrossPlatformEvent
{
public:
    CrossPlatformEvent() : signaled(false) {}

    void Set()
    {
        std::unique_lock<std::mutex> lock(mtx);
        signaled = true;
        cv.notify_all();
    }

    void Reset()
    {
        std::unique_lock<std::mutex> lock(mtx);
        signaled = false;
    }

    bool Wait(int timeout_ms)
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (timeout_ms < 0)
        {
            cv.wait(lock, [this] { return signaled; });
            return true;
        }
        else
        {
            return cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] { return signaled; });
        }
    }

private:
    std::mutex mtx;
    std::condition_variable cv;
    bool signaled;
};
