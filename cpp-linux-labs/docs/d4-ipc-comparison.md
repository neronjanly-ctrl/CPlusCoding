# D4：Linux 进程间通信与系统资源管理

## 1. IPC 基础知识

IPC（Inter-Process Communication，进程间通信）是指不同进程之间交换数据或协调执行过程的机制。

进程通常拥有相互独立的虚拟地址空间。一个进程修改自己的变量，并不会自动让另一个进程看到相同的修改。因此，需要通过明确的通信机制实现数据共享、消息传递和事件通知。

D4 主要学习三类通信需求：

- **数据共享（Data Sharing）：** 共享内存。
- **事件通知（Event Notification）：** 信号。
- **数据传输（Data Transmission）：** 管道和 Unix Domain Socket。

这些机制承担不同的职责，不能简单地视为可以相互替代的工具。

## 2. 共享内存：mmap()

### 2.1 创建共享内存映射

`mmap` 实验使用：

```cpp
mmap(nullptr, sizeof(SharedData),
     PROT_READ | PROT_WRITE,
     MAP_SHARED | MAP_ANONYMOUS,
     -1, 0);
```

该映射在调用 `fork()` 之前创建。

各参数的含义：

- `nullptr`：由内核选择合适的映射起始地址。
- `sizeof(SharedData)`：指定映射区域的大小。
- `PROT_READ | PROT_WRITE`：允许读取和写入。
- `MAP_SHARED`：共享映射区域的修改可以被其他共享该映射的进程看到。
- `MAP_ANONYMOUS`：匿名映射，不依赖普通文件作为存储载体。
- `-1` 和 `0`：匿名映射不需要有效文件描述符及文件偏移量。

在 `fork()` 之后，父进程和子进程可以访问同一块共享映射区域。

与之不同，`MAP_PRIVATE` 使用写时复制（Copy-on-Write）语义，不适合用于实现父子进程之间的修改共享。

### 2.2 将映射内存转换为 C++ 结构体指针

`mmap()` 返回的类型是 `void*`。

实验中需要将返回的地址转换为指向共享结构体的指针：

```cpp
auto* shared = static_cast<SharedData*>(mapping);
```

这里的类型转换为映射内存提供了一个具有明确类型的访问接口。

父进程和子进程分别通过自己的指针变量访问共享数据。

需要注意：

- 两个进程中的指针变量属于各自的进程。
- 指针所访问的底层映射区域是共享的。
- 指针本身不是数据共享的实现机制，真正实现共享的是 `MAP_SHARED` 映射。

### 2.3 进程共享互斥锁

共享内存并不自动提供同步机制。

实验使用：

```cpp
pthread_mutexattr_setpshared(
    &attr,
    PTHREAD_PROCESS_SHARED
);
```

该配置将互斥锁设置为支持进程间共享同步。

不能直接假设默认的进程私有互斥锁可以作为进程间共享同步原语使用。

实验中，父进程和子进程分别执行 100,000 次计数器递增操作。

预期输出：

```text
Parent increments: 100000
Child increments: 100000
Final counter: 200000
PASS: shared memory synchronization
```

这个实验说明：

**数据共享与数据同步是两个独立的问题。**

共享内存解决不同进程如何访问同一份数据的问题，互斥锁解决多个执行主体如何安全地修改这份数据的问题。

## 3. 信号：事件通知机制

### 3.1 注册信号处理函数

信号实验使用 `sigaction()` 注册信号处理函数：

```cpp
struct sigaction action {};

action.sa_handler = handle_sigusr1;
sigemptyset(&action.sa_mask);
action.sa_flags = 0;

sigaction(SIGUSR1, &action, nullptr);
```

信号处理函数负责记录信号到达：

```cpp
volatile std::sig_atomic_t signal_received = 0;

void handle_sigusr1(int signal_number) {
    if (signal_number == SIGUSR1) {
        signal_received = 1;
    }
}
```

这里的处理函数只修改一个简单的状态标志，而不执行复杂操作。

### 3.2 避免信号丢失唤醒

信号可能在父进程检查标志位之后、真正开始等待之前到达。

如果没有正确设置屏蔽机制，信号处理函数可能已经执行完毕，但父进程随后仍然进入等待状态，等待下一次信号，导致一直阻塞。

实验使用：

```cpp
sigprocmask(SIG_BLOCK, &blocked_set, &original_mask);
```

暂时屏蔽 `SIGUSR1`。

然后准备等待期间使用的信号屏蔽集合：

```cpp
sigset_t wait_mask = original_mask;
sigdelset(&wait_mask, SIGUSR1);
```

父进程通过以下函数等待信号：

```cpp
sigsuspend(&wait_mask);
```

`sigsuspend()` 会原子地切换为临时信号屏蔽集合，并进入等待状态。

当信号到达并处理后，函数返回，并恢复原来的信号屏蔽集合。

这样可以避免检查标志位与开始等待之间的时序竞争。

### 3.3 信号机制的职责

子进程通过：

```cpp
kill(parent_pid, SIGUSR1);
```

向父进程发送 `SIGUSR1`。

信号主要用于传递事件通知，而不是传输任意的数据内容。

父进程通过信号处理函数更新状态，然后继续执行正常程序逻辑。

需要区分：

- **Signal：** 通知某个事件已经发生。
- **Shared Memory：** 在进程之间共享实际数据。
- **Mutex：** 协调共享数据的并发访问。

## 4. Pipe：单向进程间通信

### 4.1 创建管道

```cpp
int pipefd[2];

pipe(pipefd);
```

一个 Pipe 提供两个文件描述符：

- `pipefd[0]`：读取端。
- `pipefd[1]`：写入端。

正常的数据传递方向为：

```text
写入进程 → Pipe 缓冲区 → 读取进程
```

父进程调用 `fork()` 创建子进程后，子进程会继承相关文件描述符。

每个进程都应该关闭自己不使用的管道端点。

### 4.2 EOF 的产生条件

阻塞模式下的 `read()` 有三种重要情况：

| 当前状态 | read() 的行为 |
|---|---|
| 缓冲区存在数据 | 返回实际读取的字节数 |
| 缓冲区为空，但仍有写入端打开 | 等待新的数据 |
| 缓冲区为空，且所有写入端均已关闭 | 返回 0，即 EOF |

关闭写入端并不会直接丢弃已经进入缓冲区的数据。

读取进程仍然可以继续读取剩余数据，直到缓冲区为空。

只有当缓冲区已经读空，并且所有指向该管道写入端的文件描述符引用都已关闭时，读取进程才能收到 EOF。

### 4.3 read() 和 write() 的返回值

一次成功的 `read()` 不一定能够读取请求的全部字节。

同样，一次 `write()` 也不一定能够完成全部数据写入。

因此，在需要保证完整传输的场景中，通常需要使用循环记录已经读取或写入的字节数，直到完成传输或遇到错误。

## 5. Duplex Pipe：双向管道通信

单个 Pipe 只能提供一个方向的数据传递。

为了实现双向通信，实验创建两个方向相反的 Pipe：

```cpp
int parent_to_child[2];
int child_to_parent[2];

pipe(parent_to_child);
pipe(child_to_parent);
```

两个管道分别负责：

```text
父进程 → parent_to_child → 子进程

父进程 ← child_to_parent ← 子进程
```

调用 `fork()` 后，各进程保留和关闭的端点如下：

| 进程 | 保留的端点 | 关闭的端点 |
|---|---|---|
| Parent | `parent_to_child[1]`、`child_to_parent[0]` | `parent_to_child[0]`、`child_to_parent[1]` |
| Child | `parent_to_child[0]`、`child_to_parent[1]` | `parent_to_child[1]`、`child_to_parent[0]` |

实验通过两个管道交换两条消息：

```text
ping
pong
```

关闭不需要的端点对于正确处理 EOF 和释放系统资源非常重要。

如果父进程保留了一个不需要的写入端，而子进程又需要通过对应的读取端判断 EOF，就可能导致子进程一直等待。

## 6. Unix Domain Socket：socketpair()

### 6.1 创建已经连接的 Socket Pair

实验使用：

```cpp
int sv[2];

socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
```

参数含义：

| 参数 | 含义 |
|---|---|
| `AF_UNIX` | 使用本地 Unix Domain Socket 地址族 |
| `SOCK_STREAM` | 使用可靠的、面向连接的字节流通信方式 |
| `0` | 使用默认协议 |
| `sv` | 接收创建成功的两个 Socket 文件描述符 |

内核会创建两个已经建立连接关系的 Socket 端点。

与 Pipe 不同，两个 Socket 端点都支持读取和写入。

```text
Socket 端点 A ⇄ Socket 端点 B
```

由于 `socketpair()` 已经创建了相互连接的端点，因此不需要额外调用 `bind()`、`listen()`、`accept()` 或 `connect()`。

### 6.2 fork() 之后的通信

调用 `fork()` 后：

- 父进程关闭 `sv[1]`，使用 `sv[0]`。
- 子进程关闭 `sv[0]`，使用 `sv[1]`。

两个端点都可以独立调用 `read()` 和 `write()`。

实验交换的数据为：

```text
Parent → Child: ping
Child → Parent: pong
```

### 6.3 SOCK_STREAM 不提供消息边界

`SOCK_STREAM` 提供的是连续字节流。

一次 `write()` 并不保证对应一次 `read()`。

例如：

```cpp
write(fd, "hello", 5);
```

接收方可能一次读取 5 个字节，也可能分多次读取。

同样，连续发送的多条消息也可能被合并到一次读取操作中。

因此，需要根据具体的通信协议设计完整读取和写入机制，正确处理部分传输和 `EINTR` 等情况。

这也是 D5 TCP 网络编程中需要重点掌握的知识。

## 7. 文件描述符生命周期管理

### 7.1 三个不同的概念层次

理解 IPC，需要区分三个概念：

1. **文件描述符（File Descriptor，FD）：** 进程内部使用的整数编号。
2. **打开文件描述（Open File Description，OFD）：** 内核管理的打开实例，保存相关打开状态。
3. **底层资源（Underlying Resource）：** 实际对应的普通文件、Pipe 端点或 Socket 端点等内核资源。

可以用下面的关系理解：

```text
进程 FD 表
    |
    v
文件描述符表项
    |
    v
打开文件描述（OFD）
    |
    v
底层内核资源
```

FD 是访问资源的入口，而不是资源本身。

### 7.2 fork() 与文件描述符继承

调用 `fork()` 之前：

```text
Parent FD 3 → OFD A
Parent FD 4 → OFD B
```

调用 `fork()` 之后：

```text
Parent FD 3 → OFD A
Parent FD 4 → OFD B

Child  FD 3 → OFD A
Child  FD 4 → OFD B
```

父进程和子进程拥有相互独立的文件描述符表。

但是，继承得到的对应文件描述符会引用相同的内核打开文件描述。

对于 `socketpair()`，两个端点本身是不同的资源，只是每个端点都可以被父进程和子进程分别继承和引用。

这解释了为什么父进程关闭自己的 `sv[1]`，并不会自动关闭子进程的 `sv[1]`。

### 7.3 dup2() 与文件描述符重定向

例如：

```cpp
dup2(pipefd[1], STDOUT_FILENO);
close(pipefd[1]);
```

假设：

```text
pipefd[1] = FD 4
STDOUT_FILENO = FD 1
```

执行：

```cpp
dup2(4, 1);
```

会让 FD 1 指向与 FD 4 相同的打开文件描述。

因此：

| FD | 指向 |
|---|---|
| FD 4 | 同一个 OFD |
| FD 1 | 同一个 OFD |

随后关闭 FD 4，并不会关闭 FD 1。

子进程可以继续执行另一个外部程序。该程序向标准输出写入数据时，数据就会进入 Pipe。

### 7.4 close() 与资源生命周期

`close(fd)` 关闭的是调用该函数的进程所持有的文件描述符。

它不会直接删除其他进程文件描述符表中的对应表项。

当其他文件描述符仍然引用同一个打开文件描述时，底层资源通常不会因为这一次 `close()` 而立即释放。

最终资源是否能够释放，还取决于底层内核资源自身的状态及内部引用关系。

## 8. Socket 半关闭：shutdown()

Socket 的 `close()` 和 `shutdown()` 承担不同职责。

### 8.1 close()

```cpp
close(sv[0]);
```

关闭当前进程持有的文件描述符。

如果其他文件描述符仍然引用同一个 Socket 端点，那么该端点不一定会立即释放。

### 8.2 shutdown()

```cpp
shutdown(sv[0], SHUT_WR);
```

`shutdown()` 改变 Socket 端点的通信状态，而不是简单地删除一个 FD 表项。

| 操作 | 含义 |
|---|---|
| `SHUT_RD` | 关闭本地端点的接收方向 |
| `SHUT_WR` | 关闭本地端点的发送方向 |
| `SHUT_RDWR` | 同时关闭发送和接收方向 |

例如：

```cpp
write(sv[0], "ping", 4);
shutdown(sv[0], SHUT_WR);
```

这表示父进程已经完成发送，但仍然可以接收子进程的数据。

对端可以继续读取已经发送的数据，读取完毕后，在对端接收方向观察到 EOF。

这种只关闭一个通信方向、保留另一个通信方向的机制称为：

**半关闭（Half-close）。**

与 `close()` 不同，`shutdown()` 作用于共享的 Socket 端点，因此也会影响其他引用同一个端点的文件描述符。

## 9. IPC 通信机制对比

| 通信机制 | 主要职责 | 核心特点 |
|---|---|---|
| Shared Memory（共享内存） | 数据共享 | 多个进程访问同一块映射内存 |
| Signal（信号） | 事件通知 | 轻量级异步事件通知 |
| Pipe（管道） | 数据传输 | 单向字节流 |
| Duplex Pipe（双向管道） | 数据传输 | 两个 Pipe 实现两个通信方向 |
| Unix Domain Socket | 数据传输 | 双向、面向连接的本地字节流通信 |

### 机制选择与适用场景

- **共享内存：** 适用于多个进程需要直接访问同一块数据区域的场景，但需要单独设计同步机制。
- **信号：** 适用于事件通知，不适合承担大量数据传输任务。
- **Pipe：** 适用于简单的单向进程间通信。
- **双 Pipe：** 通过两个单向管道实现双向通信。
- **Unix Domain Socket：** 适用于本机进程之间的双向通信，也提供了适合本地 Client/Server 设计的 Socket 接口。

## 10. D4 核心知识总结

D4 最重要的知识点：

1. 共享内存与同步是两个不同的问题。
2. 信号负责通知进程发生了某个事件，不能代替实际的数据传输通道。
3. Pipe 是单向通信机制，而 Socket Pair 的每个端点都支持双向通信。
4. `fork()` 创建独立的文件描述符表，但继承的文件描述符可以引用相同的内核打开文件描述。
5. `dup2()` 可以创建新的文件描述符，使其引用相同的打开文件描述。
6. `close()` 关闭调用进程持有的文件描述符，不代表底层资源一定立即销毁。
7. Pipe 的 EOF 需要同时满足缓冲区为空、所有写入端引用均已关闭。
8. Socket 的 `shutdown()` 可以结束一个通信方向，同时保留另一个通信方向。

### 相关源码与实验

- `src/mmap_demo.cpp`
- `src/signal_demo.cpp`
- `src/pipe_demo.cpp`
- `src/pipe_exec_demo.cpp`
- `src/pipeline_demo.cpp`
- `src/pipe_duplex_demo.cpp`
- `src/socketpair_demo.cpp`

相关实验文档：

- `docs/d4-mmap.md`
- `docs/d4-signal-learning.md`

### D5 学习衔接

D4 建立了 Linux 本地进程间通信的基础。

D5 将从本地 IPC 进入 TCP 网络编程，进一步学习：

- IPv4 Socket API。
- TCP Client/Server 架构。
- TCP 字节流与数据传输。
- 应用层消息边界与协议设计。
- TCP 粘包与拆包处理。
