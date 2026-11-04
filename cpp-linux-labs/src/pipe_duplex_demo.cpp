
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
  int parent_to_child[2];
  int child_to_parent[2];

  // 1. 创建两个方向相反的 Pipe
  if (::pipe(parent_to_child) == -1 || ::pipe(child_to_parent) == -1) {
    perror("pipe");
    return 1;
  }

  // 2. 创建子进程
  const pid_t child_pid = ::fork();

  if (child_pid == -1) {
    perror("fork");
    return 1;
  }

  if (child_pid == 0) {
    // Child:
    // 关闭不使用的端点
    ::close(parent_to_child[1]);
    ::close(child_to_parent[0]);

    // 接收父进程的消息
    char buffer[128]{};
    const ssize_t n = ::read(parent_to_child[0], buffer, sizeof(buffer));

    if (n <= 0) {
      ::_exit(1);
    }

    std::cout << "[Child] Received: ";
    std::cout.write(buffer, n);
    std::cout << '\n';
    std::cout.flush();

    // 向父进程回复
    constexpr char response[] = "pong";
    if (::write(child_to_parent[1], response, sizeof(response) - 1) !=
        static_cast<ssize_t>(sizeof(response) - 1)) {
      ::_exit(1);
    }

    ::close(parent_to_child[0]);
    ::close(child_to_parent[1]);

    ::_exit(0);
  }

  // Parent:
  // 关闭不使用的端点
  ::close(parent_to_child[0]);
  ::close(child_to_parent[1]);

  // 发送 ping
  constexpr char message[] = "ping";
  if (::write(parent_to_child[1], message, sizeof(message) - 1) !=
      static_cast<ssize_t>(sizeof(message) - 1)) {
    perror("write");
    return 1;
  }

  ::close(parent_to_child[1]);

  // 接收 pong
  char buffer[128]{};
  const ssize_t n = ::read(child_to_parent[0], buffer, sizeof(buffer));

  if (n <= 0) {
    perror("read");
    return 1;
  }

  std::cout << "[Parent] Received: ";
  std::cout.write(buffer, n);
  std::cout << '\n';

  ::close(child_to_parent[0]);

  int status = 0;
  if (::waitpid(child_pid, &status, 0) == -1) {
    perror("waitpid");
    return 1;
  }

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::cerr << "Child failed\n";
    return 1;
  }

  std::cout << "PASS: duplex pipe communication\n";
  return 0;
}
