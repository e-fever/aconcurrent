#pragma once
#include <QObject>

class AConcurrentTests : public QObject
{
    Q_OBJECT
public:
    explicit AConcurrentTests(QObject *parent = 0);

private slots:
    void testCase();
};

