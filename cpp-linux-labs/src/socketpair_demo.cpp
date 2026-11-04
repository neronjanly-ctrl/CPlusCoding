
#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// 保证指定长度的数据全部写入
bool write_all(int fd, const char *data, size_t size) {
  size_t total = 0;

  while (total < size) {
    ssize_t n = ::write(fd, data + total, size - total);

    if (n > 0) {
      total += static_cast<size_t>(n);
    } else if (n == -1 && errno == EINTR) {
      continue;
    } else {
      return false;
    }
  }

  return true;
}

// 保证读取到指定长度的数据
bool read_exact(int fd, char *buffer, size_t size) {
  size_t total = 0;

  while (total < size) {
    ssize_t n = ::read(fd, buffer + total, size - total);

    if (n > 0) {
      total += static_cast<size_t>(n);
    } else if (n == -1 && errno == EINTR) {
      continue;
    } else {
      return false;
    }
  }

  return true;
}

int main() {
  int sv[2];

  // 1. 创建一对 Unix Domain Socket
  if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) {
    perror("socketpair");
    return 1;
  }

  // 2. 创建子进程
  const pid_t child_pid = ::fork();

  if (child_pid == -1) {
    perror("fork");
    ::close(sv[0]);
    ::close(sv[1]);
    return 1;
  }

  if (child_pid == 0) {
    // Child:
    // 3. 关闭子进程不使用的 Socket 端点
    ::close(sv[0]);

    // 4. 接收父进程发送的 ping
    char request[4]{};

    if (!read_exact(sv[1], request, sizeof(request))) {
      ::close(sv[1]);
      ::_exit(1);
    }

    std::cout << "[Child] Received: ";
    std::cout.write(request, sizeof(request));
    std::cout << '\n';
    std::cout.flush();

    // 5. 向父进程发送 pong
    constexpr char response[] = "pong";

    const bool success = write_all(sv[1], response, sizeof(response) - 1);

    ::close(sv[1]);

    ::_exit(success ? 0 : 1);
  }

  // Parent:
  // 6. 关闭父进程不使用的 Socket 端点
  ::close(sv[1]);

  // 7. 向子进程发送 ping
  constexpr char message[] = "ping";

  const bool sent = write_all(sv[0], message, sizeof(message) - 1);

  // 8. 接收子进程返回的 pong
  char response[4]{};

  const bool received = sent && read_exact(sv[0], response, sizeof(response));

  ::close(sv[0]);

  // 9. 等待并回收子进程
  int status = 0;

  pid_t result;

  do {
    result = ::waitpid(child_pid, &status, 0);
  } while (result == -1 && errno == EINTR);

  if (result == -1) {
    perror("waitpid");
    return 1;
  }

  if (!sent || !received || !WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    std::cerr << "FAIL: socketpair communication\n";
    return 1;
  }

  std::cout << "[Parent] Received: ";
  std::cout.write(response, sizeof(response));
  std::cout << '\n';

  std::cout << "PASS: socketpair communication\n";

  return 0;
}
