#ifndef ASYNCTASK_H
#define ASYNCTASK_H

#include <QFuture>
#include <QMetaObject>
#include <QObject>
#include <QPromise>
#include <QThread>

#include <chrono>
#include <exception>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace async {

namespace detail {

template <typename T> struct is_qfuture : std::false_type {};
template <typename T> struct is_qfuture<QFuture<T>> : std::true_type {};

}

// 即发即忘：把 fn 排到 obj 所属线程，返回值丢弃；
// 回调若返回 QFuture<T> 会被 static_assert 拒绝，因为结果会静默丢失。
template <typename Fn>
void post(QObject *obj, Fn &&fn)
{
    using R = std::invoke_result_t<std::decay_t<Fn>>;
    static_assert(!detail::is_qfuture<R>::value,
                  "post discards the return value; use postFuture if fn "
                  "returns QFuture<T>");
    Q_ASSERT(obj != nullptr);
    const bool posted = QMetaObject::invokeMethod(obj,
        [fn = std::forward<Fn>(fn)]() mutable {
            if constexpr (std::is_void_v<R>)
                std::invoke(fn);
            else
                (void)std::invoke(fn);
        },
        Qt::QueuedConnection);
    Q_ASSERT(posted);
}

// 排队执行 fn 并通过 QFuture 返回结果。fn 抛出的异常记在 future 上，
// 由 waitForFinished() / result() 在调用点重抛，不会逃逸进事件循环。
template <typename Fn>
auto postFuture(QObject *obj, Fn &&fn)
    -> QFuture<std::invoke_result_t<std::decay_t<Fn>>>
{
    using R = std::invoke_result_t<std::decay_t<Fn>>;
    Q_ASSERT(obj != nullptr);
    QPromise<R> promise;
    promise.start();
    QFuture<R> future = promise.future();

    const bool posted = QMetaObject::invokeMethod(
        obj,
        [p = std::move(promise), fn = std::forward<Fn>(fn)]() mutable {
            try {
                if constexpr (std::is_void_v<R>)
                    std::invoke(fn);
                else
                    p.addResult(std::invoke(fn));
            } catch (...) {
                p.setException(std::current_exception());
            }
            p.finish();
        },
        Qt::QueuedConnection);
    Q_ASSERT(posted);

    return future;
}

// 同 postFuture，但基于 std::future。obj 不能属于当前线程：
// get() 会阻塞住任务运行所依赖的那个事件循环，必然死锁。
template <typename Fn>
auto postStdFuture(QObject *obj, Fn &&fn)
    -> std::future<std::invoke_result_t<std::decay_t<Fn>>>
{
    using R = std::invoke_result_t<std::decay_t<Fn>>;
    Q_ASSERT_X(obj && obj->thread() != QThread::currentThread(),
               "async::postStdFuture",
               "obj belongs to current thread: get() would block the event "
               "loop and deadlock (queued task can never run)");
    std::promise<R> promise;
    auto future = promise.get_future();

    const bool posted = QMetaObject::invokeMethod(
        obj,
        [p = std::move(promise), fn = std::forward<Fn>(fn)]() mutable {
            try {
                if constexpr (std::is_void_v<R>) {
                    std::invoke(fn);
                    p.set_value();
                } else {
                    p.set_value(std::invoke(fn));
                }
            } catch (...) {
                p.set_exception(std::current_exception());
            }
        },
        Qt::QueuedConnection);
    Q_ASSERT(posted);

    return future;
}

// 有界等待：超时返回 false（void）或空 optional，只停止等待，不取消已排队的任务。
template <typename Fn, typename Rep, typename Period>
auto postWithTimeout(QObject *obj, Fn &&fn,
                     std::chrono::duration<Rep, Period> timeout)
{
    using R = std::invoke_result_t<std::decay_t<Fn>>;
    auto future = postStdFuture(obj, std::forward<Fn>(fn));
    if (future.wait_for(timeout) != std::future_status::ready) {
        if constexpr (std::is_void_v<R>)
            return false;
        else
            return std::optional<R>(std::nullopt);
    }
    if constexpr (std::is_void_v<R>) {
        future.get();
        return true;
    } else {
        return std::optional<R>(future.get());
    }
}

}

#endif
