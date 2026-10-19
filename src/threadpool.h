#ifndef THREADPOOL_H
#define THREADPOOL_H

#include "actor.h"

#include <QThread>

#include <type_traits>
#include <utility>

class QObject;

// 共享线程池：给 actor 提供落脚的线程。
namespace actor {

/// 起线程池，必须在 spawnOnSharedPool 之前调用，与 Shutdown() 配对
void Init(int threadPoolSize = 1);

/// 停掉所有线程，并销毁登记过的所有 actor
void Shutdown();

namespace detail {

/// 轮转取下一条线程，由 spawnOnSharedPool 使用
QThread *nextThread();

/// 登记一个 actor，让 Shutdown() 一并销毁它
void registerActor(QObject *object);

} // namespace detail

///
/// 共享池版：造一个 actor，轮转搬到运行时的某条池线程上。
///
///   actor::Init(2);
///   auto c = actor::spawnOnSharedPool<Counter>(10);
///
/// 注意这**不是**"任务线程池"：对象长期存活、被钉死在一条线上，
/// 所有消息在那条线上串行。它只是把 N 个 actor 均摊到几组 worker 线程，
/// 省线程数；同一条线上的 actor 互相串行（一个卡住会拖住另一个）。
///
/// 想要"把一堆独立小任务分散到池里并行"，请用 QtConcurrent::run / async::run，
/// 不要走 actor。
///
/// 线程安全：并发 spawn 由运行时内部的锁保护（轮转游标、actor 登记表）。
/// 但 Shutdown() 之后（以及进行中）不要再 spawn。
///
template <class T, class... Args>
ActorRef<T> spawnOnSharedPool(Args &&...args)
{
    static_assert(std::is_base_of_v<QObject, T>,
                  "actor 必须是 QObject 子类：要靠 moveToThread + 事件循环来串行化");

    T *object = new T(std::forward<Args>(args)...);
    object->moveToThread(detail::nextThread());
    detail::registerActor(object);

    return ActorRef<T>(object);
}

} // namespace actor

#endif
