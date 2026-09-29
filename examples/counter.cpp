#include <actor.h>

#include <QCoreApplication>
#include <QDebug>
#include <QFuture>

#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <utility>

class Calculator final : public Actor
{
public:
    explicit Calculator(ExecThread *thread)
        : Actor(thread)
    {
    }

    QFuture<int> add(int operand)
    {
        return apply([this, operand] { m_value += operand; });
    }

    QFuture<int> subtract(int operand)
    {
        return apply([this, operand] { m_value -= operand; });
    }

    QFuture<int> multiply(int operand)
    {
        return apply([this, operand] { m_value *= operand; });
    }

    QFuture<int> divide(int operand)
    {
        return apply([this, operand] {
            if (operand == 0)
                throw std::invalid_argument("division by zero");
            m_value /= operand;
        });
    }

private:
    template <typename Fn>
    QFuture<int> apply(Fn &&fn)
    {
        return async::postFuture(execThread(), [this, fn = std::forward<Fn>(fn)] {
            fn();
            return m_value;
        });
    }

    void onShutdown() override
    {
        qInfo() << "Calculator stopped on"
                << QThread::currentThread()->objectName()
                << "with value" << m_value;
    }

    int m_value = 0;
};

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    ExecThread thread("calculator");
    Calculator *calculator = thread.spawn<Calculator>().result();

    calculator->add(2);
    calculator->add(3);
    calculator->subtract(1);

    calculator->multiply(4).then(&app, [&app](QFuture<int> future) {
        try {
            qInfo() << "(2 + 3 - 1) * 4 =" << future.result();
            app.exit(EXIT_SUCCESS);
        } catch (const std::exception &error) {
            qCritical() << "Calculator task failed:" << error.what();
            app.exit(EXIT_FAILURE);
        }
    });

    QFuture<int> rejected = calculator->divide(0);
    try {
        rejected.waitForFinished();
        qInfo() << "16 / 0 =" << rejected.result();
    } catch (const std::exception &error) {
        qInfo() << "Rejected 16 / 0:" << error.what();
    }

    const int exitCode = app.exec();

    calculator->shutdown();
    calculator->deleteLater();
    thread.stop();

    return exitCode;
}
