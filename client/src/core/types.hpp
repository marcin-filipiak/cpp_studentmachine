#pragma once

#include <filesystem>
#include <string>

namespace sm {

namespace fs = std::filesystem;

// Exit codes are stable so shell scripts can react to failures.
enum ExitCode : int {
    ExitOk = 0,
    ExitFailure = 1,
    ExitUsage = 2,
    ExitDependency = 3,
    ExitNetwork = 4,
    ExitExternal = 5,
    ExitAborted = 6,
};

struct Context {
    bool assumeYes = false;
    bool verbose = false;
};

inline constexpr const char* kDefaultEmail = "student@student.machine";
inline constexpr const char* kApiBase = "https://api.filipiak.cc/StudentMachine";
inline constexpr const char* kKeyringWeb = "https://api.filipiak.cc/StudentMachine/keyring/";
inline constexpr const char* kBinaryUrl =
    "https://github.com/marcin-filipiak/cpp_studentmachine/raw/main/client/build/studentmachine";

}  // namespace sm
