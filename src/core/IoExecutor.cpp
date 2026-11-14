#include "core/IoExecutor.h"
namespace game {
IoExecutor& IoExecutor::instance() {static IoExecutor executor;return executor;}
IoExecutor::IoExecutor():thread_([this]{for(;;) {
    std::function<void()> job;{std::unique_lock lock(mutex_);condition_.wait(lock,[&]{return stopping_ || !jobs_.empty();});
    if(jobs_.empty() && stopping_) return;job=std::move(jobs_.front());jobs_.pop_front();space_.notify_all();}job();
}}) {}
IoExecutor::~IoExecutor() {{std::lock_guard lock(mutex_);stopping_=true;}condition_.notify_all();thread_.join();}
}
