#include <cerrno>
#include <cstring>
#include <iostream>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main() {
    std::cout << "Before fork: PID = " << ::getpid() << std::endl;

    const pid_t child_pid = ::fork();

    if (child_pid == -1) {
        const int error_number = errno;
        std::cerr << "fork failed: "
                  << std::strerror(error_number) << '\n';
        return 1;
    }

    if (child_pid == 0) {
        std::cout << "[Child] PID = " << ::getpid()
                  << ", PPID = " << ::getppid() << '\n';

        return 7;
    }

    std::cout << "[Parent] PID = " << ::getpid()
              << ", child PID = " << child_pid << '\n';

    int status = 0;
    const pid_t waited_pid = ::waitpid(child_pid, &status, 0);

    if (waited_pid == -1) {
        const int error_number = errno;
        std::cerr << "waitpid failed: "
                  << std::strerror(error_number) << '\n';
        return 1;
    }

    if (WIFEXITED(status)) {
        std::cout << "[Parent] Child " << waited_pid
                  << " exited with code "
                  << WEXITSTATUS(status) << '\n';
    } else {
        std::cout << "[Parent] Child did not exit normally\n";
    }

    return 0;
}
