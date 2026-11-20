#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/types.hpp"

namespace sm::net {

struct Probe {
    bool ok = false;
    int status = 0;
    std::uint64_t size = 0;
    std::string error;
};

struct DownloadResult {
    bool ok = false;
    int curlExit = 0;
    int httpStatus = 0;
    std::uint64_t bytes = 0;
    std::string error;
    std::string hint;
};

struct UploadResult {
    bool ok = false;
    std::string response;
    std::string error;
    std::string hint;
};

Probe probe(const std::string& url);
DownloadResult download(const std::string& url, const fs::path& destination,
                        const std::string& label, bool showProgress = true);
UploadResult uploadFile(const std::string& url, const fs::path& file, const std::string& field,
                        const std::string& label);
bool endpointReachable(const std::string& url, std::string& error);
std::string describeCurlFailure(int code, const std::string& output);
std::string hintForFailure(int code, const std::string& output);

}  // namespace sm::net
