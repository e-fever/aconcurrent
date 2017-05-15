#include <QQmlApplicationEngine>
#include <QTest>
#include <Automator>
#include <aconcurrent.h>
#include "aconcurrenttests.h"

//@TODO - Migrate to Automator::waitUntil()

void tick() {
    for (int i = 0 ; i < 3;i++) {
        Automator::wait(10);
    }
}

template <typename F>
bool waitUntil(F f, int timeout = -1) {
    QTime time;
    time.start();

    while (!f()) {
        Automator::wait(10);
        if (timeout > 0 && time.elapsed() > timeout) {
            tick();
            return false;
        }
    }
    tick();
    return true;
}

template <typename T>
bool waitUntil(QFuture<T> future, int timeout = -1) {
    return waitUntil([=]() {
       return future.isFinished();
    }, timeout);
    tick();
}

AConcurrentTests::AConcurrentTests(QObject *parent) : QObject(parent)
{
    auto ref = [=]() {
        QTest::qExec(this, 0, 0); // Autotest detect available test cases of a QObject by looking for "QTest::qExec" in source code
    };
    Q_UNUSED(ref);
}

void AConcurrentTests::test_mapped()
{
    auto worker = [](int value) {
        return value * value;
    };

    int count = 200;
    QList<int> input;
    QList<int> expected;

    for (int i = 0 ; i < count ; i++) {
        input << (i+1);
        expected << (i+1) * (i+1);
    }

    QFuture<int> future = AConcurrent::mapped(QThreadPool::globalInstance(), input, worker);

    AConcurrent::await(future);

    QVERIFY(future.isFinished());

    QList<int> result;
    result = future.results();

    QVERIFY(result == expected);
}

void AConcurrentTests::test_mapped_void()
{
    int count = 0;
    QMutex mutex;

    auto worker = [&](int value) -> void {
        Q_UNUSED(value);
        mutex.lock();
        count++;
        mutex.unlock();
    };

    QList<int> input;

    for (int i = 0 ; i < 3 ; i++) {
        input << (i+1);
    }

    QFuture<void> future = AConcurrent::mapped(QThreadPool::globalInstance(), input, worker);
    AConcurrent::await(future);

    QCOMPARE(count, 3);
}

void AConcurrentTests::test_mapped_memory()
{
    static int count = 0;

    class Dummy {
    public:

        Dummy() {
            count++;
        }

        ~Dummy() {
            count--;
        }
    };

    QSharedPointer<Dummy> dummy = QSharedPointer<Dummy>::create();
    QCOMPARE(count, 1);

    class Data {
    public:
        int result;
        QSharedPointer<Dummy> ref;
    };

    auto worker = [&](int value) -> Data {
        Data data;
        data.result = value * value;
        data.ref = dummy;
        return data;
    };

    {
        QList<int> input;
        input << 1 << 2 << 3;

        QFuture<Data> future = AConcurrent::mapped(QThreadPool::globalInstance(), input, worker);

        AConcurrent::await(future);

        QVERIFY(future.isFinished());
        QCOMPARE(count , 1);

        dummy.clear();
        QCOMPARE(count , 1);
    }

    QCOMPARE(count , 0);
}

void AConcurrentTests::test_blockingMapped()
{
    auto worker = [](int value) {
        return value * value;
    };

    QList<int> input;
    input << 1 << 2 << 3;

    QList<int> result = AConcurrent::blockingMapped(QThreadPool::globalInstance(), input, worker);
    QList<int> expected;
    expected  << 1 << 4 << 9;

    QVERIFY(result == expected);
}

void AConcurrentTests::test_queue()
{
    int count = 0;
    auto worker = [&](int value) -> qreal {
        Automator::wait(50);
        count++;
        return value * value;
    };

    auto queue = AConcurrent::queue(QThreadPool::globalInstance(), worker);
    QCOMPARE(queue.count(), 0);
    QCOMPARE(count, 0);

    auto f = queue.future();
    QCOMPARE(f.isFinished(), false);

    queue.enqueue(2);
    QCOMPARE(queue.head(), 2);
    QCOMPARE(queue.count(), 1);
    QCOMPARE(f.isFinished(), false);

    queue.run();
    QCOMPARE(f.isFinished(), false);

    waitUntil([=]() {
       return f.isFinished();
    }, 1000);

    QCOMPARE(f.isFinished(), true);
    QCOMPARE(f.result(), 4.0);
    QCOMPARE(queue.count(), 1);
    QCOMPARE(count, 1);

    queue.enqueue(3);
    QCOMPARE(f.isFinished(), true);
    QCOMPARE(queue.count(), 2);

    queue.dequeue();
    QCOMPARE(queue.count(), 1);

    f = queue.future();
    queue.run();
    waitUntil([=]() {
       return f.isFinished();
    }, 1000);

    QCOMPARE(f.isFinished(), true);
    QCOMPARE(f.result(), 9.0);
    QCOMPARE(queue.count(), 1);
    QCOMPARE(count, 2);
}

