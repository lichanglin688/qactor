#include "actor.h"

#include <QDebug>

#include <exception>

Actor::Actor(ExecThread *execThread)
    : m_execThread(execThread)
{
    Q_ASSERT(QThread::currentThread() == execThread);
}

Actor::~Actor() = default;

void Actor::shutdown()
{
    async::post(m_execThread, [this] {
        if (m_shutdownStarted)
            return;
        m_shutdownStarted = true;
        try {
            onShutdown();
        } catch (const std::exception &error) {
            qWarning() << "onShutdown() failed:" << error.what();
        } catch (...) {
            qWarning() << "onShutdown() failed with an unknown exception";
        }
    });
}
