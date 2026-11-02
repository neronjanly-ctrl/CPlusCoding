# D4 Linux Signal Handling：误解、正确认识与机制总结

## 1. 实验目标

通过 `signal_demo.cpp` 理解 Linux 进程间信号通信，以及如何使用 `sigaction()`、`sigprocmask()` 和 `sigsuspend()` 实现可靠的信号等待。

本实验使用父子进程作为通信双方：

- 父进程注册 `SIGUSR1` 信号处理函数。
- 父进程暂时屏蔽 `SIGUSR1`，避免信号提前递送。
- 子进程通过 `kill()` 向父进程发送 `SIGUSR1`。
- 父进程通过 `sigsuspend()` 等待信号。
- 信号处理函数修改状态标志，通知主程序信号已经处理。
- 父进程通过 `waitpid()` 等待并回收子进程。

实验重点不是传输大量数据，而是理解异步信号通知机制及其时序控制。

---

## 2. 学习过程中出现的主要误解

### 误解一：子进程发送信号，父进程必须正在等待才能收到

**正确认识：**

子进程调用：

```cpp
::kill(parent_pid, SIGUSR1);
```

只是向指定进程发送信号，不要求目标进程此时正在执行 `sigsuspend()`。

信号是否立即递送，取决于目标进程当前的信号屏蔽状态。

- 如果 SIGUSR1 没有被屏蔽，信号可以被递送，执行已经注册的处理函数。
- 如果 SIGUSR1 被屏蔽，信号会处于 pending（待处理）状态。
- 当信号之后解除屏蔽时，待处理信号可以被递送。

因此：

**发送信号不等于执行信号处理函数，信号递送也不要求接收方主动调用接收函数。**

### 误解二：fork() 之后父进程一定先执行屏蔽操作

**正确认识：**

`fork()` 创建子进程后，父子进程独立调度，不能保证谁先获得 CPU。

例如：

1. 父进程调用 `fork()`。
2. 子进程先获得 CPU。
3. 子进程执行 `kill(parent_pid, SIGUSR1)`。
4. 父进程此时还没有执行 `sigprocmask()`。

如果父进程没有提前屏蔽 SIGUSR1，信号可能已经被处理。

因此实验将屏蔽操作放在 `fork()` 之前：

```cpp
sigprocmask(SIG_BLOCK, &blocked_set, &original_mask);

pid_t child_pid = ::fork();
```

这样能够保证子进程获得发送机会之前，父进程已经完成信号屏蔽准备。

补充：子进程会继承父进程的信号屏蔽集合，但这不会阻止子进程向父进程发送 SIGUSR1。信号屏蔽控制的是当前执行线程接收信号的递送，而不是限制其发送信号的能力。

### 误解三：sigemptyset()、sigaddset() 和 sigdelset() 本身就在屏蔽信号

**正确认识：**

这些函数只负责操作 `sigset_t` 信号集合，并不会直接改变当前进程的信号屏蔽状态。

| 函数 | 作用 |
|---|---|
| `sigemptyset()` | 初始化空信号集合 |
| `sigaddset()` | 向集合添加指定信号 |
| `sigdelset()` | 从集合删除指定信号 |
| `sigprocmask()` | 修改当前执行线程的信号屏蔽集合 |
| `sigsuspend()` | 临时替换信号屏蔽集合并等待信号 |

真正执行屏蔽的是：

```cpp
sigprocmask(SIG_BLOCK, &blocked_set, &original_mask);
```

### 误解四：blocked_set 和 wait_mask 是同一个作用

**正确认识：**

两者承担不同职责。

```cpp
sigset_t blocked_set;
sigset_t original_mask;

::sigemptyset(&blocked_set);
::sigaddset(&blocked_set, SIGUSR1);

::sigprocmask(
    SIG_BLOCK,
    &blocked_set,
    &original_mask
);
```

这里：

- `blocked_set`：指定需要屏蔽的 SIGUSR1。
- `original_mask`：保存执行屏蔽操作之前的信号屏蔽集合。
- `sigprocmask(SIG_BLOCK, ...)`：真正将 SIGUSR1 加入当前屏蔽集合。

随后：

```cpp
sigset_t wait_mask = original_mask;
::sigdelset(&wait_mask, SIGUSR1);
```

`wait_mask` 是另一个集合，用于指定等待期间的信号屏蔽状态。

假设 SIGUSR1 在原始集合中没有被屏蔽：

| 集合或状态 | SIGUSR1 |
|---|---|
| `blocked_set` | 包含 |
| `original_mask` | 不包含 |
| 当前屏蔽集合 | 包含 |
| `wait_mask` | 不包含 |

特别注意：创建 `wait_mask` 不会立即解除信号屏蔽，它只是准备传递给 `sigsuspend()` 的参数。

### 误解五：sigsuspend() 只是普通的等待函数

**正确认识：**

`sigsuspend()` 有两个关键作用：

1. 临时将当前信号屏蔽集合替换为传入的 `wait_mask`。
2. 进入等待，直到有未被屏蔽的信号被递送。

```cpp
while (signal_received == 0) {
    ::sigsuspend(&wait_mask);
}
```

它最重要的价值是将切换信号屏蔽集合与进入等待作为一个原子操作完成。

这解决了检查状态与进入等待之间可能出现的竞态。

### 误解六：sigsuspend() 直接调用信号处理函数

**正确认识：**

`sigsuspend()` 不负责决定收到信号后执行哪个函数。

这个职责属于 `sigaction()`。

```cpp
action.sa_handler = handle_sigusr1;

::sigaction(SIGUSR1, &action, nullptr);
```

操作系统通过之前注册的信号处理方式，决定 SIGUSR1 递送时执行：

```cpp
void handle_sigusr1(int signal_number) {
    if (signal_number == SIGUSR1) {
        signal_received = 1;
    }
}
```

两者之间的关系：

- `sigaction()`：注册信号处理函数。
- `sigsuspend()`：允许信号递送并等待。
- 操作系统：负责在信号递送时调用注册的处理函数。

`sigsuspend()` 在处理函数返回后结束等待，恢复调用前的信号屏蔽集合，并返回 `-1`，通常设置 `errno = EINTR`。

---

## 3. 信号丢失唤醒问题及解决方案

### 3.1 不屏蔽信号时可能发生的错误时序

假设父进程执行：

```cpp
while (signal_received == 0) {
    ::sigsuspend(&wait_mask);
}
```

可能出现：

| 时刻 | 父进程 | 子进程 |
|---|---|---|
| T1 | 检查 `signal_received == 0`，结果为 true | |
| T2 | 尚未执行 `sigsuspend()` | 发送 SIGUSR1 |
| T3 | 信号处理函数执行，将标志设置为 1 | |
| T4 | 从信号处理函数返回 | |
| T5 | 继续执行已选定的循环体，调用 `sigsuspend()` | |
| T6 | 阻塞等待新的信号 | 已经退出 |

问题在于：

父进程已经处理过 SIGUSR1，但仍然进入等待，可能一直无法结束。

这就是 Lost Wakeup（丢失唤醒时机）。

### 3.2 使用信号屏蔽解决问题

首先：

```cpp
sigprocmask(SIG_BLOCK, &blocked_set, &original_mask);
```

提前屏蔽 SIGUSR1。

如果子进程提前发送：

```cpp
kill(parent_pid, SIGUSR1);
```

信号将保持 pending 状态，而不会立即执行处理函数。

然后：

```cpp
sigsuspend(&wait_mask);
```

原子地切换信号屏蔽集合并进入等待。

之前 pending 的 SIGUSR1 此时可以被递送，执行处理函数。

处理函数将：

```cpp
signal_received = 1;
```

最后 `sigsuspend()` 返回，主程序重新检查标志并退出循环。

因此，两者的职责是：

- `SIG_BLOCK`：提前阻止信号递送，保留提前到达的信号。
- `sigsuspend()`：在正确的等待时机临时解除屏蔽并等待。

---

## 4. sigaction() 与状态标志的设计关系

### 4.1 为什么需要 signal_received？

```cpp
volatile std::sig_atomic_t signal_received = 0;
```

它是主程序与信号处理函数之间共享的简单状态标志。

主程序：

```cpp
while (signal_received == 0) {
    ::sigsuspend(&wait_mask);
}
```

信号处理函数：

```cpp
void handle_sigusr1(int signal_number) {
    if (signal_number == SIGUSR1) {
        signal_received = 1;
    }
}
```

这里的共享发生在同一个父进程内部，不是 `mmap()` 实验中的父子进程共享内存。

### 4.2 为什么使用 volatile std::sig_atomic_t？

- `std::sig_atomic_t`：用于信号处理场景中的简单状态访问，保证符合要求的简单访问不会出现被信号处理过程打断而产生的不完整访问。
- `volatile`：避免编译器将这个可能被异步修改的状态简单视为始终不变。

注意：`volatile` 不是通用线程同步工具，不能替代 `mutex` 或原子操作。

### 4.3 为什么处理函数不直接输出？

不建议在异步信号处理函数中调用 `std::cout` 等复杂库函数。

因此处理函数只负责：

```cpp
signal_received = 1;
```

主程序恢复正常执行后再输出：

```cpp
if (signal_received == 1) {
    std::cout << "[Parent] Received SIGUSR1\n";
}
```

这种设计将异步事件处理与正常业务逻辑分离。

---

## 5. signal_demo.cpp 的完整执行逻辑

```text
父进程启动
    |
    v
初始化 signal_received = 0
    |
    v
sigaction()
注册 SIGUSR1 的处理函数
    |
    v
sigprocmask(SIG_BLOCK)
提前屏蔽 SIGUSR1
    |
    v
准备 wait_mask
    |
    v
fork()
    |
    +-----------------------+
    |                       |
    v                       v
  子进程                   父进程
    |                       |
    v                       v
kill(parent_pid, SIGUSR1)  检查 signal_received
    |                       |
    |                       v
    |                    sigsuspend()
    |                       |
    +---- SIGUSR1 ----------+
                            |
                            v
                  操作系统递送 SIGUSR1
                            |
                            v
                    handle_sigusr1()
                            |
                            v
                  signal_received = 1
                            |
                            v
                    sigsuspend() 返回
                            |
                            v
                  恢复原信号屏蔽状态
                            |
                            v
                    检查状态标志
                            |
                            v
                  输出 Received SIGUSR1
                            |
                            v
                        waitpid()
                            |
                            v
                     回收子进程
```

`waitpid()` 与信号等待承担不同职责：

- `sigsuspend()`：等待信号递送。
- `waitpid()`：等待子进程结束，并回收子进程资源。

收到 SIGUSR1 不等于子进程已经完成退出。

---

## 6. 关键函数职责对照

| 函数 | 核心职责 |
|---|---|
| `sigaction()` | 注册信号处理方式 |
| `sigemptyset()` | 初始化空信号集合 |
| `sigaddset()` | 向信号集合添加信号 |
| `sigdelset()` | 从信号集合删除信号 |
| `sigprocmask()` | 修改当前执行线程的信号屏蔽集合 |
| `sigsuspend()` | 临时切换信号屏蔽集合并等待信号 |
| `kill()` | 向指定进程发送信号 |
| `getpid()` | 获取当前进程 PID |
| `getppid()` | 获取父进程 PID |
| `waitpid()` | 等待并回收子进程 |
| `_exit()` | 直接终止当前进程 |

---

## 7. 本实验需要记住的设计原则

1. 信号发送与信号递送是不同的概念。
2. 子进程与父进程的执行顺序不能依赖。
3. 屏蔽信号不会丢弃信号，信号可以保持 pending 状态。
4. `sigset_t` 是信号集合，集合操作函数不会直接改变当前屏蔽状态。
5. `sigaction()` 负责建立信号与处理函数的对应关系。
6. `sigsuspend()` 负责原子地切换信号屏蔽集合并等待。
7. `volatile std::sig_atomic_t` 适合当前实验的简单信号状态标记。
8. 信号处理函数应尽量简单，将复杂业务处理留给主程序。
9. `sigsuspend()` 与 `waitpid()` 的等待对象不同，不能混为一谈。

补充注意：SIGUSR1 属于标准信号，多次相同信号在 pending 期间可能合并，不应将其当作可靠的消息队列使用。需要传递具体数据时，应考虑 pipe、共享内存或其他 IPC 机制。

---

## 8. 实验结论

本实验实现了基于 SIGUSR1 的父子进程异步通知机制。

通过 `sigaction()` 注册处理函数，使用 `sigprocmask()` 提前屏蔽信号，并利用 `sigsuspend()` 原子地解除屏蔽和等待，解决了信号在状态检查与进入等待之间提前递送所导致的丢失唤醒问题。

同时，通过 `volatile std::sig_atomic_t` 在信号处理函数与主程序之间传递简单状态，将异步信号处理与正常业务逻辑分离。

这套机制为理解 Linux 信号递送、进程同步和异步事件处理提供了基础。

## 9. 本机证据

- `d4-signal-output.txt`：程序输出、子进程退出码与 PASS 结果。
- `d4-signal.strace`：`sigaction`、信号屏蔽、`kill`、信号递送和 `waitpid` 的系统调用证据。

这些文件证明当前正常路径在本机执行成功，不代表已经覆盖信号合并、多个发送者或处理函数重入等扩展场景。
