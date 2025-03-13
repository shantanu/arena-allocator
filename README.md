# Arena Allocator in C++ (⚡200x+ faster than GNU Malloc!)
This repository contains a custom Arena allocator (bump allocator) implemented in C++ to benchmark memory allocation performance with `std::vector` against the default GNU heap allocator. 

🟢 [Run it on Godbolt!](https://godbolt.org/z/jj1Mbc71o) 🟢

🏃 [View the benchmarks on QuickBench!](https://quick-bench.com/q/hnEZC5GC-9UoXK6aMM5VZ-66CKY) 🏃

# 🔑 Key Functions and Classes:
- Overloaded global `operator new` and `operator delete` for debugging. These print when heap memory is allocated. 
- `Arena`: Uses a `std::array<std::byte, N>` as a stack-allocated buffer, and a pointer to the next-allocated byte. If there is not enough space in the buffer, requests are forwarded to the global `operator new` (`std::malloc`). Memory is only freed if it is the most recent allocation. Otherwise, you have to wait until the entire arena is freed (sorry!).

- Alignment guarantee: Memory is guaranteed to be aligned to the maximum scalar type alignment guarantee (`std::max_align_t`). This matches `std::malloc`.

- `ShortAlloc`: A custom STL-compatible allocator using the Arena. Keeps a pointer to the arena, passed in during construction. Note that the arena pointer is a raw pointer and does not own the underlying memory so it's up to the user to maintain this!

- `TestClass`: A wrapper around `std::array<std::byte, N>`, which allows us to test the allocator performance on varying sized objects. Copy and move operations don't do anything, because I only want to measure allocate/deallocate performance and not copy/move performance.  

# ⏱️ Performance Benchmarks 
Using `std::vector` to `emplace_back` 100 objects of the listed size in an arena that is 1028*data size (enough for all vector reallocations). The vector does not reserve capacity, so it allocates double capacity when it fills up and the next `emplace_back` call occurs. Performance is measured on online QuickBench.

> [!NOTE]
> Disclaimer: "The benchmark runs on a pool of AWS machines whose load is unknown and potentially next to multiple other benchmarks. Any duration it could output would be meaningless. [...] Quick Bench can, however, give a reasonably good comparison between two snippets of code run in the same conditions." 


| Object Size (B) | Arena Size (B)      | Stack Time (ns) | Heap Time (ns) | Stack Time Faster        | QuickBench Link |
|----------------|--------------------|------------|-----------|--------------------------|-----------------|
| 8             | 8K                  | 179        | 401       | Stack 2.2x faster        | [Link](https://quick-bench.com/q/FSVMOb4MG32-acROZFLstV0xmbc) |
| 16            | 16K                 | 187        | 486       | Stack 2.6x faster        | [Link](https://quick-bench.com/q/EHJp6Uk8AfUtMWXmkRzLwU8nO6g) |
| 32            | 32K                 | 186        | 540       | Stack 2.9x faster        | [Link](https://quick-bench.com/q/mJGqAyIZDObgF34V8A2NWLKAIIg) |
| 64            | 64K                 | 189        | 588       | Stack 3.1x faster        | [Link](https://quick-bench.com/q/j27FuXFO9GWcSgvL8DGE-F01RJ4) |
| 128           | 128K                | 189        | 665       | Stack 3.5x faster        | [Link](https://quick-bench.com/q/0LgLAzs_9k9AYHwMDe9CjvBSF08) |
| 256           | 256K                | 189        | 744       | Stack 3.9x faster        | [Link](https://quick-bench.com/q/GOgxN2sY_svtHwzDqRKhK2qC4Uo) |
| 512           | 512K                | 189        | 824       | **Stack 4.4x faster**        | [Link](https://quick-bench.com/q/EfeGYpXX_9xcR4vIvLesMp5CbFQ) |
| 1K            | 1M                  | 188        | 17,543    | 🔴 **Stack 93x faster (!)**  | [Link](https://quick-bench.com/q/hnEZC5GC-9UoXK6aMM5VZ-66CKY) |
| 2K            | 2M                  | 188        | 27,401    | Stack 150x faster        | [Link](https://quick-bench.com/q/lhsE7jALIXMfwp5BW4tzn9yLO6U) |
| 4K            | 4M                  | 187        | 36,372    | Stack 190x faster        | [Link](https://quick-bench.com/q/C4nXuQGAnEEwD8UWKUo8ZhH8HDM) |
| 8K            | 8M (whole stack!)    | 188        | 44,346    | Stack 230x faster        | [Link](https://quick-bench.com/q/5BRGLEjOHhZYnX6Wbojy8xDAb0g) |


There is a huge gap in performance between 512B and 1K object size, where the performance of the GNU allocator drops significantly. To understand why (at least approximately, without having access to the hardware), we can dig into the GNU malloc documentation.

# 🐴 Why the Huge Jump in Latency at 1KB Objects?
In the GNU Malloc allocator, there are basically 3 categories of allocation: 
1. Fast bins (up to 160B)
2. Normal heap allocation
3. Large allocations (above 128KB)

The boundaries of these are implementation defined, but these numbers are reasonable.

The fast bins are pre-allocated, fixed-sized bins, each with a bit designating whether or not it is free. Since the allocator doesn't need to traverse a free list and deal with coalescing blocks of varied size, allocations from the fast bins are pretty fast. Having this memory pre-allocated on the heap or in thread-local storage (called tcache) also makes it faster. 

The medium-sized heap allocation happens through a free-list defined on an Arena, backed by the heap. This is your "standard" allocation request, barring rules for cache affinity and thread safety. 

The large allocations call `mmap` and directly request some memory from the OS. This is especially slow, since the `mmap` system call has to do some page table manipulations, and accessing this memory for the first time will cause a page fault. 

[The GNU Malloc documentation](https://sourceware.org/glibc/wiki/MallocInternals) contains all the nitty gritty details.

> [!IMPORTANT]
> I believe the *main* reason for the spike in latency is because when the `std::vector` doubled to 128 objects each sized 1K, we crossed the 128KB allocation threshold into `mmap` territory. The ensuing page fault(s) caused by accessing that region of memory made the allocator crawl. 
>
> Also, with 1KB sized objects, only 4 objects can fit into a page (which is 4KB on Linux). We also didn't do anything to align the objects to the page boundaries. This means that even doing sequential access of this memory can cause TLB misses, cache misses, and page faults on first load. 

The numbers line up pretty well with [Jeff Dean's Latency Numbers](https://gist.github.com/jboner/2841832). Main memory reference is ~100ns, which makes sense for Stack Time. SSD random access is noted around 150us, but sequential access and 10+ years of hard drive improvements can account for the difference on Heap Time. 

(Aside: I picked a horse emoji because it's the most similar thing I could find to a gnu, hope you appreciate it!)

# ✅ Takeaways
- Stack-based allocators are REALLY fast. They're essentially constant time, especially compared to when heap allocators get into the large object category and start making `mmap` system calls. 

- The tradeoff is that since the stack is typically small (on Linux it's only 8MB per thread), you can only do limited things with this.

- You can't free the memory within the Arena. You have to wait and free the entire buffer at once. This is only good for certain workloads and not great for a general capacity allocator. 
