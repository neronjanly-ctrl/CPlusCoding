#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace {

constexpr char kServerIp[] = "127.0.0.1";
constexpr std::uint16_t kServerPort = 8081;

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

} // namespace

int main() {
  // 1. 创建客户端 Socket
  const int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);

  if (client_fd == -1) {
    perror("socket");
    return 1;
  }

  // 2. 设置服务端地址
  sockaddr_in server_addr{};

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(kServerPort);

  if (::inet_pton(AF_INET, kServerIp, &server_addr.sin_addr) != 1) {
    std::cerr << "Invalid server IP address\n";
    ::close(client_fd);
    return 1;
  }

  // 3. 主动连接服务端
  if (::connect(client_fd, reinterpret_cast<const sockaddr *>(&server_addr),
                sizeof(server_addr)) == -1) {
    perror("connect");
    ::close(client_fd);
    return 1;
  }

  std::cout << "[Client] Connected to server\n";

  // 4. 发送消息
  const std::string message = "Hello from TCP client\n";

  if (!write_all(client_fd, message.data(), message.size())) {
    perror("write");
    ::close(client_fd);
    return 1;
  }

  std::cout << "[Client] Sent: " << message;

  // 5. 告诉服务端：客户端已经发送完毕
  ::shutdown(client_fd, SHUT_WR);

  // 6. 接收服务端回显的数据
  std::string response;

  if (!read_until_eof(client_fd, response)) {
    perror("read");
    ::close(client_fd);
    return 1;
  }

  std::cout << "[Client] Echo from server: " << response;

  // 7. 关闭客户端 Socket
  ::close(client_fd);

  return 0;
}
