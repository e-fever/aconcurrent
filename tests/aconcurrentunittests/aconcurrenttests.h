#pragma once
#include <QObject>
#include <QThreadPool>

class AConcurrentTests : public QObject
{
    Q_OBJECT
public:
    explicit AConcurrentTests(QObject *parent = 0);

private slots:

    void test_mapped();

    void test_mapped_void();

    void test_mapped_memory();

    void test_mapped_in_non_main_thread();

    void test_blockingMapped();

    void test_queue();

    void test_runOnMainThread();

private:

    QThreadPool pool;
};

