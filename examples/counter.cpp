#include <actor.h>
#include <threadpool.h>

#include <QCoreApplication>
#include <QDebug>
#include <QFuture>
#include <QTimer>

#include <cstdlib>
#include <exception>
#include <stdexcept>

// actor 就是一个普通 QObject：方法只管改自己的状态，不碰线程、不碰队列。
// 串行化由 ActorRef 保证，同一个 ref 上的消息按投递顺序执行，不会并发。
class Calculator final : public QObject
{
public:
    explicit Calculator(int initial = 0)
        : m_value(initial)
    {
    }

    // 方法返回值会被 ask 包装成 QFuture 带回调用方
    int add(int operand) { m_value += operand; return m_value; }
    int subtract(int operand) { m_value -= operand; return m_value; }
    int multiply(int operand) { m_value *= operand; return m_value; }

    // 抛出的异常记在 future 上，由调用方 result()/waitForFinished() 重抛
    int divide(int operand)
    {
        if (operand == 0)
            throw std::invalid_argument("division by zero");
        m_value /= operand;
        return m_value;
    }

    int value() const { return m_value; }

private:
    int m_value = 0;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // 起两条池线程；此后 spawnOnSharedPool 出来的 actor 都归运行时管
    actor::Init(2);

    auto shared = actor::spawnOnSharedPool<Calculator>();
    qInfo() << "shared calculator lives on" << shared.thread()->objectName();

    shared.tell<&Calculator::add>(2);   // 只发消息，不等结果
    shared.tell<&Calculator::subtract>(1);

    // tell 与 ask 进同一条队列，所以这里必定能看到上面两条运算的结果
    qInfo() << "2 - 1 =" << shared.ask<&Calculator::value>().result();

    QFuture<int> product = shared.ask<&Calculator::multiply>(5);
    product.then(&app, [&app](QFuture<int> future) {
        try {
            qInfo() << "(2 - 1) * 5 =" << future.result();
        } catch (const std::exception &error) {
            qCritical() << "Calculator failed:" << error.what();
        }
    });

    QFuture<int> rejected = shared.ask<&Calculator::divide>(0);
    try {
        rejected.waitForFinished();
        qInfo() << "1 / 0 =" << rejected.result();
    } catch (const std::exception &error) {
        qInfo() << "Rejected divide by zero:" << error.what();
    }

    // 独占一条线程的版本：类型本身就说清它只跑在自己的那条线上
    actor::ActorThreaded<Calculator> dedicated("calculator");
    qInfo() << "dedicated calculator:" << dedicated.ask<&Calculator::add>(10).result()
            << "on" << dedicated.thread()->objectName();
    QTimer::singleShot(0, &app, [&app]() {
        app.exit();
    });
    const int exitCode = app.exec();

    // 停掉池线程并销毁登记过的 actor；dedicated 由自己的析构回收
    actor::Shutdown();

    return exitCode;
}
