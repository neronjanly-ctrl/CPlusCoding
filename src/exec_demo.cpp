#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    const pid_t child_pid = ::fork();

    if (child_pid == -1) {
        const int error_number = errno;
        std::cerr << "fork failed: "
                  << std::strerror(error_number) << '\n';
        return 1;
    }

    if (child_pid == 0) {
        std::cout << "[Child] Before exec, PID = "
                  << ::getpid() << '\n';

        ::execl("/bin/echo", "echo",
                "Hello from exec", static_cast<char*>(nullptr));

        // 只有 exec 失败才会执行到这里。
        const int error_number = errno;
        std::cerr << "exec failed: "
                  << std::strerror(error_number) << '\n';
        ::_exit(127);
    }

    int status = 0;
    if (::waitpid(child_pid, &status, 0) == -1) {
        const int error_number = errno;
        std::cerr << "waitpid failed: "
                  << std::strerror(error_number) << '\n';
        return 1;
    }

    if (WIFEXITED(status)) {
        std::cout << "[Parent] Child exit code = "
                  << WEXITSTATUS(status) << '\n';
    }

    return 0;
}
