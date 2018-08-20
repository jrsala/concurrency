# concurrency
Simple concurrent C++11 data structures and utilities with example code.

## To do
*   In the queues, implement `try_enqueue` and `try_dequeue`, without which the queues do not allow for *truly, formally* lock-free or wait-free algorithms.
*   Factor out the "ring buffer" part of the queues to avoid duplication.
*   Write parameterized benchmarks, notably with
    *   structure parameter variations: capacity, element size
    *   usage pattern variations: number of producers and consumers, faster production or consumption (resp. queue mostly full or mostly empty)
    *   cache warm-up
    *   comparison with "naive" implementations, for example that use coarse locking
