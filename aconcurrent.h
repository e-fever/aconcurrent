#pragma once

#include <QFuture>
#include <QtConcurrent>
#include <QThreadPool>
#include <QTimer>
#include <asyncfuture.h>

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
        void completeDefer(AsyncFuture::Deferred<R> defer, const QVector<QFuture<R>> &futures) {
            QList<R> res;
            for (int i = 0 ; i < futures.size() ;i++) {
                res << futures[i].result();
            }
            defer.complete(res);
        }

        template <>
        void completeDefer<void>(AsyncFuture::Deferred<void> defer, const QVector<QFuture<void>>& futures) {
            Q_UNUSED(futures);
            defer.complete();
        }

    }

    template <typename T, typename F>
    auto mapped(QThreadPool*pool, QList<T> input, F func) -> QFuture<typename Private::function_traits<F>::result_type>{
        typedef typename Private::function_traits<F>::result_type RET;

        QVector<QFuture<RET> > futures;
        futures.resize(input.size());

        auto combinator = AsyncFuture::combine();
        AsyncFuture::Deferred<RET> defer = AsyncFuture::deferred<RET>();

        for (int i = 0; i < input.size(); i++) {
            auto f = QtConcurrent::run(pool, func, input[i]);
            combinator << f;
            futures[i] = f;
        }

        combinator.subscribe([=]() {
            Private::completeDefer(defer, futures);
        });

        return defer.future();
    }

    // Wait for a QFuture to be finished without blocking
    template <typename T>
    void waitForFinished(QFuture<T> future, int timeout = -1) {
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

}

