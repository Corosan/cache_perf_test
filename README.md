# cache_perf_test

Trivial memory access performance test for observing CPU caches effects.

The goal of this test was to get flag steps on a memory access graph depending on
a working set size. Which is described in a good article [What Every Programmer
Should Know About Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf).

But the only result which I've could be able to reach is failing steps - here:
![graph1](https://github.com/Corosan/cache_perf_test/blob/master/graph1.png)
