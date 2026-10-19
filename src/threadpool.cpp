#include "threadpool.h"

#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>

#include <cstddef>

namespace actor {

namespace {

/// 运行时的全局状态。所有字段都由 mutex 保护：
/// Init / Shutdown / spawn 可能从任意线程被调用，必须串行化访问。
struct Runtime
{
    QMutex mutex;                 // 保护下面所有字段
    QList<QThread *> threads;
    QList<QObject *> actors;
    std::size_t cursor = 0;       // 轮转游标
};

Runtime &runtime()
{
    static Runtime r;
    return r;
}

} // namespace

void Init(int threadPoolSize)
{
    Runtime &r = runtime();
    QMutexLocker lock(&r.mutex);

    Q_ASSERT_X(r.threads.isEmpty(), "actor::Init", "已经初始化过了");

    const int n = threadPoolSize > 0 ? threadPoolSize : 1;
    r.threads.reserve(n);

    for (int i = 0; i < n; ++i) {
        auto *thread = new QThread;
        thread->setObjectName(QStringLiteral("actorThread-%1").arg(i));
        thread->start();
        r.threads.append(thread);
    }
}

void Shutdown()
{
    Runtime &r = runtime();

    // 先把状态拷出来、清空，再在锁外做可能阻塞 / 跨线程的删除
    QList<QObject *> actors;
    QList<QThread *> threads;
    {
        QMutexLocker lock(&r.mutex);
        actors = r.actors;
        threads = r.threads;
        r.actors.clear();
        r.threads.clear();
        r.cursor = 0;
    }

    // actor 对象活在各自的 worker 线程上，跨线程 delete 是 Qt 禁止的；
    // 用 deleteLater 把删除事件丢回对象线程，再 quit/wait 让那条线处理完
    for (QObject *object : actors)
        object->deleteLater();

    for (QThread *thread : threads) {
        thread->quit();
        thread->wait();
    }

    // 线程已停（DeferredDelete 已处理），这里删的是 QThread 对象本身，
    // 它属于调用线程，可以直接删
    for (QThread *thread : threads)
        delete thread;
}

namespace detail {

QThread *nextThread()
{
    Runtime &r = runtime();
    QMutexLocker lock(&r.mutex);

    Q_ASSERT_X(!r.threads.isEmpty(), "actor::spawnOnSharedPool",
               "先调用 actor::Init()");

    QThread *thread = r.threads.at(
        static_cast<int>(r.cursor % static_cast<std::size_t>(r.threads.size())));
    ++r.cursor;
    return thread;
}

void registerActor(QObject *object)
{
    Runtime &r = runtime();
    QMutexLocker lock(&r.mutex);
    r.actors.append(object);
}

} // namespace detail

} // namespace actor
