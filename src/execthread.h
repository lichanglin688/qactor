#ifndef EXECTHREAD_H
#define EXECTHREAD_H

#include "asynctask.h"
#include "qactor_global.h"

#include <QObject>
#include <QString>
#include <QThread>

#include <tuple>
#include <utility>

class QACTOR_EXPORT ExecThread : public QThread
{
    Q_OBJECT

public:
    explicit ExecThread(const QString &name = QString());

    // 先排空邮箱再关闭事件循环：调用前投递的任务都会执行完。
    // 不能从本线程内部调用，那必然死锁。
    void stop();
    ~ExecThread() override;

    // 邮箱宿主：向它投递任务，或把它交给 connect，即可切到本线程。
    QObject *context() const { return m_context; }

    // 创建 actor 的便捷方式：构造发生在本线程，对象之后不再迁移。
    // 想一次创建多个对象时，自行 async::postWithResult 到 context() 同样可以。
    template <typename T, typename... Args>
    QFuture<T *> spawn(Args &&...args)
    {
        return async::postWithResult(m_context,
            [this, args = std::tuple<std::decay_t<Args>...>(
                              std::forward<Args>(args)...)]() mutable {
                return std::apply(
                    [this](auto &...a) -> T * { return new T(this, a...); },
                    args);
            });
    }

private:
    QObject *m_context = nullptr;
};

namespace async {

// 以下两个重载转发到线程的 context()，调用方面向线程而非内部 QObject。

template <typename Fn>
void post(ExecThread *thread, Fn &&fn)
{
    Q_ASSERT(thread != nullptr);
    post(thread->context(), std::forward<Fn>(fn));
}

template <typename Fn>
auto postWithResult(ExecThread *thread, Fn &&fn)
    -> QFuture<std::invoke_result_t<std::decay_t<Fn>>>
{
    Q_ASSERT(thread != nullptr);
    return postWithResult(thread->context(), std::forward<Fn>(fn));
}

}

#endif
