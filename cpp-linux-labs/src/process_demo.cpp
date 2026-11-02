#include <cerrno>
#include <charconv>
#include <cstring>
#include <iostream>
#include <string_view>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

bool parse_hold_seconds(int argc, char* argv[], unsigned int* hold_seconds) {
    if (argc == 1) {
        *hold_seconds = 0;
        return true;
    }
    if (argc != 3 || std::string_view(argv[1]) != "--hold-seconds") {
        return false;
    }

    unsigned int parsed = 0;
    const std::string_view text(argv[2]);
    const auto result =
        std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc() || result.ptr != text.data() + text.size() ||
        parsed > 30) {
        return false;
    }

    *hold_seconds = parsed;
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    unsigned int hold_seconds = 0;
    if (!parse_hold_seconds(argc, argv, &hold_seconds)) {
        std::cerr << "usage: process_demo [--hold-seconds 0..30]\n";
        return 2;
    }

    std::cout << "Before fork: PID = " << ::getpid() << std::endl;

    const pid_t child_pid = ::fork();

    if (child_pid == -1) {
        const int error_number = errno;
        std::cerr << "fork failed: " << std::strerror(error_number) << '\n';
        return 1;
    }

    if (child_pid == 0) {
        std::cout << "[Child] PID = " << ::getpid()
                  << ", PPID = " << ::getppid() << std::endl;

        unsigned int remaining = hold_seconds;
        while (remaining > 0) {
            remaining = ::sleep(remaining);
        }
        return 7;
    }

    std::cout << "[Parent] PID = " << ::getpid()
              << ", child PID = " << child_pid << '\n';

    int status = 0;
    pid_t waited_pid = -1;
    do {
        waited_pid = ::waitpid(child_pid, &status, 0);
    } while (waited_pid == -1 && errno == EINTR);

    if (waited_pid == -1) {
        const int error_number = errno;
        std::cerr << "waitpid failed: " << std::strerror(error_number) << '\n';
        return 1;
    }

    if (!WIFEXITED(status)) {
        std::cerr << "FAIL: child did not exit normally\n";
        return 1;
    }

    const int exit_code = WEXITSTATUS(status);
    std::cout << "[Parent] Child " << waited_pid
              << " exited with code " << exit_code << '\n';

    if (exit_code != 7) {
        std::cerr << "FAIL: expected child exit code 7\n";
        return 1;
    }

    std::cout << "PASS: fork and waitpid experiment\n";
    return 0;
}
