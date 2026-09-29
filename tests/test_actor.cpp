#include <QtTest>

#include "actor.h"

#include <atomic>

class ProbeActor final : public Actor
{
public:
    ProbeActor(ExecThread *thread, std::atomic<int> *shutdowns)
        : Actor(thread), m_shutdowns(shutdowns)
    {
    }

    QFuture<QThread *> ping()
    {
        return async::postFuture(execThread(), [] { return QThread::currentThread(); });
    }

    void onShutdown() override { m_shutdowns->fetch_add(1); }

private:
    std::atomic<int> *m_shutdowns = nullptr;
};

class ActorTest : public QObject
{
    Q_OBJECT

private slots:
    void pendingShutdownSurvivesThreadStop();
};

// 全程不等待：stop() 必须先把邮箱排空，shutdown 与析构才会各发生一次。
void ActorTest::pendingShutdownSurvivesThreadStop()
{
    ExecThread thread("actorTest");
    std::atomic<int> shutdowns{0};
    ProbeActor *actor = thread.spawn<ProbeActor>(&shutdowns).result();

    QFuture<QThread *> future = actor->ping();
    future.waitForFinished();
    QCOMPARE(future.result(), static_cast<QThread *>(&thread));

    actor->shutdown();
    actor->shutdown();
    actor->deleteLater();
    thread.stop();

    QCOMPARE(shutdowns.load(), 1);
    QVERIFY(!thread.isRunning());
}

QTEST_GUILESS_MAIN(ActorTest)

#include "test_actor.moc"
