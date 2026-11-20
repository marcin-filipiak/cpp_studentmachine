#include "core/proc.hpp"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

#include "core/logger.hpp"
#include "core/strings.hpp"
#include "core/terminal.hpp"
#include "core/types.hpp"

namespace sm::proc {
namespace {

bool gVerbose = false;

bool drain(int descriptor, std::string& output, bool echo) {
    char buffer[8192];
    for (;;) {
        const ssize_t count = ::read(descriptor, buffer, sizeof(buffer));
        if (count > 0) {
            output.append(buffer, static_cast<std::size_t>(count));
            if (echo) {
                std::cout.write(buffer, count);
                std::cout.flush();
            }
            continue;
        }
        if (count == 0) {
            return true;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return false;
        }
        return true;
    }
}

std::string executablePath(const std::string& command) {
    if (command.empty()) {
        return {};
    }
    if (command.find('/') != std::string::npos) {
        return access(command.c_str(), X_OK) == 0 ? command : std::string();
    }
    const char* pathValue = std::getenv("PATH");
    if (pathValue == nullptr) {
        return {};
    }
    const std::vector<std::string> directories = [&]() {
        std::vector<std::string> parts;
        std::istringstream stream(pathValue);
        std::string part;
        while (std::getline(stream, part, ':')) {
            if (!part.empty()) {
                parts.push_back(part);
            }
        }
        return parts;
    }();
    for (const std::string& directory : directories) {
        const fs::path candidate = fs::path(directory) / command;
        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate.string();
        }
    }

    // Some distributions keep administrative tools outside a regular user's PATH.
    const char* const fallbackDirectories[] = {"/usr/local/sbin", "/usr/local/bin", "/usr/sbin",
                                               "/sbin", "/usr/bin", "/bin"};
    for (const char* directory : fallbackDirectories) {
        const fs::path candidate = fs::path(directory) / command;
        if (access(candidate.c_str(), X_OK) == 0) {
            return candidate.string();
        }
    }
    return {};
}

}  // namespace

RunResult run(const std::vector<std::string>& argv, const RunOptions& options) {
    RunResult result;
    if (argv.empty()) {
        return result;
    }

    int pipeFds[2] = {-1, -1};
    if (options.capture) {
        if (::pipe(pipeFds) != 0) {
            result.output = "Cannot create a pipe: ";
            result.output += std::strerror(errno);
            return result;
        }
    }

    std::string executable = argv.front();
    if (executable.find('/') == std::string::npos) {
        const std::string resolved = executablePath(executable);
        if (!resolved.empty()) {
            executable = resolved;
        }
    }

    std::vector<char*> arguments;
    arguments.reserve(argv.size() + 1);
    arguments.push_back(const_cast<char*>(executable.c_str()));
    for (std::size_t i = 1; i < argv.size(); ++i) {
        arguments.push_back(const_cast<char*>(argv[i].c_str()));
    }
    arguments.push_back(nullptr);

    const pid_t child = ::fork();
    if (child < 0) {
        result.output = "Cannot start a process: ";
        result.output += std::strerror(errno);
        if (pipeFds[0] >= 0) {
            ::close(pipeFds[0]);
            ::close(pipeFds[1]);
        }
        return result;
    }

    if (child == 0) {
        if (!options.workingDirectory.empty() &&
            ::chdir(options.workingDirectory.c_str()) != 0) {
            ::dprintf(STDERR_FILENO, "studentmachine: cannot enter %s: %s\n",
                      options.workingDirectory.c_str(), std::strerror(errno));
            ::_exit(126);
        }

        for (const std::string& entry : options.environment) {
            const std::size_t separator = entry.find('=');
            if (separator == std::string::npos) {
                continue;
            }
            ::setenv(entry.substr(0, separator).c_str(), entry.substr(separator + 1).c_str(), 1);
        }

        if (!options.interactive) {
            const int nullDevice = ::open("/dev/null", O_RDONLY);
            if (nullDevice >= 0) {
                ::dup2(nullDevice, STDIN_FILENO);
                if (nullDevice != STDIN_FILENO) {
                    ::close(nullDevice);
                }
            }
        }

        if (options.capture) {
            ::dup2(pipeFds[1], STDOUT_FILENO);
            ::dup2(pipeFds[1], STDERR_FILENO);
            ::close(pipeFds[0]);
            ::close(pipeFds[1]);
        }

        ::execvp(arguments[0], arguments.data());
        ::dprintf(STDERR_FILENO, "studentmachine: cannot run %s: %s\n", arguments[0],
                  std::strerror(errno));
        ::_exit(127);
    }

    int readFd = -1;
    if (options.capture) {
        ::close(pipeFds[1]);
        readFd = pipeFds[0];
        const int flags = ::fcntl(readFd, F_GETFL, 0);
        if (flags >= 0) {
            ::fcntl(readFd, F_SETFL, flags | O_NONBLOCK);
        }
    }

    const auto started = std::chrono::steady_clock::now();
    auto lastTick = started;
    auto terminationTime = started;
    bool exited = false;
    bool eof = !options.capture;
    bool terminated = false;
    bool forceKilled = false;
    int status = 0;

    while (!exited || !eof) {
        if (options.capture && !eof) {
            struct pollfd descriptor {
                readFd, POLLIN, 0
            };
            const int ready = ::poll(&descriptor, 1, 50);
            if (ready > 0) {
                if (drain(readFd, result.output, options.echo)) {
                    eof = true;
                }
            } else if (ready < 0 && errno != EINTR) {
                eof = true;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        if (!exited) {
            const pid_t waitResult = ::waitpid(child, &status, WNOHANG);
            if (waitResult == child) {
                exited = true;
            } else if (waitResult < 0 && errno != EINTR) {
                exited = true;
            }
        }

        const auto now = std::chrono::steady_clock::now();
        if (!exited && options.timeoutSeconds > 0) {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::seconds>(now - started).count();
            if (!terminated && elapsed >= options.timeoutSeconds) {
                ::kill(child, SIGTERM);
                terminated = true;
                terminationTime = now;
                result.timedOut = true;
            } else if (terminated && !forceKilled &&
                       std::chrono::duration_cast<std::chrono::seconds>(now - terminationTime)
                               .count() >= 2) {
                ::kill(child, SIGKILL);
                forceKilled = true;
            }
        }

        if (options.onTick != nullptr &&
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastTick).count() >=
                options.tickMilliseconds) {
            options.onTick();
            lastTick = now;
        }
    }

    if (!exited) {
        ::waitpid(child, &status, 0);
    }

    if (readFd >= 0) {
        while (!drain(readFd, result.output, options.echo)) {
        }
        ::close(readFd);
    }

    if (WIFEXITED(status)) {
        result.exitCode = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result.exitCode = 128 + WTERMSIG(status);
    } else {
        result.exitCode = -1;
    }

    logger::command(argv, result.exitCode, result.output);

    if (gVerbose && !options.echo && !result.output.empty()) {
        std::cout << "    " << term::dim("$ " + joinForDisplay(argv)) << "\n";
        std::cout << result.output;
        if (result.output.back() != '\n') {
            std::cout << "\n";
        }
        std::cout.flush();
    }
    return result;
}

bool exists(const std::string& command) {
    return !executablePath(command).empty();
}

std::string pathOf(const std::string& command) {
    return executablePath(command);
}

void setVerbose(bool value) {
    gVerbose = value;
}

bool isRoot() {
    return ::geteuid() == 0;
}

std::vector<std::string> elevate(const std::vector<std::string>& argv) {
    if (isRoot()) {
        return argv;
    }
    std::vector<std::string> elevated;
    elevated.reserve(argv.size() + 1);
    elevated.push_back("sudo");
    elevated.insert(elevated.end(), argv.begin(), argv.end());
    return elevated;
}

std::string joinForDisplay(const std::vector<std::string>& argv) {
    std::vector<std::string> quoted;
    quoted.reserve(argv.size());
    for (const std::string& argument : argv) {
        quoted.push_back(str::shellQuote(argument));
    }
    return str::join(quoted, " ");
}

}  // namespace sm::proc
