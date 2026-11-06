#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <thread>
#include <unistd.h>

namespace {

constexpr char kServerIp[] = "127.0.0.1";
constexpr std::uint16_t kServerPort = 8080;

// 保证将指定长度的数据全部写入
bool write_all(int fd, const char *data, std::size_t length) {
  std::size_t total = 0;

  while (total < length) {
    const ssize_t n = ::write(fd, data + total, length - total);

    if (n > 0) {
      total += static_cast<std::size_t>(n);
    } else if (n == -1 && errno == EINTR) {
      continue;
    } else {
      return false;
    }
  }

  return true;
}

// 持续读取，直到对端关闭发送方向
bool read_until_eof(int fd, std::string &output) {
  char buffer[1024];

  while (true) {
    const ssize_t n = ::read(fd, buffer, sizeof(buffer));

    if (n > 0) {
      output.append(buffer, static_cast<std::size_t>(n));
    } else if (n == 0) {
      return true;
    } else if (errno == EINTR) {
      continue;
    } else {
      return false;
    }
  }
}

void handle_client(int client_fd) {

  // 1. 接收当前客户端的数据
  std::string request;

  if (!read_until_eof(client_fd, request)) {
    perror("read");
    ::close(client_fd);
    return;
  }

  std::cout << "[Worker] Received: " << request;

  // 2. 返回当前客户端的数据
  if (!write_all(client_fd, request.data(), request.size())) {
    perror("write");
    ::close(client_fd);
    return;
  }

  // 3. 关闭当前连接的发送方向
  ::shutdown(client_fd, SHUT_WR);

  // 4. 释放当前客户端 FD
  ::close(client_fd);

  std::cout << "[Worker] Client finished\n";
}
} // namespace

int main() {
  // 1. 创建 IPv4 TCP 监听 Socket
  const int listen_fd = ::socket(AF_INET, SOCK_STREAM, 0);

  if (listen_fd == -1) {
    perror("socket");
    return 1;
  }

  // 2. 设置地址复用
  int reuse = 1;

  if (::setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse,
                   sizeof(reuse)) == -1) {
    perror("setsockopt");
    ::close(listen_fd);
    return 1;
  }

  // 3. 配置服务端地址
  sockaddr_in server_addr{};

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(kServerPort);

  if (::inet_pton(AF_INET, kServerIp, &server_addr.sin_addr) != 1) {
    std::cerr << "Invalid server IP address\n";
    ::close(listen_fd);
    return 1;
  }

  // 4. 绑定地址
  if (::bind(listen_fd, reinterpret_cast<const sockaddr *>(&server_addr),
             sizeof(server_addr)) == -1) {
    perror("bind");
    ::close(listen_fd);
    return 1;
  }

  // 5. 开始监听
  if (::listen(listen_fd, 5) == -1) {
    perror("listen");
    ::close(listen_fd);
    return 1;
  }

  std::cout << "[Server] Listening on " << kServerIp << ":" << kServerPort
            << std::endl;

  // 6. 持续接受客户端连接
  while (true) {

    int client_fd;

    do {
      client_fd = ::accept(listen_fd, nullptr, nullptr);
    } while (client_fd == -1 && errno == EINTR);

    if (client_fd == -1) {
      perror("accept");
      continue;
    }

    std::cout << "[Server] Client connected\n";

    // 7. 为当前客户端创建工作线程
    try {
      std::thread worker([client_fd]() { handle_client(client_fd); });

      // 8. 分离工作线程
      worker.detach();

    } catch (const std::system_error &e) {
      std::cerr << "Thread creation failed: " << e.what() << '\n';

      ::close(client_fd);
    }
  }
}
