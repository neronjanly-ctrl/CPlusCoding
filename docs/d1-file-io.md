# D1：文件偏移与 errno 实验

范围：10 周路线第 1 周 D1 下午的文件 I/O、FD 和 errno。
本次只完成文件偏移与不存在文件的错误处理，不推进进程、线程或网络实验。

## 本地检查结论

- 修改前 `src/file_io_demo.cpp` 只有 open/write/close，未实现 read/lseek 或缺失文件测试。
- 旧 strace 已证明目标文件 close 返回 0。
- 原 `build/` 使用 Unix Makefiles，CMAKE_BUILD_TYPE 为空，不能视作已配置 Debug。
- 新建 `build-debug/`：Ninja、Debug、GCC 13.3.0，编译命令含 -g 与三项警告。
- 原 ENABLE_SANITIZERS 只配置 hello；现将 file_io_demo 也纳入编译和链接选项。
- build-asan 的 file_io_demo 已出现 -fsanitize=address,undefined，CTest 通过。
- Debug 与 ASan/UBSan 各运行 1 项 CTest，该项同时验证两个实验。
- 原 build-release 缓存为 Release/Ninja；本次未重新运行 Release，不视为新增实验已在 Release 验证。
- mininet、linkcore、edge-gateway、interview-notes 在此次目录检查中均无文件，仍为占位目录。
- 本次修改尚未提交或推送；最近已有提交为 0a3fb68。

## 实际问题与需要

同一个普通文件刚写完，为什么马上 read 却读不到内容？
因为读写并非每次自动从文件开头开始。内核为打开文件描述维护当前位置，
本次同一 fd 的 read 和 write 都使用这个偏移。偏移不是 C++ buffer 的下标。

文件不存在时，为什么只看返回 -1 还不够？
返回值说明调用失败，errno 才说明失败原因。不存在、权限不足、FD 耗尽都可能使 open 失败。
本次只实际复现 ENOENT，未注入其余错误。

## 文件偏移：代码与状态

1. open(O_CREAT | O_TRUNC | O_RDWR)：打开实验文件，原内容被清空，偏移从 0 开始。
2. write(fd, message, 25)：本次写入 25 字节，偏移变为 25。
3. lseek(fd, 0, SEEK_CUR)：相对当前位置移动 0 字节，用于查询偏移，返回 25。
4. read(fd, buffer, 128)：最多读 128 字节，但当前位置已在末尾，返回 0。
5. lseek(fd, 0, SEEK_SET)：将偏移设置为从文件开头计算的 0，返回新偏移 0。
6. read(fd, buffer, 128)：本次读取 25 字节，偏移增加至 25。
7. lseek(fd, 0, SEEK_CUR)：再查询，返回 25。
8. close(fd)：释放当前进程的 FD 入口，成功返回 0。

不执行第 5 步，且文件不增长，继续从末尾读取仍会返回 0。
lseek 不会清空文件，也不会将文件内容复制到用户缓冲区。
read 返回 0 是 EOF；返回 -1 才是本实验 API 的错误，需要检查 errno。
第二次请求 128、返回 25 是成功的短读，因为文件只有 25 字节。

read 不会追加字符串终止符。代码使用 cout.write(buffer, second_read)，
只输出实际读取的字节；不能把未初始化的 buffer 直接当作以空字符结尾的字符串。
ssize_t 能表示非负字节数和 -1；off_t 表示文件偏移。

## errno：代码与结果

测试前已确认 `/tmp/cpp_linux_labs_missing_for_errno.txt` 不存在。
程序用 O_RDONLY 打开它，不加 O_CREAT，避免在允许创建时把失败条件消除。

```cpp
const int missing_fd = ::open(kMissingPath, O_RDONLY);
// 先处理意外成功的分支；确认 missing_fd == -1 后：
const int error_number = errno;
// 再记录返回值、error_number、std::strerror(error_number)。
// 最后比较 error_number == ENOENT。
```

本机结果为返回 -1、errno=2、描述 No such file or directory。
ENOENT 是错误常量名，2 是此次平台上的数值；业务判断应写 ENOENT，而不是硬编码 2。
strerror 将保存的错误码转换成说明文字，不进行 open，也不修复错误。

在输出日志或清理资源之前保存 errno，是因为后续函数可能修改它。
本次实验验证了保存和报告 ENOENT；并未单独演示 errno 被覆盖的过程。
不要在调用成功后根据非零 errno 判定失败，成功调用不保证清零旧值。
open 失败没有获得有效 fd，不能对 -1 调用 close 当作正常资源清理。

预期的 open 失败是测试成功的一部分。程序在成功验证 ENOENT 后返回 0；
意外打开成功、错误码不匹配或偏移/内容不匹配都会返回非零，让 CTest 判失败。

## 复现命令与用途

在 cpp-linux-labs 根目录执行；固定 /tmp 文件仅用于本实验，每次会截断数据文件。
不要同时运行多个副本。如果缺失文件测试路径已存在，应选择新的不存在路径，勿删除未知文件。

```bash
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
./build-debug/file_io_demo
ctest --test-dir build-debug --output-on-failure
```

- -S 指定源码位置，-B 指定构建目录，-G Ninja 选择构建工具，Debug 启用调试配置。
- cmake --build 编译已配置目标；不重新构建，就可能运行旧二进制而看不到源码修改。
- ./build-debug/file_io_demo 执行本次目标；继续运行旧 build/file_io_demo 可能仍是旧实验。
- CTest 不编译代码，负责执行注册的程序并检查退出状态；--output-on-failure 显示失败详情。
- CMake 的 foreach 只是把两项目标应用相同的 Sanitizer 选项，本阶段无需深入 CMake 语法。

```bash
strace -yy -o docs/d1-file-io.strace \
  -e trace=openat,read,write,lseek,close ./build-debug/file_io_demo
```

- strace 观察内核系统调用；不用它仍能运行实验，但缺少独立的调用参数和返回值证据。
- -yy 给 FD 附加路径等信息，便于区分重复使用的编号，参数可查阅。
- -o 保存跟踪日志；-e trace=... 选择需要观察的调用，不列出的调用不会出现在这份日志中。
- 保存的程序输出在 d1-file-io-output.txt，完整跟踪在 d1-file-io.strace。

## 谁在做什么

- Shell：解析命令、启动程序、设置管道。采集时使用 tee 同时显示与保存 stdout。
- CMake/Ninja/G++：在运行之前配置规则、组织编译、生成可执行程序，不负责运行时的文件偏移。
- C/C++ 运行时：提供 open/read/lseek 等函数接口以及 cout/strerror；本机 open 最终使用 openat。
- Linux 内核：管理 FD、打开文件描述和文件偏移，执行实际文件操作并返回调用结果。
- libc 包装层：在出错时向程序返回 -1 并设置当前线程的 errno；strace 将错误解码为 ENOENT。

这次完整日志最后才出现一次 write(1<pipe:...>, ..., 329)，原因是采集输出经过管道，
标准输出缓冲将多次 cout 的内容一起提交。日志中显示的 ... 是 strace 的字符串展示截断，
不是程序只输出了前面几个字符。不能据此认为源码里的 cout 都在 open 失败后才执行。
write(3<实验文件>, ..., 25) 是写数据文件；write(1<pipe:...>, ..., 329) 是打印实验说明。

## 验收与边界

已验证：两次 read 分别返回 0/25，lseek 返回 25/0/25，读取内容匹配，close 返回 0，
缺失路径 open 返回 -1/ENOENT，Debug 与 ASan/UBSan 的测试均通过。
Sanitizer 没有报告本次路径的错误，不等于程序不存在所有潜在问题。

需学习者独立解释：为何文件有数据却读到 EOF、lseek 返回什么、为何请求 128 只读到 25、
为何只在失败后读 errno、为何立即保存它、为何预期的系统调用失败最终使测试通过。

尚未实践：短写循环与 EINTR 重试、错误码被后续调用覆盖的独立复现、FD 复用误操作。
当前 demo 遇到短写会明确失败，尚未实现通用 write_all；小文件单次读取不是通用文件读取器。
清理失败路径中的 close 为尽力释放资源，正常路径则检查 close 返回值。
原 D1 的数组/哈希算法练习在本仓库没有完成证据。

MiniNet 会复用 FD 生命周期、实际读写字节数和 errno 分类这些基础；
但 socket 是字节流，不能用 lseek 回到开头。LinkCore 的文件配置与日志诊断也依赖这些能力。
本轮只建立连接，不提前实现网络模块。

参考：
- https://man7.org/linux/man-pages/man2/lseek.2.html
- https://man7.org/linux/man-pages/man2/read.2.html
- https://man7.org/linux/man-pages/man3/errno.3.html
