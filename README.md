CLHT
====

CLHT is a very fast and scalable concurrent, resizable hash table. It comes in two (main) variants, a lock-based and a lock-free.
The main idea behind CLHT is to, if possible, use cache-line-sized buckets so that update operations (`put` and `remove`) complete with at most one cache-line transfer. Furthermore, CLHT is based on two ideas:
  1. operations parse (go through) the elements of a bucket taking a snapshot of each key/value pair,
  2. `get` operations are read-only (i.e., they do not perform any stores) and are wait-free (i.e., they never have to restart), and 
  2. update operations perform in place updates, hence they do not require any memory allocation.

The result is very low latency operations. For example, a single thread on an Intel Ivy Bridge processor achieves the following latencies (in cycles):
  * srch-suc: 58
  * srch-fal: 21
  * insr-suc: 74
  * insr-fal: 62
  * remv-suc: 91
  * remv-fal: 19

This translates to more than 50 Mega-operations/sec in our benchmarks (the actual throughput of CLHT is even higher, but the benchmarking code takes a significant portion of the execution).

In concurrent settings, CLHT gives more than 2 Billion-operations/sec with read-only executions on 40 threads on a 2-socket Ivy Bridge, more than 1 Billion-operations/sec with 1% updates, 900 Mega-operations/sec with 10%, and 300 Mega-operations/sec with update operations only.


* Website             : http://lpd.epfl.ch/site/ascylib
* Author              : Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
* Related Publications: CLHT was developed for:
  Asynchronized Concurrency: The Secret to Scaling Concurrent Search Data Structures,
  Tudor David, Rachid Guerraoui, Vasileios Trigonakis (alphabetical order),
  ASPLOS '14

You can also find a detailed description of the algorithms and corectness sketches/proofs in the following technical report: ...

CLHT-LB (Lock-based version)
----------------------------

CLHT-LB synchronizes update operations (`put` and `remove`) with locks. In short, an update first checks whether the operation can be successful (i.e., if the given key exists for a removal, or if the key is not already in there for an insertion), and if it is, it grabs the corresponding lock and performs the update.

If a `put` find the bucket full, then either the bucket is expanded (linked to another bucket), or the hash table is resized (for the variants of CLHT-LB that support resizing).

We have implemented the following variants of CLHT-LB:
  1. `clht_lb_res`: the default version, supports resizing. 
  2. `clht_lb`: as (1), but w/o resizing.
  3. `clht_lb_res_no_next`: as (1), but the hash table is immediatelly resized when there is no space for a `put`.
  4. `clht_lb_packed`: as (2), but elements are "packed" in the first slots of the bucket. Removals move elements to avoid leaving any empty slots.
  5. `clht_lb_linked`: as (1), but the buckets are linked to their next backets (b0 to b1, b1 to b2, ...), so that if there is no space in a bucket, the next one is used. If the hash table is too full, it is resized.
  6. `clht_lb_lock_ins`: as (2), but `remove` operations do not lock. The work using `compare-and-swap` operations.

CLHT-LF (Lock-free version)
---------------------------

CLHT-LF synchronizes update operations (`put` and `remove`) in a lock-free manner. Instead of locks, CLHT-LF uses the `snapshot_t` structure. `snapshot_t` is an 8-byte structure with two fields: a version number and an array of bytes (a map). The map is used to indicate whether a key/value pair in the bucket is valid, invalid, or is being inserted. The version field is used to indicate that the `snapshot_t` object has been changed by a concurrent update.

We have implemented the following variants of CLHT-LF:
  1. `clht_lf_res`: the default version, supports resizing.
  2. `clht_lf`: as (1), but w/o resizing. NB. CLHT-LF cannot expand/link bucket, thus, if there is not enough space for a `put`, the operation might never complete.
  3. `clht_lf_only_map_rem`: as (2), but `remove` operations do not increment the `snapshot_t`'s version number.


Installation
------------

Using CLHT
----------

Details
-------
