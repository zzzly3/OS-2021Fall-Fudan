# LY's Reminder

此处记录一些内核编程规范，以免遗忘

## A

为了避免中断嵌套造成死锁的问题，采用了三种方案：

1. 部分资源必须关闭中断访问
2. 部分资源只能在指定运行级别下访问
3. 部分资源只支持非阻塞访问

此外，部分资源保证同一时间只被一个处理器使用，只要指定运行级别（甚至特殊情况可以不指定）就可以不加锁。

下面列出各种资源（仅含共享的部分）需要的访问模式，以免遗忘

* Locked：加锁并关中断（因为开关中断有性能损失，频繁使用的资源不采用此模式）
* Fast locked：加锁但不关中断，效率更高
* Non-block：访问是非阻塞的， 不存在中断嵌套引起的死锁问题
* No share：资源是按核心分配的，不需要加锁
* ✓：直接访问
* ○：关闭中断或提升级别访问
* □：关闭中断访问
* ×：禁止访问

| Resource         | Access mode | USR  | APC  | RT   | ISR  |
| ---------------- | ----------- | ---- | ---- | ---- | ---- |
| Device Directory | Fast locked | ○    | ○    | ✓    | ×    |
| Console          | Locked | ✓    | ✓    | ✓    | ✓    |
| PMem Manager     | Fast locked | □    | □    | □    | ✓    |
| Object Pool      | Fast locked | □    | □    | □    | ✓    |
| Memory Space     | Fast locked | □    | □    | □    | ✓    |
| Process List     | Fast locked | ○    | ○    | ✓ | ×    |
| Process Object | Locked      | ✓   | ✓   | ✓    | ✓    |
| DPC List         | Locked      | ✓    | ✓    | ✓    | ✓    |
| Scheduler      | No share    | ○    | ○    | ✓    | ×    |

## B

关于进程状态

原则上KPROCESS.Status的修改似乎应当加锁，但考虑到只要设计的时候小心一些，便不存在多核同时更改进程状态的情形，因此做了无锁设计。下面列出了不同进程状态间切换的情形。

* RUNNING - WAITING 调度器switch out
* WAITING - RUNNING 调度器switch in
* RUNNING - BLOCKED 进程自身wait（状态切换是整个wait原子操作的一部分）
* BLOCKED - WAITING signal

进程状态与其处于调度器的Active list还是Inactive list并无直接联系。后者由调度器依据进程状态进行管理，并非一一对应。
