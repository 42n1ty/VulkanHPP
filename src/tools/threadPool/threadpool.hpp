#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <optional>

#include <fmt/core.h>
#include <fmt/color.h>

class ThreadPool {
  std::vector<std::jthread> workers;
  std::queue<std::function<void()>> tasks;
  std::mutex qMtx;
  std::condition_variable qCnd;
  std::atomic<bool> stop;

public:
  ThreadPool(size_t num) : stop(0) {
    if(num == 0) {
      fmt::print(fmt::fg(fmt::color::yellow), "##WARNING##\tNum of threads can't be 0. Num sets to 2\n");
      num = 2;
    }
    fmt::print(fmt::fg(fmt::color::orange), "##INFO##\tNum of threads: {}\n", num);
    workers.reserve(num);

    for(size_t i = 0; i < num; ++i) {
      workers.emplace_back([this] {
        while(1) {
          std::optional<std::function<void()>> task;
          
          // -
          {
            std::unique_lock<std::mutex> lock(this->qMtx);
            qCnd.wait(lock, [this] {
              return stop || !tasks.empty();
            });
            if(stop && tasks.empty()) {
              return;
            }
            if(!tasks.empty()) {
              task = std::move(tasks.front());
              tasks.pop();
            } else {
              continue;
            }
          }
          // -
          
          if(task) (*task)();
        }
      });
    }
  }
  
  ~ThreadPool() {
    // -
    {
      std::unique_lock<std::mutex> lock(qMtx);
      stop = 1;
    }
    // -

    qCnd.notify_all();
    for(auto & worker : workers) {
      if(worker.joinable()) {
        worker.join();
      }
    }
  }

  template<class F, class... Args>
  auto add_task(F && f, Args && ... args) -> std::future<typename std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>;

  size_t getCount() const { return workers.size(); }
};


template<class F, class... Args>
auto ThreadPool::add_task(F && f, Args && ... args) -> std::future<typename std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>> {
  using retType = typename std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

  auto task = std::make_shared<std::packaged_task<retType()>>(
    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
  );

  std::future<retType> res = task->get_future();

  // -
  {
    std::unique_lock<std::mutex> lock(qMtx);
    if(stop) {
      fmt::print(fmt::fg(fmt::color::yellow), "##WARNING##\tUnable to add task when pool is stopped\n");
      return std::future<retType>();
    }
    tasks.emplace([task] {
      (*task)();
    });
    
    qCnd.notify_one();
    return res;
  }
  // -
}