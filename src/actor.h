#ifndef ACTOR_H
#define ACTOR_H

#include "asynctask.h"
#include "execthread.h"
#include "qactor_global.h"

#include <QObject>

// actor 必须诞生在自己的执行线程上：通常用 ExecThread::spawn<T>()，
// 也可以自行 async::postFuture 到该线程构造（一次要建多个对象时更合适）。
// 于是构造、初始化与所有消息都在那里执行，状态不再跨线程。
class QACTOR_EXPORT Actor : public QObject
{
public:
    explicit Actor(ExecThread *execThread);
    ~Actor() override;

    // 把 onShutdown() 投递到执行线程，且只执行一次，重复调用无害。
    // 无需等待：ExecThread::stop() 会先排空邮箱。没有 future 承载结果，
    // 所以 onShutdown() 抛出的异常被就地捕获并记录。
    void shutdown();
    QObject *context() const { return m_execThread->context(); }

protected:
    // 派生类唯一需要实现的接口：清理逻辑，已在执行线程上运行且只运行一次。
    virtual void onShutdown() = 0;

    ExecThread *execThread() const { return m_execThread; }

private:
    ExecThread *m_execThread = nullptr;
    bool m_shutdownStarted = false;
};

#endif
