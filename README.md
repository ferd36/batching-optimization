# Batching Optimization 

<center>

|                              Warning                             |
|:----------------------------------------------------------------:|
| Your mileage will vary depending  on the details of your system! |

</center>

Experiment description
----------------------
In this experiment, we study the "batch optimization", that maximizes RAM bandwidth usage, by issuing a lot of
load commands all at once (for a batch), then letting the processor run a "payload" of other instructions.
Essentially, since all the loads from main memory are done in parallel, it is faster than issuing a load,
processing an instruction, issuing another load, processing another instruction...

We randomly access values in a big, statically allocated array (that doesn't fit in L3), and perform various amounts
of processing on those values. "Batching" consists in grouping the lookup and processing of values into "batches" of
small size. We want to understand what is the optimal batch size, and how the "batching optimization" depends on the
amount of work done on each value.

The setup that produced the results in the folder linux.results is detailed here.
Compilation was also verified with clang.

Data file format
----------------
The file name indicates the "payload": identity, payload, p1, p2... Refer to batch_optimization.cpp for details.
Each file line has the following format:
>     <algorithm name (2 words)> <batch_size (1 integer)> <latency1 (integer, microseconds)> <latency2> ...

Setup information
-----------------

>     Hardware: Xeon-5 2620 v2
>     OS: linux 6.6
>     Compiler: gcc 4.8.3.4
>     Language: c++ 11
>     Compilation flags: -std=c++11 -DNDEBUG -O3 -funroll-all-loops
>     Time unit: microseconds
>     Hash function: fast hash 64 bits
>     Memory alignment: valloc
>     Data type size (int): 4 bytes
>     Data size: M = 1073741824 bytes
>     Number of data lookups per repetition: N = 1048576
>     Number of repetitions: n_reps = 100
>     Other: all runs with taskset -c 7 (pinned to CPU 7)

Hardware details (lscpu)
------------------------

>     Architecture:          x86_64
>     CPU op-mode(s):        32-bit, 64-bit
>     Byte Order:            Little Endian
>     CPU(s):                12
>     On-line CPU(s) list:   0-11
>     Thread(s) per core:    2
>     Core(s) per socket:    6
>     Socket(s):             1
>     NUMA node(s):          1
>     Vendor ID:             GenuineIntel
>     CPU family:            6
>     Model:                 62
>     Stepping:              4
>     CPU MHz:               1200.000
>     BogoMIPS:              4190.02
>     Virtualization:        VT-x
>     L1d cache:             32K
>     L1i cache:             32K
>     L2 cache:              256K
>     L3 cache:              15360K
>     NUMA node0 CPU(s):     0-11
