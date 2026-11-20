#pragma once

#include <functional>
#include <string>
#include <vector>

namespace sm::proc {

struct RunOptions {
    bool capture = true;
    bool echo = false;
    bool interactive = false;
    int timeoutSeconds = 0;
    std::string workingDirectory;
    std::vector<std::string> environment;
    std::function<void()> onTick;
    int tickMilliseconds = 100;
};

struct RunResult {
    int exitCode = -1;
    bool timedOut = false;
    std::string output;

    bool ok() const { return exitCode == 0 && !timedOut; }
};

RunResult run(const std::vector<std::string>& argv, const RunOptions& options = {});

bool exists(const std::string& command);
std::string pathOf(const std::string& command);
void setVerbose(bool value);
bool isRoot();
std::vector<std::string> elevate(const std::vector<std::string>& argv);
std::string joinForDisplay(const std::vector<std::string>& argv);

}  // namespace sm::proc
