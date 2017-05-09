#include <QQmlApplicationEngine>
#include <QTest>
#include <Automator>
#include <aconcurrent.h>
#include "aconcurrenttests.h"

AConcurrentTests::AConcurrentTests(QObject *parent) : QObject(parent)
{
    auto ref = [=]() {
        QTest::qExec(this, 0, 0); // Autotest detect available test cases of a QObject by looking for "QTest::qExec" in source code
    };
    Q_UNUSED(ref);
}

void AConcurrentTests::mapped()
{
    auto worker = [](int value) {
        return value * value;
    };

    QList<int> input;
    input << 1 << 2 << 3;

    QFuture<int> future = AConcurrent::mapped(QThreadPool::globalInstance(), input, worker);

    AConcurrent::waitForFinished(future);

    QVERIFY(future.isFinished());

    QList<int> result;
    QList<int> expected;
    expected  << 1 << 4 << 9;

    result = future.results();

    QVERIFY(result == expected);
}

void AConcurrentTests::blockingMapped()
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

