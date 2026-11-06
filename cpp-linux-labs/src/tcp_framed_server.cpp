#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <system_error>
#include <thread>
#include <unistd.h>

#include "tcp_protocol.h"

namespace {

constexpr char kServerIp[] = "127.0.0.1";
constexpr std::uint16_t kServerPort = 8081;

void handle_client(int client_fd) {

  // 是否继续接收下一条消息
  bool continue_receiving = true;

  // 是否属于客户端正常关闭发送方向
  bool peer_closed_normally = false;

  while (continue_receiving) {

    // 1. 准备当前消息的接收空间
    std::string request;

    // 2. 接收一条完整的协议消息
    const auto result = tcp_protocol::receive_frame(client_fd, request);

    // 3. 根据接收状态决定后续行为
    switch (result.status) {

    case tcp_protocol::ReceiveFrameStatus::ok: {

      std::cout << "[Worker] Received: " << request << '\n';

      // 4. 将当前消息作为一条完整 Frame 返回
      if (!tcp_protocol::send_frame(client_fd, request)) {

        const int error_number = errno;

        std::cerr << "[Worker] send_frame failed: "
                  << std::strerror(error_number) << '\n';

        continue_receiving = false;
      }

      break;
    }

    case tcp_protocol::ReceiveFrameStatus::peer_closed:

      std::cout << "[Worker] Client finished sending\n";

      peer_closed_normally = true;
      continue_receiving = false;

      break;

    case tcp_protocol::ReceiveFrameStatus::truncated_frame:

      std::cerr << "[Worker] Protocol error: " << "truncated frame\n";

      continue_receiving = false;

      break;

    case tcp_protocol::ReceiveFrameStatus::message_too_large:

      std::cerr << "[Worker] Protocol error: " << "message too large\n";

      continue_receiving = false;

      break;

    case tcp_protocol::ReceiveFrameStatus::system_error:

      std::cerr << "[Worker] Receive failed: "
                << std::strerror(result.error_number) << '\n';

      continue_receiving = false;

      break;
    }
  }

  // 5. 客户端正常结束发送后，
  //    服务端也结束自己的发送方向
  if (peer_closed_normally) {

    if (::shutdown(client_fd, SHUT_WR) == -1) {
      perror("shutdown");
    }
  }

  // 6. 统一释放当前客户端的 Socket FD
  if (::close(client_fd) == -1) {
    perror("close");
  }

  std::cout << "[Worker] Client connection closed\n";
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
