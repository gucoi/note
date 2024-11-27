# Linux线程同步


当我们深入探索 Linux 操作系统的奥秘时，线程同步无疑是一个至关重要且充满挑战的领域。在复杂的 Linux 系统中，多个线程常常同时运行，它们如同在一个繁忙的数字工坊中协同工作的工匠。然而，若没有有效的同步机制，这些线程可能会相互干扰、冲突，导致数据不一致、系统不稳定甚至崩溃。

线程同步就像是为这个工坊制定的一套精准的规则和协调机制。它确保不同的线程在合适的时机进行操作，有序地共享资源，避免混乱和错误。理解 Linux 线程同步，不仅能让我们更好地优化程序性能，提高系统的可靠性，还能深入洞察操作系统底层的工作原理。本文将深入探讨 Linux 线程同步的方法和实现机制，帮助读者更好地理解和应用线程同步技术。

## 一、概述

  

Linux 线程同步是多线程编程中的关键环节，确保多个线程在访问共享资源时能够协调一致，避免出现竞态条件和数据不一致的情况。在 Linux 系统中，线程同步是非常重要的概念。线程同步是指多个线程之间为了协调彼此的操作而采取的一种机制。在多线程编程中，由于线程是并发执行的，一些资源（如共享变量）可能会被多个线程同时访问，导致竞争条件。线程同步可以确保在多线程环境下对共享资源的访问是有序的，从而避免竞争条件的发生。

在操作系统中，线程同步是非常重要的概念，它确保多个线程能够按照预期的顺序执行，并且对共享资源的访问是安全的。在多线程编程中，多个线程可能会同时访问共享的数据或资源。如果没有适当的同步机制，可能会出现竞态条件、死锁、饥饿等问题。为了解决这些问题，Linux 提供了多种线程同步方法。

Linux 实现线程同步主要有如下三个手段：mutex（互斥变量）/cond（条件变量）/sem（信号量）。互斥变量是实现线程同步的一个重要手段，通过对互斥变量加解锁机制，可以实现线程的同步。一个线程对一个互斥变量加锁，而其他任何一个线程想再次对互斥变量加锁时都会被阻塞，直到加锁线程释放互斥锁。

条件变量是实现线程同步的另一个重要手段，可以使用条件变量等待另一个线程的结束，条件变量一般痛互斥变量一起使用。条件变量的一个重要表象就是一个线程等待另外的线程发送信号。除了互斥锁和条件变量外，Linux 还支持其他线程同步机制，如信号量、屏障等。这些机制可以根据实际需求进行选择，以确保线程之间的协调和同步。

在使用这些线程同步机制时，需要注意不仅要保证正确性，还要避免死锁等问题。在编写多线程程序时，正确使用线程同步机制是至关重要的。如果不正确地处理线程之间的同步关系，可能会导致程序出现各种问题，如数据错误、死锁等。因此，在进行多线程编程时，开发人员需要充分了解线程同步的概念和机制，并在代码中妥善处理线程之间的同步关系。

### 1.1名词解释

CPU：本文中的CPU都是指逻辑CPU。

UP：单处理器(单CPU)。

SMP：对称多处理器(多CPU)。

线程、执行流：线程的本质是一个执行流，但执行流不仅仅有线程，还有ISR、softirq、tasklet，都是执行流。本文中说到线程一般是泛指各种执行流，除非是在需要区分不同执行流时，线程才特指狭义的线程。

并发、并行：并发是指线程在宏观上表现为同时执行，微观上可能是同时执行也可能是交错执行，并行是指线程在宏观上是同时执行，微观上也是同时执行。

伪并发、真并发：伪并发就是微观上是交错执行的并发，真并发就是并行。UP上只有伪并发，SMP上既有伪并发也有真并发。

临界区：访问相同数据的代码段，如果可能会在多个线程中并发执行，就叫做临界区，临界区可以是一个代码段被多个线程并发执行，也可以是多个不同的代码段被多个线程并发执行。

同步：首先线程同步的同步和同步异步的同步，不是一个意思。线程同步的同步，本文按照字面意思进行解释，同步就是统一步调、同时执行的意思。

线程同步现象：线程并发过程中如果存在临界区并发执行的情况，就叫做线程同步现象。

线程防同步机制：如果发生线程同步现象，由于临界区会访问共同的数据，程序可能就会出错，因此我们要防止发生线程同步现象，也就是防止临界区并发执行的情况，为此我们采取的防范措施叫做线程防同步机制。

### 1.2线程同步与防同步

为什么线程同步叫线程同步，不叫线程防同步，叫线程同步给人的感觉好像就是要让线程同时执行的意思啊。但是实际上线程同步是让临界区不要并发执行的意思，不管你们俩谁先执行，只要错开，谁先谁后执行都可以。所以本文后面都采用线程防同步机制、防同步机制等词。

我小时候一直有个疑惑，感冒药为啥叫感冒药，感冒药是让人感冒的药啊，不是应该叫治感冒药才对吗，治疗感冒的药。后来一想，就没有让人感冒的药，所以感冒药表达的就是治疗感冒的药，没必要加个治字。但是同时还有一种药，叫老鼠药，是治疗老鼠的药吗，不是啊，是要毒死老鼠的药，因为没有人会给老鼠治病。不过我们那里也有把老鼠药叫做害老鼠药的，加个害字，意思更明确，不会有歧义。

因此本文用了两个词，同步现象、防同步机制，使得概念更加清晰明确。

说了这么多就是为了阐述一个非常简单的概念，就是不能同时操作相同的数据，因为可能会出错，所以我们要想办法防止，这个方法我们把它叫做线程防同步。

还有一个词是竞态条件(race condition)，很多关于线程同步的书籍文档中都有提到，但是我一直没有理解是啥意思。竞态条件，条件，线程同步和条件也没啥关系啊；竞态，也不知道是什么意思。再看它的英文，condition有情况的意思，race有赛跑、竞争的意思，是不是要表达赛跑情况、竞争现象，想说两个线程在竞争赛跑，看谁能先访问到公共数据。我发现没有竞态条件这个词对我们理解线程同步问题一点影响都没有，有了这个词反而不明所以，所以我们就忽略这个词。

## 二、线程防同步方法

在我们理解了同步现象、防同步机制这两个词后，下面的内容就很好理解了。同步现象是指同时访问相同的数据，那么如何防止呢，我不让你同时访问相同的数据不就可以了嘛。因此防同步机制有三大类方法，分别是从时间上防同步、从空间上防同步、事后防同步。从时间上和空间上防同步都比较好理解，事后防同步的意思是说我先不防同步，先把临界区走一遍再说，然后回头看刚才有没有发生同步现象，如果有的话，就再重新走一遍临界区，直到没有发生同步现象为止。

### 2.1 时间上防同步

我不让你们同时进入临界区，这样就不会同时操作相同的数据了，有三种方法：

⑴原子操作

对于个别特别简单特别短的临界区，CPU会提供一些原子指令，在一条指令中把多个操作完成，两个原子指令必然一个在前一个在后地执行，不可能同时执行。原子指令能防伪并发和真并发，适用于UP和SMP。

⑵加锁

对于大部分临界区来说，代码都比较复杂，CPU不可能都去实现原子指令，因此可以在临界区的入口处加锁，同一时间只能有一个线程进入，获得锁的线程进入，在临界区的出口处再释放锁。未获得锁的线程在外面等待，等待的方式有两种，忙等待的叫做自旋锁，休眠等待的叫做阻塞锁。根据临界区内的数据读写操作不同，锁又可以分为单一锁和读写锁，单一锁不区分读者写者，所有用户都互斥；读写锁区分读者和写者，读者之间可以并行，写者与读者、写者与写者之间是互斥的。自旋锁有单一锁和读写锁，阻塞锁也有单一锁和读写锁。自旋锁只能防真并发，适用于SMP；休眠锁能防伪并发和真并发，适用于UP和SMP。

⑶临时禁用伪并发

对于某些由于伪并发而产生的同步问题，可以通过在临界区的入口处禁用此类伪并发、在临界区的出口处再恢复此类伪并发来解决。这种方式显然只能防伪并发，适用于UP和SMP上的单CPU。而自旋锁只能防真并发，所以在SMP上经常会同时使用这两种方法，同时防伪并发和真并发。临时禁用伪并发有三种情况：

- a.禁用中断：如果线程和中断、软中断和中断之间会访问公共数据，那么在前者的临界区内可以临时禁用后者，也就是禁用中断，达到防止伪并发的目的。在后者的临界区内不用采取措施，因为前者不能抢占后者，但是后者能抢占前者。前者一般会同时使用禁中断和自旋锁，禁中断防止伪并发，自旋锁防止真并发。
    
- b.禁用软中断：如果线程和软中断会访问公共数据，那么在前者的临界区内禁用后者，也就是禁用软中断，可以达到防止伪并发的目的。后者不用采取任何措施，因为前者不会抢占后者。前者也可以和自旋锁并用，同时防止伪并发和真并发。
    
- c.禁用抢占：如果线程和线程之间会访问公共数据，那么可以在临界区内禁用抢占，达到防止伪并发的目的。禁用抢占也可以和自旋锁并用，同时防止伪并发和真并发。
    

### 2.2 空间上防同步

你们可以同时，但是我不让你们访问相同的数据，有两种方法：

⑴数据分割

把大家都要访问的公共数据分割成N份，各访问各的。这也有两种情况：

① 在SMP上如果多个CPU要经常访问一个全局数据，那么可以把这个数据拆分成NR_CPU份，每个CPU只访问自己对应的那份，这样就不存在并发问题了，这个方法叫做 per-CPU 变量，只能防真并发，适用于SMP，需要和禁用抢占配合使用。

②TLS，每个线程都用自己的局部数据，这样就不存在并发问题了，能防真并发和伪并发，适用于UP和SMP。

⑵数据复制

RCU，只适用于用指针访问的动态数据。读者复制指针，然后就可以随意读取数据了，所有的读者可以共同读一份数据。写者复制数据，然后就可以随意修改复制后的数据了，因为这份数据是私有的。不过修改完数据之后要修改指针指向最新的数据，修改指针的这个操作需要是原子的。对于读者来说，它是复制完指针之后用自己的私有指针来访问数据的，所以它访问的要么是之前的数据，要么是修改之后的数据，而不会是不一致的数据。RCU不仅能实现读者之间的同时访问，还能实现读者与一个写者的同时访问，可谓是并发性非常高。RCU对于读者端的开销非常低、性能非常高。RCU能防伪并发和真并发，适用于UP和SMP。

### 2.3 事后防同步

不去积极预防并发，而是假设不存在并发，直接访问数据。访问完了之后再检查刚才是否有并发发生，如果有就再重来一遍，一直重试，直到没有并发发生为止。这就是内核里面的序列锁seqlock，能防伪并发和真并发，适用于UP和SMP。下面我们来画张图总结一下：

![[Pasted image 20241127110411.png]]
## 三、线程同步方法

### 3.1互斥锁

互斥锁是 Linux 线程同步中一种简单而有效的加锁方法。它具有原子性和唯一性等特点，确保在同一时间只有一个线程可以访问共享资源。在多任务操作系统中，多个线程可能都需要使用同一种资源，互斥锁就像一把锁，用于控制对共享资源的访问。

互斥锁的特点如下：

- 原子性：把一个互斥量锁定为一个原子操作，这意味着操作系统（或 pthread 函数库）保证了如果一个线程锁定了一个互斥量，没有其他线程在同一时间可以成功锁定这个互斥量。
    
- 唯一性：如果一个线程锁定了一个互斥量，在它解除锁定之前，没有其他线程可以锁定这个互斥量。
    
- 非繁忙等待：如果一个线程已经锁定了一个互斥量，第二个线程又试图去锁定这个互斥量，则第二个线程将被挂起（不占用任何 CPU 资源），直到第一个线程解除对这个互斥量的锁定为止，第二个线程则被唤醒并继续执行，同时锁定这个互斥量。
    

互斥锁的操作流程如下：

- 在访问共享资源前的临界区域，对互斥锁进行加锁。
    
- 在访问完成后释放互斥锁上的锁。
    

常用的关于互斥锁的函数有：

- pthread_mutex_init：初始化一个互斥锁。
    
- pthread_mutex_lock：对互斥锁上锁，若此时互斥锁已经上锁，说明此时共享资源正在被使用，则调用者该函数者一直阻塞，直到互斥锁解锁后再上锁。
    
- pthread_mutex_trylock：调用该函数时，若互斥锁未加锁，则上锁，返回 0；若互斥锁已加锁，则函数直接返回失败，即 EBUSY。
    
- pthread_mutex_timedlock：当线程试图获取一个已加锁的互斥量时，允许绑定线程阻塞时间。
    
- pthread_mutex_unlock：对指定的互斥锁解锁。
    
- pthread_mutex_destroy：销毁指定的一个互斥锁。互斥锁在使用完毕后，必须要对互斥锁进行销毁，以释放资源。
    

互斥锁是休眠锁，加锁失败时要把自己放入等待队列，释放锁的时候要考虑唤醒等待队列的线程。互斥锁的定义如下(删除了一些配置选项)：

```c
struct mutex {
	atomic_long_t		owner;
	raw_spinlock_t		wait_lock;
	struct list_head	wait_list;
};
```

可以看到互斥锁的定义有atomic_long_t owner，这个和我们之前定义的int lock是相似的，只不过这里是个加强版，0代表没加锁，加锁的时候是非0，而不是简单的1，而是记录的是加锁的线程。然后是自旋锁和等待队列，自旋锁是用来保护等待队列的。这里的自旋锁为什么要用raw_spinlock_t呢，它和spinlock_t有什么区别呢？在标准的内核版本下是没有区别的，如果打了RTLinux补丁之后它们就不一样了，spinlock_t会转化为阻塞锁，raw_spinlock_t还是自旋锁，如果需要在任何情况下都要自旋的话请使用raw_spinlock_t。下面我们看一下它的加锁操作：

```c
void __sched mutex_lock(struct mutex *lock){
	might_sleep();
	if (!__mutex_trylock_fast(lock))
		__mutex_lock_slowpath(lock);
	}
	static __always_inline bool __mutex_trylock_fast(struct mutex *lock){
	unsigned long curr = (unsigned long)current;
	unsigned long zero = 0UL;
	if (atomic_long_try_cmpxchg_acquire(&lock->owner, &zero, curr))
		return true;
	return false;
}
```

可以看到加锁时先尝试直接加锁，用的就是CAS原子指令(x86的CAS指令叫做cmpxchg)。如果owner是0，代表当前锁是开着的，就把owner设置为自己(也就是当前线程，struct task_struct * 强转为 ulong)，代表自己成为这个锁的主人，也就是自己加锁成功了，然后返回true。如果owner不为0的话，代表有人已经持有锁了，返回false，后面就要走慢速路径了，也就是把自己放入等待队列里休眠等待。下面看看慢速路径的代码是怎么实现的：

```c
static noinline void __sched__mutex_lock_slowpath(struct mutex *lock){
	__mutex_lock(lock, TASK_UNINTERRUPTIBLE, 0, NULL, _RET_IP_);
}
 static int __sched__mutex_lock(struct mutex *lock, unsigned int state, unsigned int subclass,struct lockdep_map *nest_lock, unsigned long ip){	
	 return __mutex_lock_common(lock, state, subclass, nest_lock, ip, NULL, false);
}
static __always_inline int __sched__mutex_lock_common(struct mutex *lock, unsigned int state, unsigned int subclass,struct lockdep_map *nest_lock, unsigned long ip,struct ww_acquire_ctx *ww_ctx, const bool use_ww_ctx){
	struct mutex_waiter waiter;	
	int ret;
	raw_spin_lock(&lock->wait_lock);
	waiter.task = current;
	__mutex_add_waiter(lock, &waiter, &lock->wait_list);
	set_current_state(state);
	for (;;) {
		bool first;
		if (__mutex_trylock(lock))
			goto acquired;
		raw_spin_unlock(&lock->wait_lock);
		schedule_preempt_disabled();
		first = __mutex_waiter_is_first(lock, &waiter);
		set_current_state(state);
		if (__mutex_trylock_or_handoff(lock, first))
			break;
		raw_spin_lock(&lock->wait_lock);
	 }
	 acquired:	__set_current_state(TASK_RUNNING);
	 __mutex_remove_waiter(lock, &waiter);
	 raw_spin_unlock(&lock->wait_lock);	
	 return ret;
 }
 static void __mutex_add_waiter(struct mutex *lock, struct mutex_waiter *waiter,struct list_head *list){
	 debug_mutex_add_waiter(lock, waiter, current);	
	 list_add_tail(&waiter->list, list);
	 if (__mutex_waiter_is_first(lock, waiter))	
		 __mutex_set_flag(lock, MUTEX_FLAG_WAITERS);
 }
```

可以看到慢速路径最终会调用函数__mutex_lock_common，这个函数本身是很复杂的，这里进行了大量的删减，只保留了核心逻辑。函数先加锁mutex的自旋锁，然后把自己放到等待队列上去，然后就在无限for循环中休眠并等待别人释放锁并唤醒自己。

For循环的入口处先尝试加锁，如果成功就goto acquired，如果不成功就释放自旋锁，并调用schedule_preempt_disabled，此函数就是休眠函数，它会调度其它进程来执行，自己就休眠了，直到有人唤醒自己才会醒来继续执行。别人释放锁的时候会唤醒自己，这个我们后面会分析。

当我们被唤醒之后会去尝试加锁，如果加锁失败，再次来到for循环的开头处，再试一次加锁，如果不行就再走一次休眠过程。为什么我们加锁会失败呢，因为有可能多个线程同时被唤醒来争夺锁，我们不一定会抢锁成功。抢锁失败就再去休眠，最后总会抢锁成功的。

把自己加入等待队列时会设置flag MUTEX_FLAG_WAITERS，这个flag是设置在owner的低位上去，因为task_struct指针至少是L1_CACHE_BYTES对齐的，所以最少有3位可以空出来做flag。

**下面我们再来看一下释放锁的流程：**

```c
void __sched mutex_unlock(struct mutex *lock)
```

解锁的时候先尝试快速解锁，快速解锁的意思是没有其它在等待队列里，可以直接释放锁。怎么判断等待队列里没有线程在等待呢，这就是前面设置的MUTEX_FLAG_WAITERS的作用了。如果设置了这个flag，lock->owner 和 curr就不会相等，直接释放锁就会失败，就要走慢速路径。慢速路径的代码如下：

```c
static noinline void __sched __mutex_unlock_slowpath(struct mutex *lock, unsigned long ip)
```

上述代码进行了一些删减。可以看出上述代码会先释放锁，然后唤醒等待队列里面的第一个等待者。

### 3.2条件变量

条件变量是一种线程同步机制，用于等待特定条件的发生。条件变量通常与互斥锁一起使用，线程在等待条件变量时会释放互斥锁，进入等待状态。当条件满足时，其他线程可以通过信号量唤醒等待的线程。

条件变量需要配合互斥量一起使用，互斥量作为参数传入wait函数，函数把调用线程放到等待条件的线程列表上，然后对互斥量解锁，这两个是原子操作。当线程等待到条件，从wait函数返回之前，会再次锁住互斥量。

在使用条件变量时，需要注意 “惊群效应”。为了防止这种情况，在wait被唤醒后，还需要在while中去检查条件。例如有两个线程同时阻塞在wait，先后醒来，快的线程做完处理然后把条件reset了，并且对互斥量解锁，此时慢的线程在wait里获得了锁返回，还再去做处理就会出问题。

### 3.3信号量

信号量是一种计数器，用于控制对共享资源的访问数量。信号量可以用于实现线程同步和互斥，通过增加和减少信号量的值来控制线程的访问。

信号量的主要作用是保护共享资源，确保在同一时刻只有一个线程能够对其进行访问。通过信号量，线程在进入关键代码段之前必须获取信号量，一旦完成了对共享资源的操作，就释放信号量，以允许其他线程访问该资源。

信号量与互斥量的区别在于，互斥量只允许一个线程进入临界区，其他线程必须等待该线程释放锁才能进入；而信号量可以控制对一组资源的访问，信号量的值可以大于 1，表示同时允许多个线程访问资源。

信号量的实现原理涉及到原子操作和操作系统的底层支持，在不同的操作系统和编程语言中可能会有不同的实现方式。通常，操作系统提供了对信号量的原子操作支持，以确保在多线程环境下信号量的正确使用。

下面我们先看一下信号量的定义：

```c
struct semaphore {	
	raw_spinlock_t		lock;	
	unsigned int		count;
	struct list_head	wait_list;
};

#define __SEMAPHORE_INITIALIZER(name, n)				\
{                                                       \
.lock		= __RAW_SPIN_LOCK_UNLOCKED((name).lock),	\
.count		= n,						\
.wait_list	= LIST_HEAD_INIT((name).wait_list),		\
}
static inline void sema_init(struct semaphore *sem, int val) {	
	*sem = (struct semaphore) __SEMAPHORE_INITIALIZER(*sem, val);
}
```

可以看出信号量和互斥锁的定义很相似，都有一个自旋锁，一个等待队列，不同的是信号量没有owner，取而代之的是count，代表着某一类资源的个数，而且自旋锁同时保护着等待队列和count。信号量初始化时要指定count的大小。我们来看一下信号量的down操作(获取一个资源)：

```c
void down(struct semaphore *sem){
	unsigned long flags;
	might_sleep();
	raw_spin_lock_irqsave(&sem->lock, flags);
	if (likely(sem->count > 0)) sem->count--;
	else __down(sem);
	raw_spin_unlock_irqrestore(&sem->lock, flags);
}
static noinline void __sched __down(struct semaphore *sem){
	__down_common(sem, TASK_UNINTERRUPTIBLE, MAX_SCHEDULE_TIMEOUT);
}
static inline int __sched __down_common(struct semaphore *sem, long state,								long timeout){
	struct semaphore_waiter waiter;
	list_add_tail(&waiter.list, &sem->wait_list);
	waiter.task = current;	waiter.up = false;
	for (;;) {
		if (signal_pending_state(state, current))
			goto interrupted;
		if (unlikely(timeout <= 0))
			goto timed_out;
		__set_current_state(state);
		raw_spin_unlock_irq(&sem->lock);
		timeout = schedule_timeout(timeout);
		raw_spin_lock_irq(&sem->lock);
		if (waiter.up)
			return 0;
	}
	timed_out:
		list_del(&waiter.list);
		return -ETIME;
	interrupted:
		list_del(&waiter.list);
		return -EINTR;
}
```

可以看出我们会先持有自旋锁，然后看看count是不是大于0，大于0的话代表资源还有剩余，我们直接减1，代表占用一份资源，就可以返回了。如果不大于0的话，代表资源没有了，我们就进去等待队列等待。

**我们再来看看up操作(归还资源)：**

```
void up(struct semaphore *sem){	unsigned long flags;	raw_spin_lock_irqsave(&sem->lock, flags);	if (likely(list_empty(&sem->wait_list)))		sem->count++;	else		__up(sem);	raw_spin_unlock_irqrestore(&sem->lock, flags);}static noinline void __sched __up(struct semaphore *sem){	struct semaphore_waiter *waiter = list_first_entry(&sem->wait_list,						struct semaphore_waiter, list);	list_del(&waiter->list);	waiter->up = true;	wake_up_process(waiter->task);}
```

先加锁自旋锁，然后看看等待队列是否为空，如果为空的话直接把count加1就可以了。如果不为空的话，则代表有人在等待资源，资源就不加1了，直接唤醒队首的线程来获取。

### 3.4Futex

Futex 是 Linux 系统中的一种高效线程同步机制，通过减少系统调用的次数来提高效率。Futex 允许线程在用户态进行大部分操作，只有在真正需要等待时才会调用系统调用进入内核态。

Futex 是 Fast Userspace Mutexes 的缩写，即快速用户空间互斥体。其设计思想是在传统的 Unix 系统中，很多同步操作是无竞争的，但进程仍需要陷入内核去检查是否有竞争发生，这造成了大量的性能开销。Futex 是一种用户态和内核态混合的同步机制，同步的进程间通过 mmap 共享一段内存，futex 变量就位于这段共享的内存中且操作是原子的，当进程尝试进入互斥区或者退出互斥区的时候，先去查看共享内存中的 futex 变量，如果没有竞争发生，则只修改 futex，而不用再执行系统调用了。当通过访问 futex 变量告诉进程有竞争发生，则还是得执行系统调用去完成相应的处理（wait 或者 wake up）。

## 四、线程同步的实现机制

### 4.1信号量机制

在 Linux 中，信号量是一种常用的线程同步机制。信号量可以用于实现线程同步和互斥，通过增加和减少信号量的值来控制线程的访问。

信号量的初始化可以通过sem_init函数实现，该函数接受三个参数：信号量指针、共享范围（0 表示线程间使用，非 0 表示进程间使用）以及初始值。例如：sem_init(&sem, 0, value);，这里的sem是要初始化的信号量，初始值为value。

当需要等待信号量时，可以使用sem_wait函数。该函数以原子操作的方式将信号量的值减 1。如果信号量的值大于 0，将信号量的值减 1，立即返回。如果信号量的值为 0，则线程阻塞，直到该信号量值为非 0 值。

当信号量的值满足条件时，可以使用sem_post函数以原子操作的方式将信号量的值加 1。当信号量的值大于 0 时，其它正在调用sem_wait等待信号量的线程将被唤醒，选择机制同样是由线程的调度策略决定的。

在使用完信号量后，可以使用sem_destroy函数对其进行销毁清理。只有用sem_init初始化的信号量才能用sem_destroy销毁。

信号量的主要作用是保护共享资源，确保在同一时刻只有一个线程能够对其进行访问。通过信号量，线程在进入关键代码段之前必须获取信号量，一旦完成了对共享资源的操作，就释放信号量，以允许其他线程访问该资源。

信号量与互斥量的区别在于，互斥量只允许一个线程进入临界区，其他线程必须等待该线程释放锁才能进入；而信号量可以控制对一组资源的访问，信号量的值可以大于 1，表示同时允许多个线程访问资源。

信号量的实现原理涉及到原子操作和操作系统的底层支持，在不同的操作系统和编程语言中可能会有不同的实现方式。通常，操作系统提供了对信号量的原子操作支持，以确保在多线程环境下信号量的正确使用。

### 4.2互斥锁机制

互斥锁的初始化、加锁、解锁和销毁等操作可以通过系统调用实现。互斥锁的类型包括普通锁、嵌套锁、检错锁和适应锁等，不同类型的锁在试图对一个已经被锁定的互斥锁加锁时表现不同。

普通锁是互斥锁的默认类型。当一个线程对一个普通锁加锁以后，其余请求该锁的线程将形成一个等待队列，并在该锁解锁后按照优先级获得它，这种锁类型保证了资源分配的公平性。一个线程如果对一个已经加锁的普通锁再次加锁，将引发死锁；对一个已经被其他线程加锁的普通锁解锁，或者对一个已经解锁的普通锁再次解锁，将导致不可预期的后果。

检错锁在一个线程对一个已经加锁的检错锁再次加锁时，加锁操作返回EDEADLK；对一个已经被其他线程加锁的检错锁解锁或者对一个已经解锁的检错锁再次解锁，则解锁操作返回EPERM。

嵌套锁允许一个线程在释放锁之前多次对它加锁而不发生死锁；其他线程要获得这个锁，则当前锁的拥有者必须执行多次解锁操作；对一个已经被其他线程加锁的嵌套锁解锁，或者对一个已经解锁的嵌套锁再次解锁，则解锁操作返回EPERM。

默认锁在一个线程如果对一个已经加锁的默认锁再次加锁，或者虽一个已经被其他线程加锁的默认锁解锁，或者对一个解锁的默认锁解锁，将导致不可预期的后果；这种锁实现的时候可能被映射成上述三种锁之一。

互斥锁的操作流程通常是在访问共享资源前的临界区域，对互斥锁进行加锁，使用诸如pthread_mutex_lock等函数。在访问完成后释放互斥锁上的锁，如使用pthread_mutex_unlock函数。

### 4.3条件变量机制

条件变量的初始化、等待、激发和销毁等操作可以通过系统调用实现。条件变量通常与互斥锁一起使用，用于等待特定条件的发生。

条件变量需要配合互斥量一起使用，互斥量作为参数传入wait函数，函数把调用线程放到等待条件的线程列表上，然后对互斥量解锁，这两个是原子操作。当线程等待到条件，从wait函数返回之前，会再次锁住互斥量。

在使用条件变量时，需要注意 “惊群效应”。为了防止这种情况，在wait被唤醒后，还需要在while中去检查条件。例如有两个线程同时阻塞在wait，先后醒来，快的线程做完处理然后把条件reset了，并且对互斥量解锁，此时慢的线程在wait里获得了锁返回，还再去做处理就会出问题。

**示例伪代码如下：**

```c
void* Thread1(void){    
	while(线程运行条件成立){
		…
		pthread_mutex_lock(qlock);
		while(条件成立)
			pthread_cond_wait(qcond,qlock); 
			// 或者 pthread_cond_wait(qcond,qlock,timeout);
			reset条件变量…
		pthread_mutex_unlock(qlock);
	}
}
void* Thread2(void){
	while(线程运行条件成立){
		…
		pthread_mutex_lock(qlock);
		set了条件变量…
		// 可以发送处理信号
		pthread_cond_signal(qcond);
		// 或者 pthread_cond_broadcast(qcond);
		pthread_mutex_unlock(qlock);
	}
}
```

条件变量的用法包括创建和注销、等待和唤醒等操作。创建和注销有静态和动态两种方式，静态方式使用PTHREAD_COND_INITIALIZER常量，动态方式调用pthread_cond_init函数。注销一个条件变量需要调用pthread_cond_destroy函数，只有在没有线程在该条件变量上等待的时候才能注销这个条件变量，否则返回EBUSY。

等待有两种方式，无条件等待pthread_cond_wait和计时等待pthread_cond_timedwait。这两种等待方式都必须和一个互斥锁配合，以防止多个线程同时请求的竞争条件。

唤醒条件有两种形式，pthread_cond_signal唤醒一个等待该条件的线程，存在多个等待线程时按入队顺序唤醒其中一个；而pthread_cond_broadcast则唤醒所有等待线程。必须在互斥锁的保护下使用相应的条件变量，否则可能会引起死锁。

## 五、线程同步的应用场景

### 5.1生产者 - 消费者问题

在多线程编程中，生产者 - 消费者问题是一个经典的同步问题。生产者线程负责生产数据，消费者线程负责消费数据。当生产者生产数据时，如果数据未被消费完，生产者需要等待消费者消费完数据后才能继续生产。同样，当消费者消费数据时，如果没有数据可消费，消费者需要等待生产者生产数据后才能继续消费。

条件变量可以用于实现生产者 - 消费者问题。当生产者生产数据时，通过信号量唤醒等待的消费者线程；当消费者消费数据时，通过信号量唤醒等待的生产者线程。例如，在以下代码中：

```c
// 生产者线程程序
void *product() {
	int id = ++product_id;
	while (1) {
		// 用 sleep 的数量可以调节生产和消费的速度,便于观察
		sleep(1);
		sem_wait(&empty_sem);
		pthread_mutex_lock(&mutex);
		in = in % M;
		printf("product%d in %d. like: \t", id, in);
		buff[in] = 1;
		print();
		++in;
		pthread_mutex_unlock(&mutex);
		sem_post(&full_sem);
	}
}
// 消费者方法
void *prochase() 
{
	int id = ++prochase_id;
	 while (1) {
		 // 用 sleep 的数量可以调节生产和消费的速度,便于观察
		 sleep(1);
		 sem_wait(&full_sem); 
		 pthread_mutex_lock(&mutex); 
		 out = out % M;
		 printf("prochase%d in %d. like: \t", id, out);
		 buff[out] = 0;
		 print();
		 ++out;
		 pthread_mutex_unlock(&mutex);
		 sem_post(&empty_sem);
	 }
 }
```

这里使用了信号量empty_sem和full_sem以及互斥锁mutex来实现生产者 - 消费者问题。生产者在生产数据之前，先检查缓冲区是否已满，如果已满，则等待消费者消费数据后再进行生产。消费者在消费数据之前，先检查缓冲区是否为空，如果为空，则等待生产者生产数据后再进行消费。

### 5.2读者 - 写者问题

多个读者可以同时读取数据，但写者需要独占访问。互斥锁可以用于实现读者 - 写者问题，当读者读取数据时，通过互斥锁保护共享数据；当写者写入数据时，通过互斥锁独占访问共享数据。

例如，在读者优先的情况下：

```c
void *reader(void *in) {
//     while(1)
//     {
	sem_wait(&sem_read);
	readerCnt++;
	if (readerCnt == 1) {
		pthread_mutex_lock(&mutex_write);
	}
	sem_post(&sem_read);
	printf("读线程id%d进入数据集\n", pthread_self());
	read();
	printf("读线程id%d退出数据集\n", pthread_self());
	sem_wait(&sem_read);
	readerCnt--;
	if (readerCnt == 0) {
		pthread_mutex_unlock(&mutex_write);
	}
	sem_post(&sem_read);
	sleep(R_SLEEP);
	// }
	pthread_exit((void *)0);
}
void *writer(void *in) {
// while(1)
//    {    
	pthread_mutex_lock(&mutex_write);
	printf("写线程id%d进入数据集\n", pthread_self()); 
	write();
	printf("写线程id%d退出数据集\n", pthread_self());
	pthread_mutex_unlock(&mutex_write);
	sleep(W_SLEEP);
	//     }    
	pthread_exit((void *)0);
}
```

这里使用了互斥锁mutex_write和信号量sem_read来实现读者优先的读者 - 写者问题。读者在读取数据之前，先增加读者计数器readerCnt，如果是第一个读者，则获取互斥锁mutex_write，以防止写者写入数据。读者在读取数据之后，减少读者计数器readerCnt，如果是最后一个读者，则释放互斥锁mutex_write，以允许写者写入数据。写者在写入数据之前，获取互斥锁mutex_write，以独占访问共享数据。写者在写入数据之后，释放互斥锁mutex_write，以允许读者读取数据。在写者优先的情况下：

```c
void *writer(void *in) {    
//    while(1)
//    {
	sem_wait(&sem_write);
	{
	//临界区,希望修改 writerCnt,独占 writerCnt        
	writerCnt++;
	if (writerCnt == 1) {
	//阻止后续的读者加入待读队列
		pthread_mutex_lock(&mutex_read);
	}
	sem_post(&sem_write);
	pthread_mutex_lock(&mutex_write);
	{
		//临界区，限制只有一个写者修改数据
		printf("写线程id%d进入数据集\n", pthread_self());
		write();
		printf("写线程id%d退出数据集\n", pthread_self());
	}
	pthread_mutex_unlock(&mutex_write);
	sem_wait(&sem_write);
	{
		//临界区,希望修改 writerCnt,独占 writerCnt
		writerCnt--;
		if (writerCnt == 0) {
			//阻止后续的读者加入待读队列
			pthread_mutex_unlock(&mutex_read);
		}
		sem_post(&sem_write);
	}
		sleep(W_SLEEP);
	}
	pthread_exit((void *)0);
}
void *reader(void *in) 
	//    while(1)
	//    {
	//假如写者锁定了 mutex_read,那么成千上万的读者被锁在这里 
	pthread_mutex_lock(&mutex_read)
	//只被一个读者占有    
	{
		//临界区
		sem_wait(&sem_read);
		//代码段 1
		{
			readerCnt++;
			if (readerCnt == 1) {
				pthread_mutex_lock(&mutex_write);
			}
			sem_post(&sem_read); 
		} 
		pthread_mutex_unlock(&mutex_read);
		//释放时,写者将优先获得 mutex_read
		printf("读线程id%d进入数据集\n", pthread_self());
		read();
		printf("读线程id%d退出数据集\n", pthread_self());
		sem_wait(&sem_read);
		//代码段2
		{
			//临界区
			readerCnt--;
			if (readerCnt == 0) {
				pthread_mutex_unlock(&mutex_write);
				//在最后一个并发读者读完这里开始禁止写者执行写操作
			}
			sem_post(&sem_read);
		}
		sleep(R_SLEEP);
	}
	pthread_exit((void *)0);
}
```

这里使用了两个互斥锁mutex_write和mutex_read以及两个信号量sem_read和sem_write来实现写者优先的读者 - 写者问题。写者在写入数据之前，先增加写者计数器writerCnt，如果是第一个写者，则获取互斥锁mutex_read，以阻止读者读取数据。写者在写入数据之后，减少写者计数器writerCnt，如果是最后一个写者，则释放互斥锁mutex_read，以允许读者读取数据。读者在读取数据之前，先获取互斥锁mutex_read，以防止写者写入数据。读者在读取数据之后，释放互斥锁mutex_read，以允许写者写入数据。

### 5.3计数器问题

多个线程需要对同一个计数器进行递增操作，需要同步以保证计数的准确性。原子操作可以用于实现计数器问题，通过原子操作确保计数器的递增操作是原子性的。例如：

```c
static int count = 0;
void *test_func(void *arg) {
	int i = 0
	for (i = 0; i < 20000; ++i) {
		__sync_fetch_and_add(&count, 1)
	}
	return NULL;
}
int main(int argc, const char *argv[]) {    
	pthread_t id[20];
	int i = 0;
	for (i = 0; i < 20; ++i) {
		pthread_create(&id[i], NULL, test_func, NULL);
	}
	for (i = 0; i < 20; ++i) {
		pthread_join(id[i], NULL);
	}
	printf("%d\n", count);
	return 0;
}
```

在这个例子中，使用了__sync_fetch_and_add函数来实现原子操作，确保多个线程对计数器count的递增操作是原子性的，从而保证了计数的准确性。

## 六、全文总结

Linux 线程同步是多线程编程中的关键环节，本文介绍了 Linux 线程同步的方法、实现机制和应用场景。通过合理地使用互斥锁、条件变量、信号量和 Futex 等线程同步机制，可以有效地避免竞态条件和数据不一致的情况，提高多线程程序的稳定性和可靠性。

在实际的多线程编程中，我们需要根据具体的应用场景选择合适的线程同步机制。例如，对于简单的资源互斥访问，可以使用互斥锁；对于需要等待特定条件的情况，可以使用条件变量；对于控制对一组资源的访问，可以使用信号量。而 Futex 则可以在需要高效线程同步的场景下发挥作用，通过减少系统调用的次数来提高效率。

同时，在使用这些线程同步机制时，我们需要注意一些问题，如避免死锁、防止 “惊群效应” 等。正确使用线程同步机制是至关重要的，否则可能会导致程序出现各种问题，如数据错误、死锁等。