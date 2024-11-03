#include "future.h"
#include "promise.h"

bool FutureBase::has_result() const { return promise->has_result(); }
bool FutureBase::finished() const { return promise->finished(); }
bool FutureBase::success() const { return promise->success(); }

bool FutureBase::wait(unsigned long timeout, unsigned long delay_interval) const { return promise->wait(timeout, delay_interval); }

void FutureBase::on_finished(FutureFinishedCb callback) const { promise->on_finished(std::move(callback)); }

Future<void>::Future(const std::shared_ptr<Promise<void>> &promise) : FutureBase(promise) {}
Future<void>::Future(const std::shared_ptr<PromiseBase> &promise) : FutureBase(promise) {}
Future<void>::Future(const FutureBase &future) : FutureBase(future) {}

Future<void> Future<void>::successful() {
    auto promise = Promise<void>::create();
    promise->set_success();
    return promise;
}

Future<void> Future<void>::errored() {
    auto promise = Promise<void>::create();
    promise->set_error();
    return promise;
}

Future<void> Future<void>::on_error(std::function<Future()> fn) { return FutureBase::on_error(*this, std::move(fn)); }

Future<void> Future<void>::finally(std::function<void()> fn) { return FutureBase::finally(*this, std::move(fn)); }