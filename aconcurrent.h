#pragma once

#include <QFuture>
#include <QtConcurrent>
#include <QThreadPool>
#include <QTimer>
#include <asyncfuture.h>
#include <functional>

/* Enhance QtConcurrent by AsyncFuture
 *
 * 1) mapped() may take lambda function as input
 * 2) blockingMapped() - Run with event loop, don't block UI
 *
 */

namespace AConcurrent {

    namespace Private {
        // Value is a wrapper of data structure which could contain <void> type.
        template <typename R>
        class Value {
        public:
           Value() {
           }

           template <typename Functor>
           void run(Functor functor) {
               value = functor();
           }

           void complete(AsyncFuture::Deferred<R> defer) {
               defer.complete(value);
           }

           R value;
        };

        template <>
        class Value<void> {
        public:
            Value() {

            }

            template <typename Functor>
            void run(Functor functor) {
                functor();
            }

            void complete(AsyncFuture::Deferred<void> defer) {
                defer.complete();
            }
        };

        // function_traits: Source: http://stackoverflow.com/questions/7943525/is-it-possible-to-figure-out-the-parameter-type-and-return-type-of-a-lambda

        template <typename T>
        struct function_traits
                : public function_traits<decltype(&T::operator())>
        {};

        template <typename ClassType, typename ReturnType, typename... Args>
        struct function_traits<ReturnType(ClassType::*)(Args...) const>
        // we specialize for pointers to member function
        {
            enum { arity = sizeof...(Args) };
            // arity is the number of arguments.

            typedef ReturnType result_type;

            template <size_t i>
            struct arg
            {
                typedef typename std::tuple_element<i, std::tuple<Args...>>::type type;
                // the i-th argument is equivalent to the i-th tuple element of a tuple
                // composed of those arguments.
            };
        };

        template <typename R>
        inline void completeDefer(AsyncFuture::Deferred<R> defer, const QVector<QFuture<R>> &futures) {
            QList<R> res;
            for (int i = 0 ; i < futures.size() ;i++) {
                res << futures[i].result();
            }
            defer.complete(res);
        }

        template <>
        inline void completeDefer<void>(AsyncFuture::Deferred<void> defer, const QVector<QFuture<void>>& futures) {
            Q_UNUSED(futures);
            defer.complete();
        }

        inline QString key(QObject* object, QString extraKey) {
            return QString("%1-%2").arg(QString::number((long) object, 16) ).arg(extraKey);
        }

        extern QMap<QString, QFuture<void>> debounceStore;

        template <typename T>
        class CustomDeferred : public AsyncFuture::Deferred<T> {
        public:
            void setProgressValue(int value) {
                AsyncFuture::Deferred<T>::deferredFuture->setProgressValue(value);
            }

            void setProgressRange(int min, int max) {
                AsyncFuture::Deferred<T>::deferredFuture->setProgressRange(min, max);
            }

            void reportResult(T value, int index) {
                AsyncFuture::Deferred<T>::deferredFuture->reportResult(value, index);
            }
        };

        template <>
        class CustomDeferred<void> : public AsyncFuture::Deferred<void> {
        public:
            void setProgressValue(int value) {
                AsyncFuture::Deferred<void>::deferredFuture->setProgressValue(value);
            }

            void setProgressRange(int min, int max) {
                AsyncFuture::Deferred<void>::deferredFuture->setProgressRange(min, max);
            }
        };

    } // End of Private namespace

    /// Run a function on main thread. If it is already in main thread, it will be executed in next tick.
    template <typename Functor>
    inline auto runOnMainThread(Functor func) -> QFuture<typename Private::function_traits<Functor>::result_type> {
        typedef typename Private::function_traits<Functor>::result_type RET;
        QObject tmp;
        AsyncFuture::Deferred<RET> defer;
        auto worker = [=]() {
            Private::Value<RET> value;
            value.run(func);
            value.complete(defer);
        };
        QObject::connect(&tmp, &QObject::destroyed, QCoreApplication::instance(), worker, Qt::QueuedConnection);
        return defer.future();
    }

    // Wait for a QFuture to be finished without blocking
    template <typename T>
    inline void await(QFuture<T> future, int timeout = -1) {
        if (future.isFinished()) {
            return;
        }

        QFutureWatcher<T> watcher;
        watcher.setFuture(future);
        QEventLoop loop;

        if (timeout > 0) {
            QTimer::singleShot(timeout, &loop, &QEventLoop::quit);
        }

        QObject::connect(&watcher, SIGNAL(finished()), &loop, SLOT(quit()));

        loop.exec();
    }

    template <typename T, typename Functor>
    void debounce(QObject* context, QString key, QFuture<T> future, Functor functor) {

        QString k = Private::key(context, key);

        auto defer = AsyncFuture::deferred<void>();

        auto cleanup = [=]() {
            if (Private::debounceStore.contains(k) &&
                Private::debounceStore[k] == defer.future()) {
                Private::debounceStore.remove(k);
            }
        };

        defer.subscribe([=]() {
            if (Private::debounceStore.contains(k) &&
                Private::debounceStore[k] == defer.future()) {
                functor();
            }
            cleanup();
        }, cleanup);

        defer.complete(future);

        if (Private::debounceStore.contains(k)) {
            Private::debounceStore[k].cancel();
        }
        Private::debounceStore[k] = defer.future();
    }

    template <typename ARG, typename RET>
    class Queue {
    private:
        class Context {
        public:
            QPointer<QThreadPool> pool;
            std::function<RET(ARG)> worker;
            AsyncFuture::Deferred<RET> defer;
            QQueue<ARG> queue;

            // Is the head started?
            bool started;
        };

    public:
        Queue(QThreadPool* pool, std::function<RET(ARG)> worker) : d(QSharedPointer<Context>::create()) {
            d->pool = pool;
            d->worker = worker;
            d->started = false;
        }

        int count() {
            return d->queue.count();
        }

        // The head's future
        QFuture<RET> future() {
            return d->defer.future();
        }

        void enqueue(ARG arg) {
            d->queue.enqueue(arg);
        }

        ARG head() {
            return d->queue.head();
        }

        void dequeue() {
            d->defer = AsyncFuture::deferred<RET>();
            d->started = false;
            if (d->queue.count() > 0) {
                d->queue.dequeue();
            }
        }

        QFuture<RET> run() {
            // Run the head item
            if (d->started || d->queue.count() == 0) {
                return d->defer.future();
            }
            d->started = true;
            auto f = QtConcurrent::run(d->pool, d->worker, d->queue.head());
            d->defer.complete(f);
            return d->defer.future();
        }

    private:
        QSharedPointer<Context> d;
    };

    template <typename Functor>
    inline auto queue(QThreadPool*pool, Functor func) -> Queue<
        typename Private::function_traits<Functor>::template arg<0>::type,
        typename Private::function_traits<Functor>::result_type
    >{
        Queue<
                typename Private::function_traits<Functor>::template arg<0>::type,
                typename Private::function_traits<Functor>::result_type
            > queue(pool, func);


        return queue;
    }

    template <typename ARG, typename RET>
    class Pipeline {
    private:
        class Context {
        public:
            QPointer<QThreadPool> pool;
            std::function<RET(ARG)> worker;
            int next;
            QList<RET> input;
            int running;

            Private::CustomDeferred<RET> defer;
            QList<AsyncFuture::Deferred<RET>> defers;
        };

        QSharedPointer<Context> d;

        void run() {
            if (d->running >= d->pool->maxThreadCount() || d->next >= d->input.size()) {
                return;
            }

            int index = d->next;
            ARG input = d->input.at(d->next);
            auto defer = d->defers.at(d->next);
            d->next++;
            d->running++;

            auto future = QtConcurrent::run(d->pool, d->worker, input);
            defer.complete(future);
            defer.subscribe([=]() {
                int progressValue = d->defer.future().progressValue();
                d->defer.reportResult(defer.future().result(), index);
                d->defer.setProgressValue(progressValue+1);
                d->running--;
                run();
            });
        }

    public:
        Pipeline(QThreadPool* pool, std::function<RET(ARG)> worker) : d(QSharedPointer<Context>::create()) {
            d->pool = pool;
            d->worker = worker;
            d->next = 0;
            d->running = 0;
            d->defer.subscribe([]() {}, [](){
               qDebug() << "cancelled";
            });

        }

        QFuture<RET> add(ARG value) {
            auto defer = AsyncFuture::Deferred<RET>();
            runOnMainThread([=]() {
                d->input.append(value);
                d->defers << defer;
                d->defer.setProgressRange(0, d->defers.size());
                if (d->running < d->pool->maxThreadCount()) {
                    run();
                }
            });
            return defer.future();
        }

        QFuture<RET> future() {
            return d->defer.future();
        }

    };

    template <typename Functor>
    inline auto pipeline(QThreadPool*pool, Functor func) -> Pipeline<
        typename Private::function_traits<Functor>::template arg<0>::type,
        typename Private::function_traits<Functor>::result_type
    >{
        Pipeline<
                typename Private::function_traits<Functor>::template arg<0>::type,
                typename Private::function_traits<Functor>::result_type
            > res(pool, func);

        return res;
    }

    namespace Private {    
        namespace Scheduler {

            template <typename Sequence, typename Functor>
            class Scheduler {
            public:
                typedef typename Private::function_traits<Functor>::template arg<0>::type ARG;
                typedef typename Private::function_traits<Functor>::result_type RET;

                Scheduler(QThreadPool*pool, Sequence sequence, Functor worker) :  worker(worker), pool(pool) {
                    source = sequence;
                    finishedCount = 0;
                    index = 0;
                    futures.resize(source.size());
                }

                void start() {
                    defer.setProgressRange(0, source.count());
                    int count = qMin(pool->maxThreadCount(), source.count());

                    runOnMainThread([=]() {
                        int c = count;
                        while (c--) {
                            enqueue();
                        }
                    });
                }

                void enqueue() {
                    auto future = QtConcurrent::run(pool.data(), worker, source[index]);
                    futures[index] = future;
                    index++;
                    AsyncFuture::observe(future).subscribe([=]() {
                        onFutureFinished();
                    });
                }

                void onFutureFinished() {
                    finishedCount++;
                    defer.setProgressValue(finishedCount);

                    if (index < source.size()) {
                        enqueue();
                        return;
                    }

                    if (finishedCount == source.size()) {
                        completeDefer(defer, futures);
                        delete this;
                    }
                }

                QFuture<RET> future() {
                    return defer.future();
                }

            private:
                Sequence source;
                std::function<RET(ARG)> worker;
                QPointer<QThreadPool> pool;
                AConcurrent::Private::CustomDeferred<RET> defer;
                QVector<QFuture<RET>> futures;
                int finishedCount;
                int index;
            };

            template <typename Sequence, typename Functor>
            inline auto schedule(QThreadPool*pool, Sequence sequence, Functor functor) -> Scheduler<Sequence, Functor>* {
                auto res = new Scheduler<Sequence, Functor>(pool, sequence, functor);
                res->start();
                return res;
            }
        }
    }

    template <typename Sequence, typename Functor>
    inline auto mapped(QThreadPool*pool, Sequence input, Functor func) -> QFuture<typename Private::function_traits<Functor>::result_type>{
        auto scheduler = Private::Scheduler::schedule(pool, input, func);
        return scheduler->future();
    }

    template <typename Sequence, typename Functor>
    inline auto blockingMapped(QThreadPool*pool, Sequence input, Functor func) -> QList<typename Private::function_traits<Functor>::result_type>{
        auto f = mapped(pool, input, func);
        await(f);
        return f.results();
    }

}

