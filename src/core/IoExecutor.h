#pragma once
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
namespace game {
// One bounded disk queue. Tasks own snapshots and never reference live ECS/script state.
class IoExecutor {
public:
    static IoExecutor& instance();
    template<class F> auto submit(F function) {
        using T=std::invoke_result_t<F>;
        auto task=std::make_shared<std::packaged_task<T()>>(std::move(function));auto result=task->get_future();
        if(std::this_thread::get_id()==thread_.get_id()) {(*task)();return result;}
        std::unique_lock lock(mutex_);space_.wait(lock,[&]{return jobs_.size()<32;});
        jobs_.push_back([task]{(*task)();});condition_.notify_one();return result;
    }
    ~IoExecutor();
private:
    IoExecutor();
    std::mutex mutex_;std::condition_variable condition_,space_;std::deque<std::function<void()>> jobs_;
    bool stopping_=false;std::thread thread_;
};
}
