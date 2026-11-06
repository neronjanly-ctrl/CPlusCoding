
#pragma once

#include <arpa/inet.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace tcp_protocol {

constexpr std::uint32_t MAX_MESSAGE_SIZE = 1024 * 1024;

// ==================== read_exact ====================

enum class ReadExactStatus {
  complete,
  eof_before_data,
  eof_mid_read,
  system_error,
};

struct ReadExactResult {
  ReadExactStatus status;
  int error_number;
};

// 完整读取指定长度的数据
inline ReadExactResult read_exact(int fd, void *buffer, std::size_t length) {
  std::size_t total = 0;
  auto *bytes = static_cast<char *>(buffer);

  while (total < length) {
    const ssize_t n = ::read(fd, bytes + total, length - total);

    if (n > 0) {
      total += static_cast<std::size_t>(n);

    } else if (n == 0) {
      return {total == 0 ? ReadExactStatus::eof_before_data
                         : ReadExactStatus::eof_mid_read,
              0};

    } else if (errno == EINTR) {
      continue;

    } else {
      return {ReadExactStatus::system_error, errno};
    }
  }

  return {ReadExactStatus::complete, 0};
}

// ==================== send_all ====================

// 完整发送指定长度的数据；MSG_NOSIGNAL 避免对端关闭时进程被 SIGPIPE 终止。
inline bool send_all(int fd, const void *buffer, std::size_t length) {
  std::size_t total = 0;

  const auto *bytes = static_cast<const char *>(buffer);

  while (total < length) {
    const ssize_t n = ::send(fd, bytes + total, length - total, MSG_NOSIGNAL);

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

// ==================== send_frame ====================

inline bool send_frame(int fd, const std::string &payload) {
  if (payload.size() > MAX_MESSAGE_SIZE) {
    return false;
  }

  const std::uint32_t length =
      htonl(static_cast<std::uint32_t>(payload.size()));

  // 先发送 4 字节长度头
  if (!send_all(fd, &length, sizeof(length))) {
    return false;
  }

  // 再发送 payload
  if (!payload.empty()) {
    if (!send_all(fd, payload.data(), payload.size())) {
      return false;
    }
  }

  return true;
}

// ==================== receive_frame ====================

enum class ReceiveFrameStatus {
  ok,
  peer_closed,
  truncated_frame,
  message_too_large,
  system_error,
};

struct ReceiveFrameResult {
  ReceiveFrameStatus status;
  int error_number;
};

inline ReceiveFrameResult receive_frame(int fd, std::string &payload) {
  payload.clear();

  // 读取 4 字节长度头
  std::uint32_t network_length = 0;

  const ReadExactResult header_result =
      read_exact(fd, &network_length, sizeof(network_length));

  switch (header_result.status) {
  case ReadExactStatus::complete:
    break;

  case ReadExactStatus::eof_before_data:
    return {ReceiveFrameStatus::peer_closed, 0};

  case ReadExactStatus::eof_mid_read:
    return {ReceiveFrameStatus::truncated_frame, 0};

  case ReadExactStatus::system_error:
    return {ReceiveFrameStatus::system_error, header_result.error_number};
  }

  // 转换为主机字节序
  const std::uint32_t length = ntohl(network_length);

  // 防止超大消息造成内存分配
  if (length > MAX_MESSAGE_SIZE) {
    return {ReceiveFrameStatus::message_too_large, 0};
  }

  // 空消息也是合法消息
  if (length == 0) {
    return {ReceiveFrameStatus::ok, 0};
  }

  // 读取 payload
  payload.resize(length);

  const ReadExactResult payload_result = read_exact(fd, payload.data(), length);

  switch (payload_result.status) {
  case ReadExactStatus::complete:
    return {ReceiveFrameStatus::ok, 0};

  case ReadExactStatus::eof_before_data:
  case ReadExactStatus::eof_mid_read:
    payload.clear();

    return {ReceiveFrameStatus::truncated_frame, 0};

  case ReadExactStatus::system_error:
    payload.clear();

    return {ReceiveFrameStatus::system_error, payload_result.error_number};
  }

  return {ReceiveFrameStatus::system_error, 0};
}

} // namespace tcp_protocol
