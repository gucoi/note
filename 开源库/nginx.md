
在单进程的网络编程模型中。所有的网络相关的动作都是在一个进程里完成的，如监听 socket 的创建， bind、listen。再比如 epoll 的创建、要监听事件的添加，以及 epoll_wait 等待时间发生。这些统统都是在一个进程里搞定。

一个客户端和使用了 epoll 的服务端的交互过程如下图所示。


![[Pasted image 20240920135612.png]]

代码示例
```c
int main(){  
 //监听  
 lfd = socket(AF_INET,SOCK_STREAM,0);  
 bind(lfd, ...)  
 listen(lfd, ...);  
  
 //创建epoll对象，并把 listen socket的事件管理起来  
 efd = epoll_create(...);  
 epoll_ctl(efd, EPOLL_CTL_ADD, lfd, ...);  
  
 //事件循环  
 for (;;)  
 {  
  size_t nready = epoll_wait(efd, ep, ...);  
  for (int i = 0; i < nready; ++i){  
  
   if(ep[i].data.fd == lfd){  
    //lfd上发生事件表示都连接到达，accept接收它  
    fd = accept(listenfd, ...);  
    epoll_ctl(efd, EPOLL_CTL_ADD, fd, ...);  
   }else{  
    //其它socket发生的事件都是读写请求、或者关闭连接  
    ...  
   }  
  }  
 }  
}
```

在单进程模型中，不管有多少的连接，是几万还是几十万，服务器都是通过 epoll 来监控这些连接 socket 上的可读和可写事件。当某个 socket 上有数据发生的时候，再以非阻塞的方式对 socket 进行读写操作。

事实上，Redis 5.0 及以前的版本中，它的网络部分去掉对 handler 的封装，去掉时间事件以后，代码基本和上述 demo 非常接近。而且因为 Redis 的业务特点只需要内存 IO，且 CPU 计算少，所以可以达到数万的 QPS。

![[Pasted image 20240920135746.png]]
但是单进程的问题也是显而易见的，没有办法充分发挥多核的优势。所以目前业界绝大部分的后端服务还都是需要基于多进程的方式来进行开发的。到了多进程的时候，更复杂的问题多进程之间的配合和协作问题就产生了。比如  

- 哪个进程执行监听 listen ，以及 accept 接收新连接？
- 哪个进程负责发现用户连接上的读写事件？
- 当有用户请求到达的时候，如何均匀地将请求分散到不同的进程中？
- 需不需要单独搞一部分进程执行计算工作
- ...
 

事实上，以上这些问题并没有标准答案。各大应用或者网络框架都有自己不同的实现方式。为此业界还专门总结出了两类网络设计模式 - [[Reactor]] 和 [[Proactor]]。不过今天我不想讨论这种抽象模式，而是想带大家看一个具体的 Case - Nginx 是如何在多进程下使用 epoll 的。

## Nginx Master 进程初始化


在 Nginx 中，将进程分成了两类。一类是 Master 进程，一类是 Worker 进程。

在 Master 进程中，主要的任务是负责启动整个程序、读取配置文件、监听和处理各种信号，并对 Worker 进程进行统筹管理。

不过今天我们要查看的重点问题是看网络。在 Master 进程中，和网络相关的操作非常简单就是创建了 socket 并对其进行 bind 和 监听。

![[Pasted image 20240920140558.png]]


```c
//file: src/core/nginx.c  
int ngx_cdecl main(int argc, char *const *argv)  
{  
 ngx_cycle_t      *cycle, init_cycle;  
  
 //1.1 ngx_init_cycle 中开启监听  
 cycle = ngx_init_cycle(&init_cycle);  
  
 //1.2 启动主进程循环  
 ngx_master_process_cycle(cycle);  
}
```

在 Nginx 中，ngx_cycle_t 是非常核心的一个结构体。这个结构体存储了很多东西，也贯穿了好多的函数。其中对端口的 bind 和 listen 就是在它执行时完成的。

ngx_master_process_cycle 是 Master 进程的主事件循环。它先是根据配置启动指定数量的 Worker 进程，然后就开始关注和处理重启、退出等信号。接下来我们分两个小节来更详细地看。

### Nginx 的服务端口监听

我们看下 ngx_init_cycle 中是如何执行 bind 和 listen 的。

```c
//file: src/core/ngx_cycle.c  
ngx_cycle_t *ngx_init_cycle(ngx_cycle_t *old_cycle)  
{  
 ......  
 if (ngx_open_listening_sockets(cycle) != NGX_OK) {  
  goto failed;  
 }  
}
```

真正的监听还是在 ngx_open_listening_sockets 函数中，继续看它的源码。

```c
//file: src/core/ngx_connection.c  
ngx_int_t ngx_open_listening_sockets(ngx_cycle_t *cycle)  
{  
 ......  
  
 //要监听的 socket 对象  
 ls = cycle->listening.elts;  
 for (i = 0; i < cycle->listening.nelts; i++) {  
  
  //获取第i个socket  
  s = ngx_socket(ls[i].sockaddr->sa_family, ls[i].type, 0);  
  
  //绑定  
  bind(s, ls[i].sockaddr, ls[i].socklen)  
  
  //监听  
  listen(s, ls[i].backlog)  
  ls[i].listen = 1;  
  ls[i].fd = s;  
 }  
}
```

在这个函数中，遍历要监听的 socket。如果是启用了 REUSEPORT 配置，那先把 socket 设置上 SO_REUSEPORT 选项。然后接下来就是大家都熟悉的 bind 和 listen。**所以，bind 和 listen 是在 Master 进程中完成的。**

### Master 进程的主循环

在 ngx_master_process_cycle 中主要完成两件事。

- 启动 Worker 进程
- 将 Master 进程推入事件循环

在创建 Worker 进程的时候，是通过 fork 系统调用让 Worker 进程完全复制自己的资源，包括 listen 状态的 socket 句柄。

![[Pasted image 20240920140810.png]]

```c
//file: src/os/unix/ngx_process_cycle.c  
void ngx_master_process_cycle(ngx_cycle_t *cycle)  
{  
 ......  
 ngx_start_worker_processes(cycle, ccf->worker_processes,  
          NGX_PROCESS_RESPAWN);  
 //进入主循环,等待接收各种信号  
 for ( ;; ) {  
  //ngx_quit  
  //ngx_reconfigure  
  //ngx_restart  
  ...  
 }  
}
```

主进程在配置中读取到了 Worker 进程的数量 `ccf->worker_processes`。通过 ngx_start_worker_processes 来启动指定数量的 Worker。

```c
//file:src/os/unix/ngx_process_cycle.c 
static void ngx_start_worker_processes(...)  
{  
 for (i = 0; i < n; i++) {  
  @cycle nginx 核心数据结构
  @ngx_worker_process_cycle worker 进程的入口函数
  @i 当前 worker 的序号
  ngx_spawn_process(cycle, ngx_worker_process_cycle,  
        (void *) (intptr_t) i, "worker process", type);  
  ...  
 }  
}
```

上述代码中值得注意的是，在调用 ngx_spawn_process 时的几个参数

```c
//file: src/os/unix/ngx_process.c
ngx_pid_t ngx_spawn_process(ngx_cycle_t *cycle, ngx_spawn_proc_pt proc,...){
	pid = fork();
	switch (pid) {  
		case -1:
		//出错了
		...   
		case 0:
		//子进程创建成功
		nigx_parent = ngx_pid;
		ngx_pid = ngx_getpid();
		proc(cycle, data);
		break;
		default:
		break;
	}
	...
}
```

在 ngx_spawn_process 中调用 fork 来创建进程，创建成功后 Worker 进程就将进入 ngx_worker_process_cycle 来进行处理了。

**总结：** 在网络上，master 进程其实只是 listen 了一下。listen 过后的 socket 存到 cycle->listening 这里了。剩下的网络操作都是在 Worker 中完成的。

## Worker 进程处理

在上面小节中看到，Master 进程关于网络其实做的事情不多，只是 bind 和 listen 了一下。epoll 相关的函数调用一个也没见着，更别说 accept 接收连接，以及 read 、 write 函数处理了。那这些细节一定都是在 Worker 进程中完成的。

事实的确如此，epoll_create、epoll_ctl、epoll_wait 都是在 Worker 进程中执行的。

![[Pasted image 20240920141346.png]]

在 Worker 进程中，创建了一个 epoll 内核对象，通过 epoll_ctl 将其想监听的事件注册上去，然后调用 epoll_wait 进入事件循环。

```c
//file: src/os/unix/ngx_process_cycle.c  
static void ngx_worker_process_cycle(ngx_cycle_t *cycle, void *data)  
{  
 //2.2 Worker进程初始化编译进来的各个模块  
 ngx_worker_process_init(cycle, worker);  
  
 //进入事件循环  
 for ( ;; ) {  
  //2.3 进入 epollwait  
  ngx_process_events_and_timers(cycle);  
  ......  
 }  
}
```

### Nginx 的网络相关 module

撇开 Worker 的工作流程不提，咱们先来了解一个背景知识 - Nginx module。

Nginx 采用的是一种模块化的架构，它的模块包括核心模块、标准HTTP模块、可选HTTP模块、邮件服务模块和第三方模块等几大类。每一个模块都以一个 module 的形式存在，都对应一个 ngx_module_s 结构体。通过这种方式来实现软件可拔插，是一种非常优秀的软件架构。

![[Pasted image 20240920141532.png]]

每个 module 根据自己的需求来实现各种 init_xxx, exit_xxx 方法来供 Nginx 在合适的时机调用。

```c
//file: src/core/ngx_module.h  
struct ngx_module_s {  
 ......  
  
 ngx_uint_t            version;  
  
 void                 *ctx;  
 ngx_command_t        *commands;  
 ngx_uint_t            type;  
  
 ngx_int_t           (*init_master)(ngx_log_t *log);  
 ngx_int_t           (*init_module)(ngx_cycle_t *cycle);  
 ngx_int_t           (*init_process)(ngx_cycle_t *cycle);  
 ngx_int_t           (*init_thread)(ngx_cycle_t *cycle);  
 void                (*exit_thread)(ngx_cycle_t *cycle);  
 void                (*exit_process)(ngx_cycle_t *cycle);  
 void                (*exit_master)(ngx_cycle_t *cycle);  
  
 ......  
};
```

其中和网络相关的 module 有 ngx_events_module 、ngx_event_core_module 和具体的网络底层模块 ngx_epoll_module、ngx_kqueue_module等。

对于 ngx_epoll_module 来说，它在其上下文 ngx_epoll_module_ctx 中定义了各种 actions 方法（添加事件、删除事件、添加连接等）。

```c
//file:src/event/ngx_event.h  
typedef struct {  
 ngx_str_t              *name;  
  
 void                 *(*create_conf)(ngx_cycle_t *cycle);  
 char                 *(*init_conf)(ngx_cycle_t *cycle, void *conf);  
  
 ngx_event_actions_t     actions;  
} ngx_event_module_t;
```

```c
//file:src/event/modules/ngx_epoll_module.c  
static ngx_event_module_t  ngx_epoll_module_ctx = {  
 &epoll_name,  
 ngx_epoll_create_conf,               /* create configuration */  
 ngx_epoll_init_conf,                 /* init configuration */  
  
 {  
  ngx_epoll_add_event,             /* add an event */  
  ngx_epoll_del_event,             /* delete an event */  
  ngx_epoll_add_event,             /* enable an event */  
  ngx_epoll_del_event,             /* disable an event */  
  ngx_epoll_add_connection,        /* add an connection */  
  ngx_epoll_del_connection,        /* delete an connection */  
#if (NGX_HAVE_EVENTFD)  
  ngx_epoll_notify,                /* trigger a notify */  
#else  
  NULL,                            /* trigger a notify */  
#endif  
  ngx_epoll_process_events,        /* process the events */  
  ngx_epoll_init,                  /* init the events */  
  ngx_epoll_done,                  /* done the events */  
 }  
};
```

其中有一个 init 方法是 ngx_epoll_init，在这个 init 中会进行 epoll 对象的创建，以及 ngx_event_actions 方法的设置。

```c
//file:src/event/modules/ngx_epoll_module.c
static ngx_int_tngx_epoll_init(ngx_cycle_t *cycle, ngx_msec_t timer){ 
	//创建一个 epoll 句柄 
	ep = epoll_create(cycle->connection_n / 2); 
	...
	ngx_event_actions = ngx_epoll_module_ctx.actions;
}
```

### Worker 进程初始化各个模块

Worker 进程初始化的时候，在 ngx_worker_process_init 中读取配置信息进行一些设置，然后调用所有模块的 init_process 方法。

![[Pasted image 20240920141828.png]]

```c
//file: src/os/unix/ngx_process_cycle.c  
static void  
ngx_worker_process_init(ngx_cycle_t *cycle, ngx_int_t worker)  
{  
 ...  
  
 //获取配置  
 ccf = (ngx_core_conf_t *) ngx_get_conf(cycle->conf_ctx, ngx_core_module);  
  
 //设置优先级  
 setpriority(PRIO_PROCESS, 0, ccf->priority)  
  
 //设置文件描述符限制  
 setrlimit(RLIMIT_NOFILE, &rlmt)  
 setrlimit(RLIMIT_CORE, &rlmt)  
  
 //group 和 uid 设置  
 initgroups(ccf->username, ccf->group)  
 setuid(ccf->user)  
  
 //CPU亲和性  
 cpu_affinity = ngx_get_cpu_affinity(worker)  
 if (cpu_affinity) {  
  ngx_setaffinity(cpu_affinity, cycle->log);  
 }  
 ......  
  
 //调用各个模块的init_process进行模块初始化  
 for (i = 0; cycle->modules[i]; i++) {  
  if (cycle->modules[i]->init_process) {  
   if (cycle->modules[i]->init_process(cycle) == NGX_ERROR) {  
    /* fatal */  
    exit(2);  
   }  
  }  
 }  
  
 ...  
}
```

前面我们说过 ngx_event_core_module ，它的 init_process 方法是 ngx_event_process_init。

```c
//file: src/event/ngx_event.c  
ngx_module_t  ngx_event_core_module = {  
 ...  
 ngx_event_process_init,                /* init process */  
 ...  
};
```

在 ngx_event_core_module 的 ngx_event_process_init 中，我们将看到 **Worker 进程使用 epoll_create 来创建 epoll 对象，使用epoll_ctl 来监听 listen socket 上的连接请求**。

![[Pasted image 20240920141921.png]]

ngx_event_process_init

```c
//file: src/event/ngx_event.c  
static ngx_int_t ngx_event_process_init(ngx_cycle_t *cycle)  
{  
 //调用模块的init，创建 epoll 对象  
 for (m = 0; cycle->modules[m]; m++) {  
  if (cycle->modules[m]->type != NGX_EVENT_MODULE) {  
   continue;  
  }  
  ...  
  module->actions.init(cycle, ngx_timer_resolution)  
  break;  
 }  
 ...  
  
 //获取自己监听的sokcet，将它们都添加到 epoll 中  
 ngx_event_t         *rev  
 ls = cycle->listening.elts;  
 for (i = 0; i < cycle->listening.nelts; i++) {  
  
  //获取一个 ngx_connection_t  
  c = ngx_get_connection(ls[i].fd, cycle->log);  
  
  //设置回调函数为 ngx_event_accept  
  rev->handler = ngx_event_accept   
  
  if (ngx_add_event(rev, NGX_READ_EVENT, 0) == NGX_ERROR) {  
   return NGX_ERROR;  
  }  
 }  
 ...  
}
```

通过 ngx_add_event 注册的 READ 事件的处理函数。ngx_add_event 就是一个抽象，对于 epoll 来说就是对 epoll_ctl 的封装而已。

```c

//file: src/event/ngx_event.h
#define ngx_add_event        ngx_event_actions.add

//file: src/event/modules/ngx_epoll_module.c
static ngx_int_t ngx_epoll_add_event(...){ 
	if (epoll_ctl(ep, op, c->fd, &ee) == -1)  {
	...
	}
}
```

### 进入 epollwait

在 ngx_worker_process_init 中， epoll_create 和 epoll_ctl 都已经完成了。接下来就是进入事件循环，执行 epoll_wait 来处理。

![[Pasted image 20240920143505.png]]

```c
//file: src/event/ngx_event.c  
void  
ngx_process_events_and_timers(ngx_cycle_t *cycle)  
{  
 ...  
 // 防accept惊群锁  
 if (ngx_use_accept_mutex) {  
  //尝试获取锁，获取失败直接返回  
  if (ngx_trylock_accept_mutex(cycle) == NGX_ERROR) {  
   return;  
  }  
  
  //获取锁成功，则设置 NGX_POST_EVENTS 标记。  
  if (ngx_accept_mutex_held) {  
   flags |= NGX_POST_EVENTS;  
  } else {  
   ...  
  }  
 }  
  
 //处理各种事件  
 (void) ngx_process_events(cycle, timer, flags);  
}
```

在 ngx_process_events_and_timers 开头处，判断是否使用 accpet_mutext 锁。这是一个防止惊群的解决办法。如果使用的话，先调用 ngx_trylock_accept_mutex 获取锁，获取失败则直接返回，过段时间再来尝试。获取成功是则设置 NGX_POST_EVENTS 的标志位。

接下来调用 ngx_process_events 来处理各种网络和 timer 事件。对于 epoll 来说，这个函数就是对 epoll_wait 的封装。

```c
//file: src/event/ngx_event.h  
#define ngx_process_events   ngx_event_actions.process_events
```

```c
//file: src/event/modules/ngx_epoll_module.c  
static ngx_int_t ngx_epoll_process_events(ngx_cycle_t *cycle, ...)  
{  
 events = epoll_wait(ep, event_list, (int) nevents, timer);  
 for (i = 0; i < events; i++) {  
  
  if (flags & NGX_POST_EVENTS) {  
   ...  
   ngx_post_event(rev, queue);  
  }else{  
   //调用回调函数  
   rev->handler(rev);  
  }  
  ...  
 }  
}
```

可见，在 ngx_epoll_process_events 是调用 epoll_wait 等待各种事件的发生。如果没有 NGX_POST_EVENTS 标志，则直接回调 rev->handler 进行处理。使用了 accept_mutex 锁的话，先把这个事件保存起来，等后面合适的时机再去 accpet。

简单对本节内容汇总一下。在 Master 进程中只是做了 socket 的 bind 和 listen。 而在 Worker 进程中所做的事情比较多，创建了 epoll，使用 epoll_ctl 将 listen 状态的 socket 的事件监控起来。最后调用 epoll_wait 进入了事件循环，开始处理各种网络和 timer 事件。 本节流程总结如图。

![[Pasted image 20240920143611.png]]

## 用户连接

现在假设用户的连接请求已经到了，这时候 epoll_wait 返回后会执行其对应的 handler 函数 ngx_add_event。

![[Pasted image 20240920143711.png]]

在该回调函数中被执行到的时候，表示 listen 状态的 socket 上面有连接到了。所以这个函数主要做了三件事。  

- 1.调用 accept 获取用户连接
- 2.获取 connection 对象，其回调函数为 ngx_http_init_connection
- 3.将新连接 socket 通过 epoll_ctl 添加到 epoll 中进行管理
我们来看 ngx_event_accept 详细代码。

```c
//file: src/event/ngx_event_accept.c  
void ngx_event_accept(ngx_event_t *ev)  
{  
 do {  
  //接收建立好的连接  
  s = accept(lc->fd, &sa.sockaddr, &socklen);  
  
  if s {  
   //3.1 获取 connection  
   c = ngx_get_connection(s, ev->log);  
  
   //3.2 添加新连接  
   if (ngx_add_conn(c) == NGX_ERROR) {  
    ngx_close_accepted_connection(c);  
    return;  
   }  
  }   
 } while (ev->available);  
}
```

listen socket 上的读事件发生的时候，就意味着有用户连接就绪了。所以可以直接通过 accept 将其取出来。取出连接以后，再获取一个空闲的 connection对象，通过 ngx_add_conn 将其添加到 epoll 中进行管理。

### 获取 connection

我们说一下 ngx_get_connection，这个函数本身倒是没有啥可说的。就是从 ngx_cycle 的 free_connections 中获取一个 connection 出来。

```c
//file: src/core/ngx_connection.c  
ngx_connection_t *ngx_get_connection(ngx_socket_t s, ngx_log_t *log)  
{  
 c = ngx_cycle->free_connections;  
 c->read = rev;  
 c->write = wev;  
 c->fd = s;  
 c->log = log;  
 return c  
}
```

值得说的是 free_connections 中的连接，对于 HTTP 服务来说，会经过 ngx_http_init_connection 的初始化处理。它会设置该连接读写事件的回调函数 c->read->handler 和 c->write->handler。

```c
//file: src/http/ngx_http_request.c  
void ngx_http_init_connection(ngx_connection_t *c)  
{  
 ...  
 rev = c->read;  
 rev->handler = ngx_http_wait_request_handler;  
 c->write->handler = ngx_http_empty_handler;  
}
```

### 添加新连接

我们再来看 ngx_add_conn，对于 epoll module 来说，它就是 ngx_epoll_add_connection 这个函数。

```c
//file: src/event/ngx_event.h  
#define ngx_add_conn         ngx_event_actions.add_conn  
  
//file: src/event/modules/ngx_epoll_module.c  
static ngx_int_t  
ngx_epoll_add_connection(ngx_connection_t *c)  
{  
 struct epoll_event  ee;  
 ee.events = EPOLLIN|EPOLLOUT|EPOLLET|EPOLLRDHUP;  
 ee.data.ptr = (void *) ((uintptr_t) c | c->read->instance);  
  
 epoll_ctl(ep, EPOLL_CTL_ADD, c->fd, &ee)  
 c->read->active = 1;  
 c->write->active = 1;  
  
 return NGX_OK;  
}
```

可见这只是 epoll_ctl 的一个封装而已。这里再补充说一下，如果这个客户端连接 socket 上有数据到达的时候，就会进入到上面[[#获取 connection]]中注册的 ngx_http_wait_request_handler 函数进行处理。后面就是 HTTP 的处理逻辑了。

## 总结

Nginx 的 Master 中做的网络相关动作不多，仅仅只是创建了 socket、然后 bind 并 listen 了一下。接着就是用自己 fork 出来多个 Worker 进程来。由于每个进程都一样，所以每个 Worker 都有 Master 创建出来的 listen 状态的 socket 句柄。

![[Pasted image 20240920144059.png]]

Worker 进程处理的网络相关工作就比较多了。epoll_create、epoll_ctl、epoll_wait 都是在 Worker 进程中执行的，也包括用户连接上的数据 read、处理 和 write。

![[Pasted image 20240920144127.png]]


- 1.先是使用 epoll_create 创建一个 epoll 对象出来
- 2.设置回调为 ngx_event_accept
- 3.通过 epoll_ctl 将所有 listen 状态的 socket 的事件都管理起来
- 4.执行 epoll_wait 等待 listen socket 上的连接到来
- 5.新连接到来是 epoll_wait 返回，进入 ngx_event_accept 回调
- 6.ngx_event_accept 回调中将新连接也添加到 epoll 中进行管理（其回调为ngx_http_init_connection）
- 7.继续进入 epoll_wait 等待事件
- 8.用户数据请求到达时进入 http 回调函数进行处理

讲到这里，你可以觉得咱们已经讨论完了。实际上有一个点我们还没有考虑到。我们上面讨论的流程是一个 Worker 在工作的情况。那么在多 Worker 的情况下，Nginx 的全貌咱们还没展开说过。通过上文我们可以看到以下几个细节：

- 1.每个 Worker 都会有一个属于自己的 epoll 对象
    
- 2.每个 Worker 会关注所有的 listen 状态上的新连接事件
    
- 3.对于用户连接，只有一个 Worker 会处理，其它 Worker 不会持有该用户连接的 socket。
    

根据这三条结论，我们再画一个 Nginx 的全貌图。

![[Pasted image 20240920144212.png]]

