#include "tcp_protocol.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

namespace {

constexpr char kServerIp[] = "127.0.0.1";
constexpr std::uint16_t kServerPort = 8081;

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
  std::vector<std::string> messages;

  messages.reserve(1000);

  for (std::size_t i = 0; i < 1000; ++i) {

    if (i == 0) {
      // 保留一条空消息测试
      messages.emplace_back();

    } else {

      // 生成具有不同长度的消息
      messages.push_back("message-" + std::to_string(i) + ":" +
                         std::string(i % 64, 'x'));
    }
  }

  std::size_t completed = 0;
  std::size_t total_bytes = 0;

  for (const auto &expected : messages) {

    // 1. 发送一条 Frame
    if (!tcp_protocol::send_frame(client_fd, expected)) {
      std::cerr << "[Client] Send failed\n";
      ::close(client_fd);
      return 1;
    }

    // 2. 接收一条 Echo Frame
    std::string response;

    const auto result = tcp_protocol::receive_frame(client_fd, response);

    // 3. 检查接收状态
    if (result.status != tcp_protocol::ReceiveFrameStatus::ok) {

      std::cerr << "[Client] Receive failed\n";
      ::close(client_fd);
      return 1;
    }

    // 4. 比较请求与响应
    if (response != expected) {
      std::cerr << "[Client] Echo mismatch\n";
      ::close(client_fd);
      return 1;
    }
    ++completed;

    total_bytes += response.size();

    if (completed % 100 == 0 || completed == messages.size()) {

      std::cout << "[Client] Verified " << completed << "/" << messages.size()
                << " frames\n";
    }
  }

  std::cout << "[Client] All frames verified: " << completed << '\n';

  std::cout << "[Client] Total payload bytes: " << total_bytes << '\n';

  // 5. 告诉服务端：客户端已经发送完毕
  if (::shutdown(client_fd, SHUT_WR) == -1) {
    perror("shutdown");
    ::close(client_fd);
    return 1;
  }

  // 6. 继续接收，等待服务端正常关闭发送方向
  std::string final_response;

  const auto close_result =
      tcp_protocol::receive_frame(client_fd, final_response);

  // 7. 验证服务端是否正常结束
  if (close_result.status != tcp_protocol::ReceiveFrameStatus::peer_closed) {

    std::cerr << "[Client] Expected peer_closed from server\n";

    ::close(client_fd);
    return 1;
  }

  std::cout << "[Client] Server closed its sending direction\n";

  // 8. 释放客户端 Socket
  if (::close(client_fd) == -1) {
    perror("close");
    return 1;
  }

  std::cout << "PASS: framed TCP client\n";

  return 0;
}
