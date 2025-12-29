
#ifndef __ThreadSafeQueue__
#define __ThreadSafeQueue__

#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue
{
public:
    
    bool empty()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        bool empty = queue_.empty();
        mlock.unlock();
        return empty;
    }
    
    void pop(T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty())
        {
            cond_.wait(mlock);
        }
        item = queue_.front();
        queue_.pop();
    }
    
    void push(const T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        queue_.push(item);
        mlock.unlock();
        cond_.notify_one();
    }
    
private:
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

#endif // !__ThreadSafeQueue__
