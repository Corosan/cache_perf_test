# cache_perf_test

## 2019
Trivial memory access performance test for observing CPU caches effects.

The goal of this test was to get flag steps on a memory access graph depending on a working set
size. Which is described in a good article [What Every Programmer Should Know About
Memory](https://people.freebsd.org/~lstewart/articles/cpumemory.pdf).

But the only result which I've could be able to reach is failing steps - here:
![graph1](graph-20190419-intel_i3_2367M.png)

## 2022
Slightly updated the testing code and get more clear result. Assuming that both experiments were run
at the same good old Intel(R) Core(TM) i3-2367M CPU @ 1.40GHz, I can see access time steps roughly
around CPU caches sizes. Let me remind you that the CPU has L1 - 64Kb, L2 - 256 Kb, L3 - 3Mb. The
graph shows results of running the test on one specific CPU core (cpuid=1). OS was isolated on
another cpuid=0 during the test.
![graph2](graph-20220209-intel_i3_2367M.png)

What has been changed since that good old times? Recently I've bought mass market HP notebook with
AMD Ryzen 5 5500U and repeat the test on it. The CPU has L1 - 64Kb, L2 - 512Kb, L3 - 8Mb (from
https://www.techpowerup.com/cpu-specs/ryzen-5-5500u.c2372).  Strange, but official link
(https://www.amd.com/ru/products/apu/amd-ryzen-5-5500u#product-specs) says nothing about L1, stands
that L2 - 3Mb, and L3 - 8Mb. The graph shows results of running the test two times on specific CPU
cores (cpuid=1 then cpuid=2). OS wasn't isolated.
![graph3](graph-20220206-amd_ryzen_5_5500U.png)

It looks like central memory access time became even worse than 10 years ago (please, don't blame
me that I compare different OEM manufacturers - Asus and HP, and different CPU manufacturers). But only
if we cound CPU cycles, not real nanoseconds.

One more measured CPU, provided by my friend, is shown on the next graph. It's an Intel(R) Core(TM)
i3-8130U CPU @ 2.20GHz. It has L1 - 64Kb, L2 - 512Kb, L3 - 4Mb. To tell the truth, I don't see any
observable difference between L2 and L3 here. And again, caches stop working slightly earlier than
working set reachs corresponding size.
![graph4](graph-20220209-intel_i3_8130U.png)

