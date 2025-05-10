#include "../include/thread_pool.hpp"

#include <stdexcept>
#include <utility>


namespace chad {

ThreadPool::ThreadPool(size_t numThreads) : stop_(false), activeThreads_(0) {
    resize(numThreads);
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        stop_ = true;
    }

    condition_.notify_all();

    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

size_t ThreadPool::getActiveThreadCount() const {
    return activeThreads_.load();
}

size_t ThreadPool::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return tasks_.size();
}

void ThreadPool::resize(size_t numThreads) {
    std::unique_lock<std::mutex> lock(queueMutex_);

    size_t oldSize = workers_.size();

    if (stop_) {
        return;
    }

    if (numThreads > oldSize) {
        workers_.reserve(numThreads);

        for (size_t i = oldSize; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(queueMutex_);
                        condition_.wait(lock, [this] {
                            return stop_ || !tasks_.empty();
                        });

                        if (stop_ && tasks_.empty()) {
                            return;
                        }

                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }

                    activeThreads_++;
                    task();
                    activeThreads_--;
                }
            });
        }
    } else if (numThreads < oldSize) {
        lock.unlock();
        condition_.notify_all();
    }
}

} // namespace chad