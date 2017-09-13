AConcurrent
Enhance QtConcurrent via AsyncFuture
-------------------------------------

Installation
============

    qpm install net.efever.aconcurrent

API
===

**QFuture<R> AConcurrent::runOnMainThread(Functor functor)**

Run a function on main thread. If the current thread is main thread, it will be executed in next tick.

**QFuture<R> AConcurrent::mapped(Sequence sequence, Functor worker)**

- An implementation of mapped() that support lambda function
- It is cancelable

**QFuture<R> AConcurrent::blockingMapped(Sequence sequence, Functor worker)**

**Queue<T> AConcurrent::queue(Functor worker)**

**void AConcurrent::debounce(context, key, future, functor)**

**AConcurrent::await(future)**

Wait until the input future is finished while keeping the event loop running.
