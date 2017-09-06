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

**Queue<T> AConcurrent::pipeline(Functor worker)**

Create a pipeline to run a sequence of tasks, but only one task will be executed at a time.

**void AConcurrent::debounce(context, key, future, functor)**
