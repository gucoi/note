
## 初始化

```c
// suricata.c
SuricataMain() {
	InitGlobal()
	|- RunModeRegisterRunModes() // register all run modules
	|- - RunModeIdsAFPRegister()
	|- - - RunModeRegisterNewRunMode(RunModeIdsAFPWorkers, AFPRunModeEnableIPS)
	- - - RunModeRegisterNewRunMode(RunModeIdsAFPAutoFp, AFPRunModeEnableIPS)
	... // register other modules
	StartInternalRunMode() // exe internal runmode (RUNMODE_PRINT_VERSION etc)
	kParseCommandLine() // parse command
	StartInternalRunMode() // exe internal runmode (RUNMODE_PRINT_VERSION etc)
	LoadYamlConfig() // read and parse config file
	PostConfLoadedSetup()
	|- RegisterAllModules()
	|- - TmModuleReceiveAFPRegister()
	RunModeInitializeThreadSettings() // init thread config thread stack size etc..
	RunModeDispatch() // use init runmode and custom mode afp autofp or afp workers etc
	}
```
### RunModeDispatch
从这里开始进入到调用配置文件里面的初始化，以及线程初始化等等, 以 AFP-PACKET 与 workers 举例
```c
// runmode.c
void RunModeDispatch(int runmode, const char *custom_mode, const char *capture_plugin_name,
        const char *capture_plugin_args)
   ​￼if (custom_mode == NULL) {
 k{
    if (custom_mode == NULL) {
        custom_mode = RunModeGetConfOrDefault(runmode, capture_plugin_name);
    }
    RunMode *mode = RunModeGetCustomMode(runmode, custom_mode);
k	// 从之前注册runmode的map里面去查
    RunMode *mode = RunModeGetCustomMode(runmode, custom_mode);
    if (mode == NULL) {
        exit(EXIT_FAILURE);
    }
	// RunModeFunc 就是 RunModeIdsAFPWorkers
    mode->RunModeFunc();

    /* Check if the alloted queues have at least 1 reader and writer */
    TmValidateQueueState();

	//创建管理线程, 不管是 workers 或者 autofp 哪种方式启动 都有两个管理线程
	FlowManagerThreadSpawn();
	FlowRecyclerThreadSpawn();
	if (RunModeNeedsBypassManager()) {
		BypassedFlowManagerThreadSpawn();
	}
	StatsSpawnThreads();
}
```
下面用到的 ReceiveAFP 和 Decode 就是之前注册好的模组

```c
// source-af-packet.c
void TmModuleReceiveAFPRegister (void)
{
    tmm_modules[TMM_RECEIVEAFP].name = "ReceiveAFP";
    tmm_modules[TMM_RECEIVEAFP].ThreadInit = NoAFPSupportExit;
    tmm_modules[TMM_RECEIVEAFP].Func = NULL;
    tmm_modules[TMM_RECEIVEAFP].ThreadExitPrintStats = NULL;
    tmm_modules[TMM_RECEIVEAFP].ThreadDeinit = NULL;
    tmm_modules[TMM_RECEIVEAFP].cap_flags = 0;
    tmm_modules[TMM_RECEIVEAFP].flags = TM_FLAG_RECEIVE_TM;
}

void TmModuleDecodeAFPRegister (void)
{
    tmm_modules[TMM_DECODEAFP].name = "DecodeAFP";
    tmm_modules[TMM_DECODEAFP].ThreadInit = NoAFPSupportExit;
    tmm_modules[TMM_DECODEAFP].Func = NULL;
    tmm_modules[TMM_DECODEAFP].ThreadExitPrintStats = NULL;
    tmm_modules[TMM_DECODEAFP].ThreadDeinit = NULL;
    tmm_modules[TMM_DECODEAFP].cap_flags = 0;
    tmm_modules[TMM_DECODEAFP].flags = TM_FLAG_DECODE_TM;
}
```

```c
//runmode-af-packet.c
int RunModeIdsAFPWorkers(void)
{
    ret = RunModeSetLiveCaptureWorkers(ParseAFPConfig, AFPConfigGeThreadsCount, "ReceiveAFP","DecodeAFP", thread_name_workers, live_dev);
	...
}
```

```c
// util-runmodes.c
//  每个网卡设备都会初始化多个 workers 线程
int RunModeSetLiveCaptureWorkers(ConfigIfaceParserFunc ConfigParser,
        ConfigIfaceThreadsCountFunc ModThreadsCount, const char *recv_mod_name,
        const char *decode_mod_name, const char *thread_name, const char *live_dev)
{
	// nlive 网卡数量
    for (ldev = 0; ldev < nlive; ldev++) {
        RunModeSetLiveCaptureWorkersForDevice(ModThreadsCount,
                recv_mod_name,
                decode_mod_name,
                thread_name,
                live_dev_c,
                aconf,
                0);
    }
    return 0;
}

static int RunModeSetLiveCaptureWorkersForDevice(ConfigIfaceThreadsCountFunc ModThreadsCount,
                              const char *recv_mod_name,
                              const char *decode_mod_name, const char *thread_name,
                              const char *live_dev, void *aconf,
                              unsigned char single_mode)
{
    int threads_count;
    uint16_t thread_max = TmThreadsGetWorkerThreadMax();

	threads_count = MIN(ModThreadsCount(aconf), thread_max);
    /* create the threads */
    for (int thread = 0; thread < threads_count; thread++) {
        char tname[TM_THREAD_NAME_MAX];
        TmModule *tm_module = NULL;
		// 在这里开始创建创建线程
        ThreadVars *tv = TmThreadCreatePacketHandler(tname,
                "packetpool", "packetpool",
                "packetpool", "packetpool",
                "pktacqloop");
        tv->printable_name = printable_threadname;

		// slot 就像是 线程固定的 工作槽, 从下方也看得出来，workers 模式下，slot 的排列是
		// recv -> decode -> flow_worker->respond_reject
		// 说明 workers 模式下的收包解包动作都在一个线程里面
        tm_module = TmModuleGetByName(recv_mod_name);
        TmSlotSetFuncAppend(tv, tm_module, aconf);

        tm_module = TmModuleGetByName(decode_mod_name);
        TmSlotSetFuncAppend(tv, tm_module, NULL);

        tm_module = TmModuleGetByName("FlowWorker");
        TmSlotSetFuncAppend(tv, tm_module, NULL);

        tm_module = TmModuleGetByName("RespondReject");
        TmSlotSetFuncAppend(tv, tm_module, NULL);
        TmThreadSetCPU(tv, WORKER_CPU_SET);
        TmThreadSpawn(tv)
    }

}
```

```c
ThreadVars *TmThreadCreatePacketHandler(const char *name, const char *inq_name,
                                    const char *inqh_name, const char *outq_name,
                                    const char *outqh_name, const char *slots)
{
    ThreadVars *tv = NULL;
    tv = TmThreadCreate(name, inq_name, inqh_name, outq_name, outqh_name,
                        slots, NULL, 0);

    tv->id = TmThreadsRegisterThread(tv, tv->type);
    return tv;
}
```

```c
ThreadVars *TmThreadCreate(const char *name, const char *inq_name, 
						   const char *inqh_name, const char *outq_name, 
						   const char *outqh_name, const char *slots,
                           void * (*fn_p)(void *), int mucond)
{
    /* XXX create separate function for this: allocate a thread container */
    tv = SCMalloc(sizeof(ThreadVars));
    memset(tv, 0, sizeof(ThreadVars));

    SC_ATOMIC_INIT(tv->flags);
    SCMutexInit(&tv->perf_public_ctx.m, NULL);

    /* default state for every newly created thread */
    TmThreadsSetFlag(tv, THV_PAUSE);
    TmThreadsSetFlag(tv, THV_USE);

    /* set the incoming queue */
    if (inq_name != NULL && strcmp(inq_name, "packetpool") != 0) {
        tmq = TmqGetQueueByName(inq_name);
        if (tmq == NULL) {
            tmq = TmqCreateQueue(inq_name);
        }
        tv->inq = tmq;
        tv->inq->reader_cnt++;
    }
    if (inqh_name != NULL) {
        int id = TmqhNameToID(inqh_name);
        tmqh = TmqhGetQueueHandlerByName(inqh_name);

        tv->tmqh_in = tmqh->InHandler;
        tv->inq_id = (uint8_t)id;
    }

    /* set the outgoing queue */
    if (outqh_name != NULL) {
        int id = TmqhNameToID(outqh_name);
        tmqh = TmqhGetQueueHandlerByName(outqh_name);
        tv->tmqh_out = tmqh->OutHandler;
        tv->outq_id = (uint8_t)id;

		// autofp 模式下一般会走这个
        if (outq_name != NULL && strcmp(outq_name, "packetpool") != 0) {
            if (tmqh->OutHandlerCtxSetup != NULL) {
                tv->outctx = tmqh->OutHandlerCtxSetup(outq_name);
                tv->outq = NULL;
            } else {
                tmq = TmqGetQueueByName(outq_name);
                if (tmq == NULL) {
                    tmq = TmqCreateQueue(outq_name);
                }

                tv->outq = tmq;
                tv->outctx = NULL;
                tv->outq->writer_cnt++;
            }
        }
    }

	TmThreadSetSlots(tv, slots, fn_p); // 在这里将 slot
    return NULL;
}
```

```c
// tmqh-packetpool.c
void TmqhPacketpoolRegister (void)
{
    tmqh_table[TMQH_PACKETPOOL].name = "packetpool";
    tmqh_table[TMQH_PACKETPOOL].InHandler = TmqhInputPacketpool;
    tmqh_table[TMQH_PACKETPOOL].OutHandler = TmqhOutputPacketpool;
}
```

```c
// tmqh-flow.c
// autofp 用来 flow 的队列 
void TmqhFlowRegister(void)
{
    tmqh_table[TMQH_FLOW].name = "flow";
    tmqh_table[TMQH_FLOW].InHandler = TmqhInputFlow; // 传送包从 flow 队列中出来
    tmqh_table[TMQH_FLOW].OutHandlerCtxSetup = TmqhOutputFlowSetupCtx;
    tmqh_table[TMQH_FLOW].OutHandlerCtxFree = TmqhOutputFlowFreeCtx;
    tmqh_table[TMQH_FLOW].RegisterTests = TmqhFlowRegisterTests;
	tmqh_table[TMQH_FLOW].OutHandler = TmqhOutputFlowHash; // 使用 hash 判断填充到那个队列中
}
```

这是 autofp 模式下才会走这个
```c
//tmqh-flow.c
void TmqhOutputFlowHash(ThreadVars *tv, Packet *p)
{
    uint32_t qid;
    TmqhFlowCtx *ctx = (TmqhFlowCtx *)tv->outctx;

    if (p->flags & PKT_WANTS_FLOW) {
        uint32_t hash = p->flow_hash;
        qid = hash % ctx->size;
    } else {
        qid = ctx->last++;
        if (ctx->last == ctx->size)
            ctx->last = 0;
    }
    PacketQueue *q = ctx->queues[qid].q;
    SCMutexLock(&q->mutex_q);
    PacketEnqueue(q, p);
    SCCondSignal(&q->cond_q);
    SCMutexUnlock(&q->mutex_q);
}
```

## 编译

```shell
./configure': ^Cconfigure --with-libhtp-includes=/usr/local/include --with-libhtp-libraries=/usr/local/lib/ --enable-non-bundled-htp --enable-debug --prefix=/root/guan_an/suricata/build/
```

autofp

![[Pasted image 20240930144332.png]]

slot


![[Pasted image 20240930144405.png]]

1. Suricata 简介  
    1.1 Suricata 的架构概览  
    1.2 核心组件和功能
    
2. Suricata 的运行模式（Runmodes）  
    2.1 单线程模式（Single Thread Mode）  
    2.2 工作者模式（Workers Mode）  
    2.3 自动模式（Autofp Mode）  
    2.4 每个 CPU 一个数据包处理线程模式（AFP mode: 1 Packet Processing Thread per CPU）
    
3. 线程通信机制  
    3.1 工作者模式下的线程通信  
    3.1.1 线程池实现  
    3.1.2 任务队列和负载均衡  
    3.1.3 锁机制和无锁算法  
    3.2 自动模式下的线程通信  
    3.2.1 流分配算法  
    3.2.2 线程间数据共享  
    3.3 AFP 模式下的线程通信  
    3.3.1 数据包分发机制  
    3.3.2 线程同步策略
    
4. TCP 流重组实现  
    4.1 流跟踪（Flow Tracking）  
    4.1.1 流表（Flow Table）的实现  
    4.1.2 哈希算法和冲突处理  
    4.2 TCP 状态机  
    4.2.1 连接建立和终止  
    4.2.2 异常情况处理（如 RST 包）  
    4.3 数据包重排序  
    4.3.1 序列号处理  
    4.3.2 滑动窗口实现  
    4.4 流重组算法  
    4.4.1 内存管理和优化  
    4.4.2 处理重传和重叠数据  
    4.5 应用层协议识别  
    4.5.1 协议解析器的实现  
    4.5.2 动态协议检测
    
5. 性能优化技术  
    5.1 内存管理优化  
    5.1.1 内存池和预分配策略  
    5.1.2 垃圾回收机制  
    5.2 数据结构优化  
    5.2.1 使用缓存友好的数据结构  
    5.2.2 无锁数据结构的应用  
    5.3 SIMD 指令集的使用  
    5.4 网络I/O优化  
    5.4.1 零拷贝技术  
    5.4.2 DPDK 和 AF_PACKET 的应用
    
6. Suricata 的规则引擎  
    6.1 规则匹配算法  
    6.1.1 Aho-Corasick 算法的实现  
    6.1.2 多模式匹配优化  
    6.2 规则评估过程  
    6.3 自定义规则开发
    
7. 日志和警报系统  
    7.1 日志格式和存储机制  
    7.2 实时警报生成  
    7.3 与外部系统集成（如 SIEM）
    
8. Suricata 的扩展性  
    8.1 插件系统设计  
    8.2 自定义输出模块开发  
    8.3 与机器学习模型集成
    
9. 调试和故障排除  
    9.1 性能分析工具的使用  
    9.2 常见问题及解决方案  
    9.3 日志分析技巧
    
10. Suricata 在大规模环境中的部署  
    10.1 集群化配置  
    10.2 负载均衡策略  
    10.3 高可用性设计
    
11. 未来发展方向  
    11.1 硬件加速技术的应用  
    11.2 AI/ML 在入侵检测中的应用  
    11.3 适应新兴网络协议和技术