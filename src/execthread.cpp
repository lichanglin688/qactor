#include "execthread.h"

ExecThread::ExecThread(const QString &name)
{
    setObjectName(name.isEmpty() ? "execThread" : name);
    m_context = new QObject;
    m_context->moveToThread(this);
    start();
}

ExecThread::~ExecThread()
{
    stop();
}

void ExecThread::stop()
{
    if (m_context == nullptr)
        return;

    Q_ASSERT_X(QThread::currentThread() != this, "ExecThread::stop",
               "cannot stop the thread from inside itself");

    // 空任务排在既有任务之后，等它完成即等于排空邮箱；quit() 不会排空，
    async::postWithResult(m_context, [] {}).waitForFinished();

    m_context->deleteLater();
    m_context = nullptr;

    quit();
    wait();
}