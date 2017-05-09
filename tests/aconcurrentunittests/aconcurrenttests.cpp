#include <QQmlApplicationEngine>
#include <QTest>
#include <Automator>
#include "aconcurrenttests.h"

AConcurrentTests::AConcurrentTests(QObject *parent) : QObject(parent)
{
    auto ref = [=]() {
        QTest::qExec(this, 0, 0); // Autotest detect available test cases of a QObject by looking for "QTest::qExec" in source code
    };
    Q_UNUSED(ref);
}

void AConcurrentTests::testCase()
{

}

