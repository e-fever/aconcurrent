AConcurrent
Enhance QtConcurrent via AsyncFuture
-------------------------------------

Installation
============

    qpm install net.efever.aconcurrent

API
===

**QFuture<R> AConcurrent::runOnMainThread(Functor functor)**

**QFuture<R> AConcurrent::mapped(Sequence sequence, Functor worker)**

- An implementation that support lambda function

**QFuture<R> AConcurrent::blockingMapped(Sequence sequence, Functor worker)**

**Queue<T> AConcurrent::queue(Functor worker)**

**void AConcurrent::debounce(context, key, future, functor)**
