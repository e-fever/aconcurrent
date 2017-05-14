#pragma once
#include <QObject>

class AConcurrentTests : public QObject
{
    Q_OBJECT
public:
    explicit AConcurrentTests(QObject *parent = 0);

private slots:

    void test_mapped();

    void test_mapped_memory();

    void test_blockingMapped();

    void test_queue();
};

