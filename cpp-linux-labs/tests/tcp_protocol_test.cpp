#include "tcp_protocol.h"

#include <arpa/inet.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

namespace {

class SocketPair {
public:
  SocketPair() {
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds_) == -1) {
      perror("socketpair");
    }
  }

  ~SocketPair() {
    close_first();
    close_second();
  }

  SocketPair(const SocketPair &) = delete;
  SocketPair &operator=(const SocketPair &) = delete;

  bool valid() const { return fds_[0] != -1 && fds_[1] != -1; }
  int first() const { return fds_[0]; }
  int second() const { return fds_[1]; }

  void close_first() {
    if (fds_[0] != -1) {
      ::close(fds_[0]);
      fds_[0] = -1;
    }
  }

  void close_second() {
    if (fds_[1] != -1) {
      ::close(fds_[1]);
      fds_[1] = -1;
    }
  }

private:
  int fds_[2]{-1, -1};
};

bool check(bool condition, const char *name) {
  if (!condition) {
    std::cerr << "FAIL: " << name << '\n';
  }
  return condition;
}

std::string encode_frame(const std::string &payload) {
  const std::uint32_t network_length =
      htonl(static_cast<std::uint32_t>(payload.size()));

  std::string encoded(sizeof(network_length), '\0');
  std::memcpy(encoded.data(), &network_length, sizeof(network_length));
  encoded.append(payload);
  return encoded;
}

bool wait_until_socket_buffer_is_empty(int fd) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(1);

  while (std::chrono::steady_clock::now() < deadline) {
    int pending_bytes = -1;
    if (::ioctl(fd, FIONREAD, &pending_bytes) == -1) {
      perror("ioctl");
      return false;
    }

    if (pending_bytes == 0) {
      return true;
    }

    std::this_thread::yield();
  }

  return false;
}

bool test_normal_frame() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  const std::string expected = "hello, frame";
  std::string actual;

  const bool sent = tcp_protocol::send_frame(sockets.first(), expected);
  const auto received = tcp_protocol::receive_frame(sockets.second(), actual);

  return check(sent &&
                   received.status == tcp_protocol::ReceiveFrameStatus::ok &&
                   actual == expected,
               "normal frame");
}

bool test_empty_frame() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  std::string actual = "old value";
  const bool sent = tcp_protocol::send_frame(sockets.first(), "");
  const auto received = tcp_protocol::receive_frame(sockets.second(), actual);

  return check(sent &&
                   received.status == tcp_protocol::ReceiveFrameStatus::ok &&
                   actual.empty(),
               "empty frame");
}

bool test_combined_frame_stream() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  const std::vector<std::string> expected{"first", "", "third frame"};
  std::string combined;

  for (const auto &payload : expected) {
    combined.append(encode_frame(payload));
  }

  // 主动把三个完整 Frame 拼成一个连续缓冲区，再交给一次 send_all。
  const bool sent =
      tcp_protocol::send_all(sockets.first(), combined.data(), combined.size());

  bool received_all = sent;
  for (const auto &expected_payload : expected) {
    std::string actual;
    const auto result = tcp_protocol::receive_frame(sockets.second(), actual);

    received_all = result.status == tcp_protocol::ReceiveFrameStatus::ok &&
                   actual == expected_payload && received_all;
  }

  return check(received_all, "combined frame stream");
}

bool test_fragmented_frame() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  const std::string expected = "fragmented payload";
  const std::uint32_t network_length =
      htonl(static_cast<std::uint32_t>(expected.size()));
  const auto *header = reinterpret_cast<const char *>(&network_length);

  std::string actual;
  tcp_protocol::ReceiveFrameResult received{
      tcp_protocol::ReceiveFrameStatus::system_error, 0};
  std::atomic<bool> receiver_started{false};

  std::thread receiver([&]() {
    receiver_started.store(true, std::memory_order_release);
    received = tcp_protocol::receive_frame(sockets.second(), actual);
  });

  while (!receiver_started.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }

  const auto send_fragment = [&](const void *data, std::size_t size) {
    return tcp_protocol::send_all(sockets.first(), data, size) &&
           wait_until_socket_buffer_is_empty(sockets.second());
  };

  // 每次写入后等待接收端取走当前片段，确保 read_exact() 需要循环读取。
  bool sent = true;
  sent = send_fragment(header, 1) && sent;
  sent = send_fragment(header + 1, 1) && sent;
  sent = send_fragment(header + 2, sizeof(network_length) - 2) && sent;
  sent = send_fragment(expected.data(), 2) && sent;
  sent = send_fragment(expected.data() + 2, 1) && sent;
  sent = send_fragment(expected.data() + 3, expected.size() - 3) && sent;

  // 即使前面的测试逻辑失败，也用 EOF 保证接收线程能够退出。
  const bool shutdown_ok = ::shutdown(sockets.first(), SHUT_WR) == 0;
  receiver.join();

  return check(sent && shutdown_ok &&
                   received.status == tcp_protocol::ReceiveFrameStatus::ok &&
                   actual == expected,
               "fragmented frame");
}

bool test_maximum_size_frame() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  const std::string expected(tcp_protocol::MAX_MESSAGE_SIZE, 'm');
  bool sent = false;

  // 1 MiB 大于常见 Socket 发送缓冲区，需要发送与接收并行进行。
  std::thread sender(
      [&]() { sent = tcp_protocol::send_frame(sockets.first(), expected); });

  std::string actual;
  const auto received = tcp_protocol::receive_frame(sockets.second(), actual);
  sender.join();

  return check(sent &&
                   received.status == tcp_protocol::ReceiveFrameStatus::ok &&
                   actual == expected,
               "maximum size frame");
}

bool test_sender_rejects_oversized_frame() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  const std::string oversized(tcp_protocol::MAX_MESSAGE_SIZE + 1, 'x');
  const bool sent = tcp_protocol::send_frame(sockets.first(), oversized);

  // 超限应在写入长度头之前拒绝；关闭发送端后，接收端应看到干净 EOF。
  sockets.close_first();
  std::string payload;
  const auto received = tcp_protocol::receive_frame(sockets.second(), payload);

  return check(!sent &&
                   received.status ==
                       tcp_protocol::ReceiveFrameStatus::peer_closed &&
                   payload.empty(),
               "sender rejects oversized frame");
}

bool test_clean_peer_close() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  sockets.close_first();

  std::string payload = "old value";
  const auto received = tcp_protocol::receive_frame(sockets.second(), payload);

  return check(received.status ==
                       tcp_protocol::ReceiveFrameStatus::peer_closed &&
                   payload.empty(),
               "clean peer close");
}

bool test_truncated_header() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  const std::uint32_t network_length = htonl(5);
  const bool sent = tcp_protocol::send_all(sockets.first(), &network_length, 2);
  const bool shutdown_ok = ::shutdown(sockets.first(), SHUT_WR) == 0;

  std::string payload;
  const auto received = tcp_protocol::receive_frame(sockets.second(), payload);

  return check(sent && shutdown_ok &&
                   received.status ==
                       tcp_protocol::ReceiveFrameStatus::truncated_frame &&
                   payload.empty(),
               "truncated header");
}

bool test_truncated_payload() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  const std::uint32_t network_length = htonl(5);
  const std::string partial_payload = "ab";
  const bool sent =
      tcp_protocol::send_all(sockets.first(), &network_length,
                             sizeof(network_length)) &&
      tcp_protocol::send_all(sockets.first(), partial_payload.data(),
                             partial_payload.size());
  const bool shutdown_ok = ::shutdown(sockets.first(), SHUT_WR) == 0;

  std::string payload;
  const auto received = tcp_protocol::receive_frame(sockets.second(), payload);

  return check(sent && shutdown_ok &&
                   received.status ==
                       tcp_protocol::ReceiveFrameStatus::truncated_frame &&
                   payload.empty(),
               "truncated payload");
}

bool test_oversized_length() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  const std::uint32_t network_length =
      htonl(tcp_protocol::MAX_MESSAGE_SIZE + 1);
  const bool sent = tcp_protocol::send_all(sockets.first(), &network_length,
                                           sizeof(network_length));

  std::string payload;
  const auto received = tcp_protocol::receive_frame(sockets.second(), payload);

  return check(sent &&
                   received.status ==
                       tcp_protocol::ReceiveFrameStatus::message_too_large &&
                   payload.empty(),
               "oversized length");
}

bool test_closed_peer_does_not_raise_sigpipe() {
  SocketPair sockets;
  if (!sockets.valid()) {
    return false;
  }

  sockets.close_second();
  const bool sent = tcp_protocol::send_frame(sockets.first(), "data");

  return check(!sent, "closed peer does not raise SIGPIPE");
}

} // namespace

int main() {
  bool passed = true;
  passed = test_normal_frame() && passed;
  passed = test_empty_frame() && passed;
  passed = test_combined_frame_stream() && passed;
  passed = test_fragmented_frame() && passed;
  passed = test_maximum_size_frame() && passed;
  passed = test_sender_rejects_oversized_frame() && passed;
  passed = test_clean_peer_close() && passed;
  passed = test_truncated_header() && passed;
  passed = test_truncated_payload() && passed;
  passed = test_oversized_length() && passed;
  passed = test_closed_peer_does_not_raise_sigpipe() && passed;

  if (!passed) {
    return 1;
  }

  std::cout << "PASS: tcp protocol framing\n";
  return 0;
}
