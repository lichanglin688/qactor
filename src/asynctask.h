#ifndef ASYNCTASK_H
#define ASYNCTASK_H

#include <QFuture>
#include <QMetaObject>
#include <QObject>
#include <QPromise>
#include <QThread>
#include <QtGlobal>

#include <chrono>
#include <exception>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

namespace async {

template <typename Fn>
void tell(QObject *obj, Fn &&fn)
{
    Q_ASSERT_X(obj != nullptr, "async::tell", "目标对象为空");

    using R = std::invoke_result_t<std::decay_t<Fn>>;
    const bool posted = QMetaObject::invokeMethod(obj,
        [fn = std::forward<Fn>(fn)]() mutable {
            // tell 没有 future 承载结果，异常就地捕获记录：
            // 让它逃逸进事件循环等于让 Qt 终止进程
            try {
                if constexpr (std::is_void_v<R>)
                    std::invoke(fn);
                else
                    (void)std::invoke(fn);
            } catch (const std::exception &error) {
                qWarning("async::tell: %s", error.what());
            } catch (...) {
                qWarning("async::tell: unknown exception");
            }
        },
        Qt::QueuedConnection);
    Q_ASSERT(posted);
}

template <typename Fn>
auto ask(QObject *obj, Fn &&fn)
    -> QFuture<std::invoke_result_t<std::decay_t<Fn>>>
{
    Q_ASSERT_X(obj != nullptr, "async::ask", "目标对象为空");

    using R = std::invoke_result_t<std::decay_t<Fn>>;
    QPromise<R> promise;
    promise.start();
    QFuture<R> future = promise.future();

    // promise 归闭包所有：任务跑完就地 finish
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

template <typename Fn, typename Rep, typename Period>
auto postWithTimeout(QObject *obj, Fn &&fn,
                     std::chrono::duration<Rep, Period> timeout)
{
    using R = std::invoke_result_t<std::decay_t<Fn>>;
    auto fut = postStdFuture(obj, std::forward<Fn>(fn));
    if (fut.wait_for(timeout) != std::future_status::ready) {
        if constexpr (std::is_void_v<R>)
            return false;
        else
            return std::optional<R>(std::nullopt);
    }
    if constexpr (std::is_void_v<R>) {
        fut.get();
        return true;
    } else {
        return std::optional<R>(fut.get());
    }
}

}

#endif
