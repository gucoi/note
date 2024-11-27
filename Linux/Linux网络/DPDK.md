## 优点
1. 通过 uio 方式绕过 linux kernel，不会带来数据流在 linux kernel 中的多次拷贝，中断处理的消耗
2. UIO + mmap 实现零拷贝
3. UIO + PMD 减少中断和CPU上下文切换
4. 大页访问减少 tlb miss

## UIO 简介

![[Pasted image 20240910152246.png]]
设备驱动的主要任务有两个
* 存取设备的内存
* 处理设备产生的中断
上图中 UIO 核心实现了 `mmap()`, 对于处理设备产生的中断，内核空间有一小部分代码用来处理中断，用户空间通过 `read()` 接口 `/dev/uioX` 来获取中断。

## DPDK UIO 机制

旁路 linux kernel，将所有报文处理的工作都在用户空间完成。

![[Pasted image 20240910160315.png]]
左边是传统数据包获取方式
	网卡->kernel 驱动->kernel tcp/ip 协议栈 -> socket 接口 -> 业务
右边是DPDK方式，基于 UIO
	 网卡-> DPDK轮询 -> DPDK 基础库 -> 业务
## DPDK 零拷贝

通过 DMA （Direct Memory Access）将网卡数据拷贝至主存 (RX buffer) 随后使用 `mmap()` 方式将数据映射到用户空间，使得用户可以直接访问Rx buffer 中的数据，从而实现了零拷贝。

## PMD

DPDK 的 UIO 驱动屏蔽了硬件发出中断，然后在用户态采用主动轮询的方式，这种模式被称为 PMD (Poll Mode Driver)
正常收包流程中，网卡在收到包之后都会发出中断信号，CPU 收到中断信号后，会停止正在运行的程序，保护断点地址和处理机当前状态，转入相应的中断服务程序并中断服务程序，完成后，继续执行中断前的程序。

DPDK 采用轮询的方式，直接访问 RX 和 TX 描述符而没有任何中断，以便在用户的应用程序中快速接收，处理和发送数据包，这样可以减少 CPU 频繁中断，切换上下文带来的消耗。

在网卡大流量时，DPDK 这种 PMD 方式会带来较大的性能提升，但是在流量小的时候，轮询会导致CPU 空转，从而导致 CPU 性能浪费和功耗问题。

**DPDK 还推出了 interrupt DPDK 模式**，**它的原理和 Linux 的NAPI 很像，就是没包可处理时进入睡眠，改为中断通知，并且可以和其他进程共享同个 CPU core，但是 DPDK 进程会有更高的调度优先级。**

运行在 PMD 模式下的core会处于用户态 CPU 100%的状态。
![[Pasted image 20240910172456.png]]

与DPDK相比，pf-ring（no zc）使用的是NAPI polling和应用层polling，而pf-ring zc与DPDK类似，仅使用应用层polling。

![[Pasted image 20240910162923.png]]
## DPDK 高性能代码实现
### HugePages 

Linux 系统默认采用的 4KB 作为页的大小，页越小，设备的内存越大，页表的开销就越大，页表的内存占用也更大。

在操作系统中引入 MMU （Memory Management Unit）后，CPU 读取内存的数据需要两次访问内存。第一次要查询页表将逻辑地址转换为物理地址，然后访问该物理地址读取数据或指令。

为了减少页数过多，页表过大而导致的查询时间过长的问题，便引入了 TLB （Translation lookaside Buffer）可翻译为地址转换缓冲器。TLB 是一个内存管理单元，一般存储在寄存器中，里面存储了当前最可能被访问到的一小部分页表项。

引入TLB后，CPU会先去TLB中寻址，查找相应页表项，如果TLB命中后，则不用再去 RAM 中进行寻址， TLB 存放在寄存器中要比 RAM 或者 disk 快很多。如果 TLB 查找失败则会从 RAM 中进行查找，然后将其更新到 TLB 中，以便下次从 TLB 中查询。

DPDK 采用 Huge pages，在 x86 -64 下支持 2MB-1GB 的页大小，大大降低了 页的总个数和页表大小，从而大大降低 TLB miss 的几率，提升 CPU 寻址性能。

### SN 

软件架构去中心化，尽量避免全局共享，带来全局竞争，失去横向扩展的能力。NUMA体系下不跨Node远程使用内存。

### SIMD

从最早的mmx/sse到最新的avx2，SIMD的能力一直在增强。DPDK采用批量同时处理多个包，再用向量编程，一个周期内对所有包进行处理。比如，memcpy就使用SIMD来提高速度。

SIMD在游戏后台比较常见，但是其他业务如果有类似批量处理的场景，要提高性能，也可看看能否满足。

### rte_ring 无锁队列
#### CAS (Compare and Swap) 

原子指令用来保障数据的一致性。指令有三个参数，当前内存值V，旧的预期值A，更新的值B，**当且仅当预期值A和内存值V相同时，将内存值修改为B并返回true，否则什么都不做，并返回 false。**

#### 创建一个 ring 对象

```cpp
@name ring 的 name 具有唯一性
@count ring 队列的长度必须是 2 的幂次方
@socket_id ring 位于的 socket
@flags 指定创建的 ring 的属性：单/多生产者 单/多消费者两者之间的组合 0 表示 使用默认属性(多生产者 多消费者) 不同的属性出入队的操作会有所不同
struct rte_ring* rte_ring_create(const char* name, unsigned count, 
								 int socket_id, unsigned flags)
```
#### 出入队

有不同的出入队方式（单、bulk、burst）都在 rte_ring.h 中。
例如：rte_ring_enqueue 和 rte_ring_dequeue 


#### rte_ring 结构体

```cpp
struct rte_ring {  
    /*  
     * Note: this field kept the RTE_MEMZONE_NAMESIZE size due to ABI  
     * compatibility requirements, it could be changed to RTE_RING_NAMESIZE  
     * next time the ABI changes  
     */  
    TAILQ_ENTRY(rte_ring) next;     /**< Next in list. */
    char name[RTE_MEMZONE_NAMESIZE];    /**< Name of the ring. */  
    int flags;                       /**< Flags supplied at creation. */  
    const struct rte_memzone *memzone;  
            /**< Memzone, if any, containing the rte_ring */  
  
    /** Ring producer status. */  
    struct prod {
        uint32_t watermark;      /**< Maximum items before EDQUOT. */  
        uint32_t sp_enqueue;     /**< True, if single producer. */  
        uint32_t size;           /**< Size of ring. */  
        uint32_t mask;           /**< Mask (size-1) of ring. */  
        // 生产者头尾指针，生产完成后都指向队尾
        volatile uint32_t head;  /**< Producer head. 预生产到地方*/  
        volatile uint32_t tail;  /**< Producer tail. 实际生产了的数量*/  
    } prod __rte_cache_aligned;  
  
    /** Ring consumer status. */  
    struct cons {  
        uint32_t sc_dequeue;     /**< True, if single consumer. */  
        uint32_t size;           /**< Size of the ring. */  
        uint32_t mask;           /**< Mask (size-1) of ring. */ 
        // 消费者头尾指针，生产完成后都指向队头
        volatile uint32_t head;  /**< Consumer head. cgm预出队的地方*/  
        volatile uint32_t tail;  /**< Consumer tail. 实际出队的地方*/  
#ifdef RTE_RING_SPLIT_PROD_CONS  
    } cons __rte_cache_aligned;  
#else  
    } cons;  
#endif  
  
#ifdef RTE_LIBRTE_RING_DEBUG  
    struct rte_ring_debug_stats stats[RTE_MAX_LCORE];  
#endif  
	// 队列中保存的所有对象
    void *ring[] __rte_cache_aligned;   /**< Memory space of ring starts here.  
                                         * not volatile so need to be careful  
                                         * about compiler re-ordering */  
};  

```

#### 入队函数

```cpp
static __rte_always_inline unsigned 
int __rte_ring_mp_do_enqueue(struct rte_ring *r, void * const *obj_table, 
							unsigned int n, enum rte_ring_queue_behavior behavior, 
							 unsigned int *free_space) { 
	uint32_t prod_head, prod_next; 
	uint32_t free_entries;

	....

	do {
		....
		// 用来作为多生产者 cas 竞争
		success = rte_atomic32_cmpset(&r->prod.head, prod_head,prod_next);
	} while(unlikely(success == 0))

	ENQUEUE_PTRS(); // 入队宏
	rte_smp_web(); // 设置 lcore 内存屏障

	while(unlikely(r->prod.tail != r->prod.head)) {
		// 同步对 prod 的修改
	}
}
```

##### ENQUEUE_PTRS 宏

```cpp
  
#define ENQUEUE_PTRS() do { \ 
const uint32_t size = r->prod.size; \ 
uint32_t idx = prod_head & mask; \ 
if (likely(idx + n < size)) { \
	// n & ((~(unsigned)0x3)) 取得是 n 最小的 4 的倍数
	for (i = 0; i < (n & ((~(unsigned)0x3))); i+=4, idx+=4) { \ 
		r->ring[idx] = obj_table[i]; \ 
		r->ring[idx+1] = obj_table[i+1]; \ 
		r->ring[idx+2] = obj_table[i+2]; \ 
		r->ring[idx+3] = obj_table[i+3]; \ 
	} \ 
	// n & 0x3 算的是 n / 4 之后的余数
	switch (n & 0x3) { \ 
		case 3: r->ring[idx++] = obj_table[i++]; \ 
		case 2: r->ring[idx++] = obj_table[i++]; \ 
		case 1: r->ring[idx++] = obj_table[i++]; \ 
	} \ 
} else { \ 
	for (i = 0; idx < size; i++, idx++)\ 
		r->ring[idx] = obj_table[i]; \ 
	for (idx = 0; i < n; i++, idx++) \ 
		r->ring[idx] = obj_table[i]; \ 
	} \ 
} while(0)
```

#### 出队函数
```cpp
 static inline int __attribute__((always_inline))  
 __rte_ring_mc_do_dequeue(struct rte_ring *r, void **obj_table,  
						 unsigned n, enum rte_ring_queue_behavior behavior)  {

	do {
		....
		// 原子地更新消费者头指针
		success = rte_atomic32_cmpset(&r->prod.head, prod_head,prod_next);
	} while(unlikely(success == 0))

	DEQUEUE_PTR();
	rte_smp_rmb();

	//	更新尾指针 注意内存顺序
	while (unlikely(r->cons.tail != cons_head)) { 
		rte_pause(); 
		 if (RTE_RING_PAUSE_REP_COUNT && 
			++rep == RTE_RING_PAUSE_REP_COUNT) { 
			rep = 0; 
			sched_yield(); 
		} 
		r->cons.tail = cons_next;
	}
}
```
##### DEQUEUE_PTR 宏

```cpp

#define DEQUEUE_PTRS() do { \ 
	uint32_t idx = cons_head & mask; \ 
	const uint32_t size = r->cons.size; \ 
	if (likely(idx + n < size)) { \ 
		for (i = 0; i < (n & (~(unsigned)0x3)); i+=4, idx+=4) {\ 
			obj_table[i] = r->ring[idx]; \ 
			obj_table[i+1] = r->ring[idx+1]; \ 
			obj_table[i+2] = r->ring[idx+2]; \ 
			obj_table[i+3] = r->ring[idx+3]; \ 
		} \ 
		switch (n & 0x3) { \ 
			case 3: obj_table[i++] = r->ring[idx++]; \ 
			case 2: obj_table[i++] = r->ring[idx++]; \ 
			case 1: obj_table[i++] = r->ring[idx++]; \ 
		} \ 
	} else { \ 
		for (i = 0; idx < size; i++, idx++) \ 
			obj_table[i] = r->ring[idx]; \ 
		for (idx = 0; i < n; i++, idx++) \ 
			obj_table[i] = r->ring[idx]; \ 
	} \
} while (0)
```

### 不使用慢速 API

这里需要重新定义一下慢速API，比如说gettimeofday，虽然在64位下通过[vDSO](http://man7.org/linux/man-pages/man7/vdso.7.html)已经不需要陷入内核态，只是一个纯内存访问，每秒也能达到几千万的级别。但是，不要忘记了我们在10GE下，每秒的处理能力就要达到几千万。所以即使是gettimeofday也属于慢速API。DPDK提供[Cycles](https://doc.dpdk.org/api/rte__cycles_8h.html)接口，例如rte_get_tsc_cycles接口，基于HPET或TSC实现。


在x86-64下使用RDTSC指令，直接从寄存器读取，需要输入2个参数，比较常见的实现：

```C++
static inline uint64_t rte_rdtsc(void) { 
	uint32_t lo, hi;
	__asm__ __volatile__ ( 
		"rdtsc" : "=a"(lo), "=d"(hi) 
		); 
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32); 
	}
```

DPDK 中的相关实现
```C++
static inline uint64_t
rte_rdtsc(void)
{
	union {
		uint64_t tsc_64;
		struct {
			uint32_t lo_32;
			uint32_t hi_32;
		};
	} tsc;

	asm volatile("rdtsc" :
		     "=a" (tsc.lo_32),
		     "=d" (tsc.hi_32));
	return tsc.tsc_64;
}

```

**巧妙的利用C的union共享内存，直接赋值，减少了不必要的运算。但是使用tsc有些问题需要面对和解决**

1. CPU亲和性，解决多核跳动不精确的问题
    
2. 内存屏障，解决乱序执行不精确的问题
    
3. 禁止降频和禁止Intel Turbo Boost，固定CPU频率，解决频率变化带来的失准问题

### 编译执行优化

#### 分支预测

现代CPU通过[pipeline](https://en.wikipedia.org/wiki/Instruction_pipelining)、[superscalar](https://zh.wikipedia.org/wiki/%E8%B6%85%E7%B4%94%E9%87%8F)提高并行处理能力，为了进一步发挥并行能力会做[分支预测](https://zh.wikipedia.org/wiki/%E5%88%86%E6%94%AF%E9%A0%90%E6%B8%AC%E5%99%A8)，提升CPU的并行能力。遇到分支时判断可能进入哪个分支，提前处理该分支的代码，预先做指令读取编码读取寄存器等，预测失败则预处理全部丢弃。我们开发业务有时候会非常清楚这个分支是true还是false，那就可以通过人工干预生成更紧凑的代码提示CPU分支预测成功率。

```C
#pragma once

#if !__GLIBC_PREREQ(2, 3)
#    if !define __builtin_expect
#        define __builtin_expect(x, expected_value) (x)
#    endif
#endif

#if !defined(likely)
#define likely(x) (__builtin_expect(!!(x), 1))
#endif

#if !defined(unlikely)
#define unlikely(x) (__builtin_expect(!!(x), 0))
#endif
```

#### CPU Cache 预取

Cache Miss的代价非常高，回内存读需要65纳秒，可以将即将访问的数据主动推送的CPU Cache进行优化。比较典型的场景是链表的遍历，链表的下一节点都是随机内存地址，所以CPU肯定是无法自动预加载的。但是我们在处理本节点时，可以通过CPU指令将下一个节点推送到Cache里。

```C++
static inline void rte_prefetch0(const volatile void *p)
{
	asm volatile ("prefetcht0 %[p]" : : [p] "m" (*(const volatile char *)p));
}
```

```C++
#if !defined(prefetch)
#define prefetch(x) __builtin_prefetch(x)
#endif
```

#### 内存对齐

避免结构体成员跨Cache Line，需2次读取才能合并到寄存器中，降低性能。结构体成员需从大到小排序和以及强制对齐。
```C++
#define __rte_packed __attribute__((__packed__))
```
多线程场景下写产生[False sharing](https://en.wikipedia.org/wiki/False_sharing)，造成Cache Miss，结构体按Cache Line对齐
```C++
#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#ifndef aligined
#define aligined(a) __attribute__((__aligned__(a)))
#endif
```
#### 常量优化
常量相关的运算的编译阶段完成。比如C++11引入了constexp，比如可以使用GCC的__builtin_constant_p来判断值是否常量，然后对常量进行编译时得出结果。举例网络序主机序转换

```C++
#define rte_bswap32(x) ((uint32_t)(__builtin_constant_p(x) ?		\
				   rte_constant_bswap32(x) :		\
				   rte_arch_bswap32(x)))
```

其中 rte_constant_bswap32 的实现
```C++
#define RTE_STATIC_BSWAP32(v) \
	((((uint32_t)(v) & UINT32_C(0x000000ff)) << 24) | \
	 (((uint32_t)(v) & UINT32_C(0x0000ff00)) <<  8) | \
	 (((uint32_t)(v) & UINT32_C(0x00ff0000)) >>  8) | \
	 (((uint32_t)(v) & UINT32_C(0xff000000)) >> 24))
```

#### 使用 CPU 指令

现代CPU提供很多指令可直接完成常见功能，比如大小端转换，x86有bswap指令直接支持了

```C++
static inline uint64_t rte_arch_bswap64(uint64_t _x)
{
	register uint64_t x = _x;
	asm volatile ("bswap %[x]"
		      : [x] "+r" (x)
		      );
	return x;
}
```

## VFIO-PCI 驱动

VFIO（Virtual Function I/O）驱动框架是一个用户态驱动框架，在intel平台它充分利用了VT-d等技术提供的DMA Remapping和Interrupt Remapping特性， 在保证直通设备的DMA安全性同时可以达到接近物理设备的I/O的性能。VFIO是一个可以安全的把设备I/O、中断、DMA等暴露到用户空间，用户态进程可以直接使用VFIO驱动访问硬件，从而可以在用户空间完成设备驱动的框架。在内核源码附带的文档<<Documentation/vfio.txt>>中对VFIO描述的相关概念有比较清楚的描述，也对VFIO的使用方法有清楚的描述。

### IOMMU

IOMMU是一个硬件单元，它可以把设备的IO地址映射成虚拟地址，为设备提供页表映射，设备通过IOMMU将数据直接DMA写到用户空间。
![[Pasted image 20241028095731.png]]

### Device

Device 是指要操作的硬件设备，这里的设备需要从IOMMU拓扑的角度来看。如果device 是一个硬件拓扑上是独立那么这个设备构成了一个IOMMU group。如果多个设备在硬件是互联的，需要相互访问数据，那么这些设备需要放到一个IOMMU group 中隔离起来。

### Group

Group 是IOMMU 能进行DMA隔离的最小单元。一个group 可以有一个或者多个device。

### container

container 是由多个group 组成。虽然group 是VFIO 的最小隔离单元，但是并不是最好的分割粒度，比如多个group 需要共享一个页表的时候。将多个group 组成一个container来提高系统的性能，也方便用户管理。一般一个进程作为一个container。

