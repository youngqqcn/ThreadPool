#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

// containers
#include <vector>
#include <queue>
// threading
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <future>
// utility wrappers
#include <memory>
#include <functional>
// exceptions
#include <stdexcept>

// std::thread pool for resources recycling
class ThreadPool
{
public:
    // the constructor just launches some amount of m_vctWorkers
    ThreadPool(size_t nThreads = std::thread::hardware_concurrency())
        : stop(false)
    {
        if (nThreads <= 0)
        {
            throw std::invalid_argument("more than zero threads expected");
        }

        this->m_vctWorkers.reserve(nThreads);
        for (size_t i = 0; i < nThreads; i++)
        {
            this->m_vctWorkers.emplace_back([this]() {
                while (true)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->m_qMutex);
                        this->condition.wait(
                            lock,
                            [this] {
                                return this->stop || !this->m_queueTasks.empty();
                            });

                        if (this->stop && this->m_queueTasks.empty())
                            return;

                        task = std::move(this->m_queueTasks.front());
                        this->m_queueTasks.pop();
                    }

                    task();
                }
            });
        }
    }

    // deleted copy&move ctors&assignments
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;

    // add new work item to the pool
    template <class F, class... Args>
    auto enqueue(F &&f, Args &&...args)
    {
        using return_type = decltype(f(args...));

        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));

        auto res = task->get_future();
        {
            std::unique_lock<std::mutex> lock(this->m_qMutex);
            this->m_queueTasks.emplace([task]() { (*task)(); });
        }
        this->condition.notify_one();
        return res;
    }

    // the destructor joins all threads
    virtual ~ThreadPool()
    {
        this->stop = true;
        this->condition.notify_all();
        for (std::thread &worker : this->m_vctWorkers)
        {
            worker.join();
        }
    }

private:
    // need to keep track of threads so we can join them
    std::vector<std::thread> m_vctWorkers;
    // the task queue
    std::queue<std::function<void()>> m_queueTasks;

    // synchronization
    std::mutex m_qMutex;
    std::condition_variable condition;
    // m_vctWorkers finalization flag
    std::atomic_bool stop;
};

#endif // THREAD_POOL_HPP
