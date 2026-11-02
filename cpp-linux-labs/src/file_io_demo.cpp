#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <string_view>
#include <unistd.h>

int main() {
  constexpr char kPath[] = "/tmp/cpp_linux_labs_file_io.txt";

  const int fd = ::open(kPath, O_CREAT | O_TRUNC | O_RDWR, 0644);
  if (fd == -1) {
    const int error_number = errno;
    std::cerr << "open failed: errno=" << error_number << " ("
              << std::strerror(error_number) << ")\n";
    return 1;
  }

  std::cout << "open succeeded: fd=" << fd << '\n';

  constexpr std::string_view kMessage = "Hello from Linux write()\n";

  const ssize_t bytes_written = ::write(fd, kMessage.data(), kMessage.size());

  if (bytes_written == -1) {
    const int error_number = errno;
    std::cerr << "write failed: errno=" << error_number << " ("
              << std::strerror(error_number) << ")\n";

    ::close(fd);
    return 1;
  }

  std::cout << "write returned: " << bytes_written << '\n';

  // This small regular-file experiment requires the entire message.
  // A short write must not silently count as success.
  if (static_cast<std::size_t>(bytes_written) != kMessage.size()) {
    std::cerr << "unexpected short write; experiment incomplete\n";
    ::close(fd);
    return 1;
  }

  const off_t after_write = ::lseek(fd, 0, SEEK_CUR);
  if (after_write == -1) {
    const int error_number = errno;
    std::cerr << "query offset failed: " << std::strerror(error_number) << '\n';
    ::close(fd);
    return 1;
  }
  std::cout << "offset after write: " << after_write << '\n';

  char buffer[128];
  const ssize_t first_read = ::read(fd, buffer, sizeof(buffer));
  if (first_read == -1) {
    const int error_number = errno;
    std::cerr << "first read failed: " << std::strerror(error_number) << '\n';
    ::close(fd);
    return 1;
  }
  std::cout << "read before rewind: " << first_read << '\n';

  const off_t rewound = ::lseek(fd, 0, SEEK_SET);
  if (rewound == -1) {
    const int error_number = errno;
    std::cerr << "rewind failed: " << std::strerror(error_number) << '\n';
    ::close(fd);
    return 1;
  }
  std::cout << "offset after rewind: " << rewound << '\n';

  const ssize_t second_read = ::read(fd, buffer, sizeof(buffer));
  if (second_read == -1) {
    const int error_number = errno;
    std::cerr << "second read failed: " << std::strerror(error_number) << '\n';
    ::close(fd);
    return 1;
  }
  std::cout << "read after rewind: " << second_read << '\n';
  std::cout << "content: ";
  // read() does not append a null terminator. Print only the bytes read.
  std::cout.write(buffer, second_read);

  const off_t after_read = ::lseek(fd, 0, SEEK_CUR);
  if (after_read == -1) {
    const int error_number = errno;
    std::cerr << "query offset failed: " << std::strerror(error_number) << '\n';
    ::close(fd);
    return 1;
  }
  std::cout << "offset after read: " << after_read << '\n';

  const bool offset_ok =
      after_write == static_cast<off_t>(kMessage.size()) && first_read == 0 &&
      rewound == 0 && after_read == static_cast<off_t>(kMessage.size()) &&
      std::string_view(buffer, static_cast<std::size_t>(second_read)) ==
          kMessage;

  if (::close(fd) == -1) {
    const int error_number = errno;
    std::cerr << "close failed: errno=" << error_number << " ("
              << std::strerror(error_number) << ")\n";
    return 1;
  }

  std::cout << "close succeeded\n";

  if (!offset_ok) {
    std::cerr << "FAIL: offset or content did not match expectations\n";
    return 1;
  }
  std::cout << "PASS: file offset experiment\n";

  // Verified absent before the experiment. Do not pass O_CREAT here.
  constexpr char kMissingPath[] = "/tmp/cpp_linux_labs_missing_for_errno.txt";
  const int missing_fd = ::open(kMissingPath, O_RDONLY);
  if (missing_fd != -1) {
    std::cerr << "FAIL: missing-file fixture exists; choose an absent path\n";
    if (::close(missing_fd) == -1) {
      const int error_number = errno;
      std::cerr << "close unexpected file failed: "
                << std::strerror(error_number) << '\n';
    }
    return 1;
  }

  // Save errno before logging or calling another library function.
  const int error_number = errno;
  std::cout << "open missing file: return=" << missing_fd
            << ", errno=" << error_number << " (" << std::strerror(error_number)
            << ")\n";
  if (error_number != ENOENT) {
    std::cerr << "FAIL: expected ENOENT\n";
    return 1;
  }
  std::cout << "PASS: errno experiment (ENOENT)\n";
  return 0;
}
