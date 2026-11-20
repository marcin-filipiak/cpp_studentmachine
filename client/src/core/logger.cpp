#include "core/logger.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "core/fsutil.hpp"
#include "core/strings.hpp"
#include "version.hpp"

namespace sm::logger {
namespace {

std::ofstream gStream;
fs::path gPath;
std::string gCommand;

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local {};
    localtime_r(&seconds, &local);
    std::ostringstream text;
    text << std::put_time(&local, "%Y-%m-%d %H:%M:%S");
    return text.str();
}

std::string fileTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
    std::tm local {};
    localtime_r(&seconds, &local);
    std::ostringstream text;
    text << std::put_time(&local, "%Y%m%d-%H%M%S");
    return text.str();
}

std::string slug(const std::string& value) {
    std::string result;
    for (char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
            c == '_') {
            result += c;
        } else {
            result += '_';
        }
    }
    return result.empty() ? "command" : result;
}

}  // namespace

void start(const std::string& command) {
    stop();
    gCommand = command;
    const fs::path directory = fsutil::cacheDir() / "logs";
    std::string error;
    fsutil::ensureDir(directory, error);
    gPath = directory / (fileTimestamp() + "-" + slug(command) + ".log");
    gStream.open(gPath, std::ios::out | std::ios::trunc);
    if (!gStream) {
        gPath.clear();
        return;
    }
    gStream << kAppName << " " << kVersion << " (" << kCodename << ")\n";
    gStream << "Command: " << command << "\n";
    gStream << "Started: " << timestamp() << "\n";
    gStream << "----------------------------------------\n";
    gStream.flush();
}

void stop() {
    if (gStream.is_open()) {
        gStream << "----------------------------------------\n";
        gStream << "Finished: " << timestamp() << "\n";
        gStream.flush();
        gStream.close();
    }
    gPath.clear();
}

bool active() {
    return gStream.is_open();
}

const fs::path& path() {
    return gPath;
}

void info(const std::string& message) {
    if (!gStream.is_open()) {
        return;
    }
    gStream << "[" << timestamp() << "] " << message << "\n";
    gStream.flush();
}

void command(const std::vector<std::string>& argv, int exitCode, const std::string& output) {
    if (!gStream.is_open()) {
        return;
    }

    std::string display;
    for (const std::string& argument : argv) {
        if (!display.empty()) {
            display += " ";
        }
        display += str::shellQuote(argument);
    }

    gStream << "\n[" << timestamp() << "] $ " << display << "\n";
    gStream << "exit: " << exitCode << "\n";

    if (!output.empty()) {
        constexpr std::size_t kMaxOutput = 512 * 1024;
        gStream << "--- output ---\n";
        if (output.size() > kMaxOutput) {
            gStream << output.substr(0, kMaxOutput) << "\n[output truncated]\n";
        } else {
            gStream << output;
            if (output.back() != '\n') {
                gStream << "\n";
            }
        }
        gStream << "--------------\n";
    }
    gStream.flush();
}

}  // namespace sm::logger
