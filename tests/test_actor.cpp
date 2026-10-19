#include <QtTest>

#include "actor.h"
#include "threadpool.h"

#include <QThread>
#include <QVector>

#include <stdexcept>

namespace {

// 每个方法都在 actor 自己的线程上被调用，所以这里的字段不需要加锁。
class Counter final : public QObject
{
public:
    explicit Counter(int start = 0)
        : m_value(start)
    {
    }

    int add(int operand) { m_value += operand; return m_value; }
    int value() const { return m_value; }

    void record(int item) { m_trace.append(item); }
    QVector<int> trace() const { return m_trace; }

    QThread *currentThread() const { return QThread::currentThread(); }

    int boom() const { throw std::runtime_error("boom"); }

private:
    int m_value = 0;
    QVector<int> m_trace;
};

} // namespace

class ActorTest : public QObject
{
    Q_OBJECT

private slots:
    void init() { actor::Init(2); }
    void cleanup() { actor::Shutdown(); }

    void messagesRunOnActorThread();
    void tellAndAskKeepOrder();
    void exceptionReachesCaller();
    void dedicatedActorHasOwnThread();
    void sharedPoolIsRoundRobin();
};

// ask 投递的方法在 actor 所在线程执行，而不是调用线程
void ActorTest::messagesRunOnActorThread()
{
    auto counter = actor::spawnOnSharedPool<Counter>(10);

    QCOMPARE(counter.ask<&Counter::currentThread>().result(), counter.thread());
    QCOMPARE(counter.ask<&Counter::value>().result(), 10);
    QVERIFY(counter.thread() != QThread::currentThread());
}

// tell 与 ask 进同一条队列，混用时投递顺序仍然成立
void ActorTest::tellAndAskKeepOrder()
{
    auto counter = actor::spawnOnSharedPool<Counter>();

    counter.tell<&Counter::record>(1);
    counter.ask<&Counter::record>(2).waitForFinished();
    counter.tell<&Counter::record>(3);

    QCOMPARE(counter.ask<&Counter::trace>().result(), QVector<int>({1, 2, 3}));
}

// 方法抛出的异常经 future 回到调用方
void ActorTest::exceptionReachesCaller()
{
    auto counter = actor::spawnOnSharedPool<Counter>();

    QFuture<int> future = counter.ask<&Counter::boom>();
    QVERIFY_EXCEPTION_THROWN(future.result(), std::runtime_error);
}

// ActorThreaded 给对象一条独占线程，且不交给 Shutdown() 托管
void ActorTest::dedicatedActorHasOwnThread()
{
    actor::ActorThreaded<Counter> counter("counterThread");

    QCOMPARE(counter.ask<&Counter::currentThread>().result(), counter.thread());
    QCOMPARE(counter.ask<&Counter::add>(5).result(), 5);
    QVERIFY(counter.thread() != QThread::currentThread());
    QVERIFY(!counter.thread()->objectName().isEmpty());
}

// 共享池版是轮转分配的：池里有两条线程，前两个 actor 落在不同线程上
void ActorTest::sharedPoolIsRoundRobin()
{
    auto first = actor::spawnOnSharedPool<Counter>();
    auto second = actor::spawnOnSharedPool<Counter>();

    QVERIFY(first.thread() != second.thread());
}

QTEST_GUILESS_MAIN(ActorTest)

#include "test_actor.moc"
