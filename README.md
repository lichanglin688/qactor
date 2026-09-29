# qactor

**中文** · [English](#english)

qactor 是一个基于 Qt 6 的 C++17 类actor 风格执行库：把一段工作投递到某个对象（或某条线程）所属的事件循环上执行。

它不是 标准actor 系统 —— 只是一个给 Qt 应用用的轻量构件。

```cpp
ExecThread thread("calculator");                         // 一条专属的事件循环线程
Calculator *calc = thread.spawn<Calculator>().result();  // actor 诞生在该线程上

//QFuture<Calculator*> future = thread.postFuture<Calculator>();
//Calculator *calc = future.result();

calc->add(2);                                            // 消息进入ExecThread，依次执行
calc->add(3);
calc->multiply(4).then(&app, [](QFuture<int> future) {   // 结果回到主线程消费
    qInfo() << "(2 + 3) * 4 =" << future.result();       // → 20
});
```

它由三件事构成：

- **`async` 投递** —— 工作的投递方式：`post(QObject*, Functor)` 即发即忘，`postFuture()` 用 `QFuture` 把结果或异常带回调用方，`postStdFuture()` 是 `std::future` 版本，`postWithTimeout()` 提供有界等待。
- **`ExecThread`** —— QThread的子类，也是 actor 的宿主；`stop()` 先执行完线程内的全部任务再关闭事件循环，因此 `shutdown()` 和 `deleteLater()` 不必手动等待。
- **`Actor`** —— 要求对象诞生在自己执行线程上的基类（通常用 `ExecThread::spawn<T>()`，也可以自行 `async::postFuture` 到该线程构造，一次建多个对象时更合适）；派生类只需实现 `onShutdown()`，说明清理做什么。

---

## English

qactor is a small C++17 actor-style execution library built on Qt 6: work is posted onto the event loop that owns some object (or some thread), so mutable state is only ever touched by that thread and no locking is needed.

It is not an actor system — just a lightweight building block for Qt applications.

```cpp
ExecThread thread("calculator");                         // a dedicated event-loop thread
Calculator *calc = thread.spawn<Calculator>().result();  // the actor is born on it

//QFuture<Calculator*> future = thread.postFuture<Calculator>();
//Calculator *calc = future.result();

calc->add(2);                                            // messages queue up, in order
calc->add(3);
calc->multiply(4).then(&app, [](QFuture<int> future) {   // result handled on the main thread
    qInfo() << "(2 + 3) * 4 =" << future.result();       // → 20
});
```

Three things make it up:

- **`async` dispatch** — the ways to hand work over: `post()` is fire and forget, `postFuture()` carries the result or the exception back through a `QFuture`, `postStdFuture()` is the `std::future` flavour, and `postWithTimeout()` offers bounded waiting.
- **`ExecThread`** — an event-loop thread with a mailbox, and the host of its actors; `stop()` drains the mailbox before closing the loop, so `shutdown()` and `deleteLater()` need no waiting.
- **`Actor`** — a base class requiring its objects to be born on their own execution thread (`ExecThread::spawn<T>()` is the convenient way, but posting your own construction with `async::postFuture` works too and suits creating several objects in one task); a derived actor only implements `onShutdown()` to say what cleanup means.

---

## Requirements

- CMake 3.19 or newer
- A C++17 compiler
- Qt 6.5 or newer (`Core`, `Test`, and `Network`; `Test` and `Network` can be omitted when their corresponding targets are disabled)

## Build and test

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

If Qt is not in a standard location, pass its installation prefix with `-DCMAKE_PREFIX_PATH=/path/to/Qt`.

Tests and examples are enabled by default. Disable them with `-DBUILD_TESTING=OFF` and `-DQACTOR_BUILD_EXAMPLES=OFF`, respectively.

## Install

```sh
cmake -S . -B build-install -DBUILD_TESTING=OFF -DQACTOR_BUILD_EXAMPLES=OFF -DCMAKE_INSTALL_PREFIX=/path/to/install
cmake --build build-install
cmake --install build-install
```

The install exports the CMake target `qactor::qactor` and a `qactorConfig.cmake` package. Consumers can use:

```cmake
find_package(qactor 0.1 CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE qactor::qactor)
```

Public headers are installed at the include root, for example `#include <actor.h>`.

On Windows the prefix defaults to `<build-dir>/install`, because the platform default sits under Program Files and would need administrator rights. Pass `-DCMAKE_INSTALL_PREFIX=/path/to/install` to override it.

## Examples

- [`examples/counter.cpp`](examples/counter.cpp)：`Calculator` 只暴露加、减、乘、除四个消息，每个返回更新后的值；结果用 `QFuture::then(&app, ...)` 在主线程消费。消息共用同一个邮箱，因此乘法能看到排在它前面的运算；除零在 actor 内抛出，异常经 future 回到调用方。
- [`examples/tcp_echo.cpp`](examples/tcp_echo.cpp)：回环 echo 的服务端与客户端各是一个 actor，各占一条 `ExecThread`；socket 的创建、使用与销毁都在 actor 自己的线程上，主线程不持有任何网络对象。

收尾顺序固定为 `shutdown()` → `deleteLater()` → `ExecThread::stop()`。前两步都不必等待，`stop()` 会排空邮箱。

**English:** [`examples/counter.cpp`](examples/counter.cpp) keeps one running value behind four messages — add, subtract, multiply and divide — each returning the updated value through a `QFuture`, consumed on the main thread with `QFuture::then(&app, ...)`. All messages share one mailbox, so the multiply observes the operations queued before it; dividing by zero throws inside the actor and the exception reaches the caller through the future. [`examples/tcp_echo.cpp`](examples/tcp_echo.cpp) runs a loopback echo server and a client as two actors, each on its own `ExecThread`, with every socket created, used and destroyed on the actor's own thread so nothing network-related touches the main thread. The teardown order is always `shutdown()` → `deleteLater()` → `ExecThread::stop()`; nothing needs waiting for, because `stop()` drains the mailbox.

## Contributing

Bug reports and focused pull requests are welcome. The project uses the MIT License; contributions are submitted under the same terms.

## License

[MIT](LICENSE)
