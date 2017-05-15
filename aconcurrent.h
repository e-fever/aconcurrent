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

                int count = qMin(pool->maxThreadCount(), sequence.count());
                while (count--) {
                    enqueue();
                }
            }

            void enqueue() {
                auto future = QtConcurrent::run(worker, source[index]);
                futures[index] = future;
                index++;
                AsyncFuture::observe(future).subscribe([=]() {
                    onFutureFinished();
                });
            }

            void onFutureFinished() {
                finishedCount++;

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
            AsyncFuture::Deferred<RET> defer;
            QVector<QFuture<RET>> futures;
            int finishedCount;
            int index;
        };

        template <typename Sequence, typename Functor>
        inline auto schedule(QThreadPool*pool, Sequence sequence, Functor functor) -> Scheduler<Sequence, Functor>* {
            return new Scheduler<Sequence, Functor>(pool, sequence, functor);
        }


    } // End of Private namespace

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

    template <typename Sequence, typename Functor>
    inline auto mapped(QThreadPool*pool, Sequence input, Functor func) -> QFuture<typename Private::function_traits<Functor>::result_type>{
        auto scheduler = Private::schedule(pool, input, func);
        return scheduler->future();
    }

    template <typename Sequence, typename Functor>
    inline auto blockingMapped(QThreadPool*pool, Sequence input, Functor func) -> QList<typename Private::function_traits<Functor>::result_type>{
        auto f = mapped(pool, input, func);
        await(f);
        return f.results();
    }

}

