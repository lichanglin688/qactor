#ifndef ACTOR_H
#define ACTOR_H

#include "asynctask.h"

#include <QFuture>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <QtGlobal>

#include <QString>

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

///
/// Actor 运行时。
///
///   actor::Init(2);                                 // 起线程池（threadpool.h）
///   auto c = actor::spawnOnSharedPool<Counter>(10);  // new Counter(10)，轮转搬进某条池线程
///   int v = c.ask<&Counter::Add>(5).result();        // async::ask 投递，返回 QFuture
///   c.tell<&Counter::Reset>();                       // async::tell 投递，不等结果
///   actor::Shutdown();
///
///   // 也可以指定线程：
///   auto d = actor::spawn<Counter>(&myThread, 10);
///   // 这种情况线程和对象都归调用方，Shutdown() 不管它们
///
/// 保证：同一个 actor 上的消息严格按投递顺序执行，不会并发，
/// 所以 actor 内部不需要加锁。跨 actor 则各自跑在各自的线程上。
///
namespace actor {

namespace detail {

template <class T> struct is_qfuture : std::false_type {};
template <class T> struct is_qfuture<QFuture<T>> : std::true_type {};
template <class T>
inline constexpr bool is_qfuture_v = is_qfuture<T>::value;

} // namespace detail

template <class T>
class ActorRef
{
public:
    using ObjectType = T;

    explicit ActorRef(T *object = nullptr) : m_object(object) {}

    bool isNull() const { return m_object.isNull(); }
    T *get() const { return m_object.data(); }

    QThread *thread() { return m_object ? m_object->thread() : nullptr; }

    ///
    /// 发消息并取结果：内部 async::ask。
    /// 调用方在任意线程都可以发，函数在 actor 所在线程执行。
    ///
    /// 两种返回，按方法自己的返回类型自动区分：
    ///   - 方法返回普通值 R     → QFuture<R>，函数跑完就完成
    ///   - 方法返回 QFuture<X>  → QFuture<X>（展平过的一层）；
    ///                           等那个内层 future 完成。
    ///
    template <auto Method, class... Args>
    auto ask(Args &&...args) const
    {
        static_assert(std::is_member_function_pointer_v<decltype(Method)>,
                      "Method 得是成员函数指针: ask<&T::foo>");
        static_assert(std::is_invocable_v<decltype(Method), T *,
                                          std::decay_t<Args>...>,
                      "这个成员函数不能用 T* 和给定的参数调用"
                      "（类不匹配？参数类型不对？参数个数不对？）");
        using R = std::invoke_result_t<decltype(Method), T *,
                                       std::decay_t<Args>...>;

        T *obj = m_object.data();
        auto message = [obj, msg = std::tuple<std::decay_t<Args>...>(
                                   std::forward<Args>(args)...)]() mutable {
            return std::apply(
                [obj](auto &&...a) {
                    return std::invoke(Method, obj,
                                       std::forward<decltype(a)>(a)...);
                },
                std::move(msg));
        };

        if constexpr (detail::is_qfuture_v<R>) {
            // 事务型：async::ask 会把 QFuture<X> 当返回值再包一层，
            // unwrap() 展平成 QFuture<X>
            return async::ask(obj, std::move(message)).unwrap();
        } else {
            return async::ask(obj, std::move(message));
        }
    }

    ///
    /// 发消息但不等结果：内部 async::tell。
    ///
    template <auto Method, class... Args>
    void tell(Args &&...args) const
    {
        static_assert(std::is_member_function_pointer_v<decltype(Method)>,
                      "Method 得是成员函数指针: tell<&T::foo>");
        static_assert(std::is_invocable_v<decltype(Method), T *,
                                          std::decay_t<Args>...>,
                      "这个成员函数不能用 T* 和给定的参数调用"
                      "（类不匹配？参数类型不对？参数个数不对？）");

        T *obj = m_object.data();
        async::tell(
            obj,
            [obj, msg = std::tuple<std::decay_t<Args>...>(
                            std::forward<Args>(args)...)]() mutable {
                std::apply(
                    [obj](auto &&...a) {
                        std::invoke(Method, obj,
                                    std::forward<decltype(a)>(a)...);
                    },
                    std::move(msg));
            });
    }

private:
    QPointer<T> m_object;
};

///
/// 造一个 actor，搬到调用方指定的线程上（第一个参数是目标 QThread*）。
///
template <class T, class... Args>
ActorRef<T> spawn(QThread *thread, Args &&...args)
{
    static_assert(std::is_base_of_v<QObject, T>,
                  "actor 必须是 QObject 子类：要靠 moveToThread + 事件循环来串行化");
    Q_ASSERT_X(thread != nullptr, "actor::spawn", "目标线程为空");

    T *object = new T(std::forward<Args>(args)...);
    object->moveToThread(thread);

    return ActorRef<T>(object);
}

///
/// 把"一个 actor + 它独占的一条线程"打包在一起。
///
/// 类型本身就是线程归属的说明：看到 ActorThreaded<DeviceLink> 就知道
/// "它有自己的一条线，只在那条线程里跑"，不用去翻注释。
///
/// 与 spawn<T>()（送进线程池轮转）相对：这里是一条对象一条线程，
/// 适合有阻塞 / 有定时器 / 不能互相干扰的对象。
///
/// 生命周期：
///   构造 —— 建线程 → 造对象搬上去 → start 线程；
///   析构 —— 对象 deleteLater → quit → wait（那条线自己处理 DeferredDelete）。
///
/// 注意 QThread 是值成员、不是被托管对象的子对象，所以 moveToThread 时
/// 不会把线程本身也拖走：线程始终归创建 ActorThreaded 的那条线程所有。
///
template <class T>
class ActorThreaded
{
public:
    template <class... Args>
    explicit ActorThreaded(const QString &threadName, Args &&...args)
    {
        m_thread.setObjectName(threadName);
        m_ref = spawn<T>(&m_thread, std::forward<Args>(args)...);
        m_thread.start();
    }

    ~ActorThreaded()
    {
        if (T *obj = m_ref.get())
            obj->deleteLater();
        m_thread.quit();
        m_thread.wait();
    }

    ActorRef<T> ref() const { return m_ref; }
    T *get() const { return m_ref.get(); }
    QThread *thread() { return &m_thread; }

    /// 转发：像 ActorRef 一样用，语义是"发到我这一条线"
    template <auto Method, class... Args>
    auto ask(Args &&...args) const
    {
        return m_ref.template ask<Method>(std::forward<Args>(args)...);
    }

    template <auto Method, class... Args>
    void tell(Args &&...args) const
    {
        m_ref.template tell<Method>(std::forward<Args>(args)...);
    }

private:
    QThread     m_thread;
    ActorRef<T> m_ref;
};

} // namespace actor

#endif
