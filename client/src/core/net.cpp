#include "core/net.hpp"

#include <cstdlib>
#include <memory>
#include <system_error>

#include "core/fsutil.hpp"
#include "core/proc.hpp"
#include "core/progress.hpp"
#include "core/strings.hpp"

namespace sm::net {
namespace {

std::vector<std::string> curlBase() {
    return {"curl",
            "--location",
            "--silent",
            "--show-error",
            "--connect-timeout",
            "20",
            "--retry",
            "3",
            "--retry-delay",
            "1",
            "--retry-connrefused",
            "--proto",
            "=https"};
}

std::uint64_t fileSize(const fs::path& file) {
    std::error_code ec;
    const auto size = fs::file_size(file, ec);
    return ec ? 0 : static_cast<std::uint64_t>(size);
}

int parseStatus(const std::string& output) {
    int status = 0;
    for (const std::string& line : str::splitLines(output)) {
        if (!str::startsWith(line, "HTTP/")) {
            continue;
        }
        const std::vector<std::string> parts = str::splitWords(line);
        if (parts.size() >= 2) {
            status = std::atoi(parts[1].c_str());
        }
    }
    return status;
}

int statusFromError(const std::string& output) {
    const int parsed = parseStatus(output);
    if (parsed != 0) {
        return parsed;
    }
    const std::size_t position = output.find("error: ");
    if (position != std::string::npos) {
        return std::atoi(output.c_str() + position + 7);
    }
    return 0;
}

bool failureStatus(int status) {
    return status == 403 || status == 404 || status == 410;
}

std::string failureText(int status) {
    switch (status) {
        case 403:
            return "The server denied access (HTTP 403).";
        case 404:
            return "The resource was not found on the server (HTTP 404).";
        case 410:
            return "The resource is no longer available (HTTP 410).";
        default:
            return "The server returned HTTP " + std::to_string(status) + ".";
    }
}

}  // namespace

Probe probe(const std::string& url) {
    Probe result;
    if (!proc::exists("curl")) {
        result.error = "curl is not installed.";
        return result;
    }

    std::vector<std::string> argv = {"curl", "--silent", "--show-error", "--head", "--location",
                                     "--max-time", "20", "--proto", "=https", url};
    const proc::RunResult response = proc::run(argv, {});
    if (!response.ok()) {
        result.error = describeCurlFailure(response.exitCode, response.output);
        return result;
    }

    int status = 0;
    std::uint64_t size = 0;
    for (const std::string& line : str::splitLines(response.output)) {
        if (str::startsWith(line, "HTTP/")) {
            const std::vector<std::string> parts = str::splitWords(line);
            if (parts.size() >= 2) {
                status = std::atoi(parts[1].c_str());
                size = 0;
            }
        } else if (str::startsWith(str::lower(line), "content-length:")) {
            const std::string value = str::trim(line.substr(line.find(':') + 1));
            size = static_cast<std::uint64_t>(std::strtoull(value.c_str(), nullptr, 10));
        }
    }

    result.ok = true;
    result.status = status;
    result.size = size;
    return result;
}

DownloadResult download(const std::string& url, const fs::path& destination,
                        const std::string& label, bool showProgress) {
    DownloadResult result;
    if (!proc::exists("curl")) {
        result.error = "curl is not installed.";
        result.hint = "Run `studentmachine install` to install the required tools.";
        return result;
    }

    std::string directoryError;
    if (fsutil::ensureDir(destination.parent_path(), directoryError).empty()) {
        result.error = directoryError;
        return result;
    }

    const Probe remote = probe(url);
    result.httpStatus = remote.status;
    if (remote.ok && (remote.status == 404 || remote.status == 410)) {
        result.error = failureText(remote.status);
        result.hint = "Check the name and try again.";
        return result;
    }

    const fs::path temporary = destination.string() + ".part";
    std::error_code ec;
    fs::remove(temporary, ec);

    std::vector<std::string> argv = curlBase();
    argv.insert(argv.end(), {"--fail", "--max-time", "900", "--output", temporary.string(), url});

    std::unique_ptr<progress::Bar> bar;
    std::unique_ptr<progress::Spinner> spinner;
    if (showProgress) {
        bar = std::make_unique<progress::Bar>(label, remote.size);
    } else {
        spinner = std::make_unique<progress::Spinner>(label);
    }

    proc::RunOptions options;
    options.onTick = [&]() {
        if (bar) {
            bar->update(fileSize(temporary));
        } else if (spinner) {
            spinner->tick();
        }
    };
    options.tickMilliseconds = 100;

    const proc::RunResult response = proc::run(argv, options);

    if (!response.ok()) {
        if (bar) {
            bar->fail("download failed");
        }
        if (spinner) {
            spinner->fail("download failed");
        }
        fs::remove(temporary, ec);
        result.curlExit = response.exitCode;
        result.error = describeCurlFailure(response.exitCode, response.output);
        result.hint = hintForFailure(response.exitCode, response.output);
        if (str::contains(response.output, "404")) {
            result.httpStatus = 404;
            result.error = failureText(404);
            result.hint = "Check the name and try again.";
        }
        return result;
    }

    const std::uint64_t size = fileSize(temporary);
    if (size == 0) {
        if (bar) {
            bar->fail("empty file");
        }
        if (spinner) {
            spinner->fail("empty file");
        }
        fs::remove(temporary, ec);
        result.error = "The downloaded file is empty.";
        result.hint = "The server may be misconfigured. Try again later.";
        return result;
    }

    fs::rename(temporary, destination, ec);
    if (ec) {
        if (bar) {
            bar->fail("cannot save");
        }
        if (spinner) {
            spinner->fail("cannot save");
        }
        fs::remove(temporary, ec);
        result.error = "Cannot save the downloaded file to " + fsutil::displayPath(destination) + ".";
        result.hint = ec.message();
        return result;
    }

    if (bar) {
        bar->finish(progress::humanBytes(size));
    }
    if (spinner) {
        spinner->succeed(progress::humanBytes(size));
    }

    result.ok = true;
    result.bytes = size;
    if (result.httpStatus == 0) {
        result.httpStatus = 200;
    }
    return result;
}

UploadResult uploadFile(const std::string& url, const fs::path& file, const std::string& field,
                        const std::string& label) {
    UploadResult result;
    if (!proc::exists("curl")) {
        result.error = "curl is not installed.";
        result.hint = "Run `studentmachine install` to install the required tools.";
        return result;
    }
    if (!fs::exists(file)) {
        result.error = "The file to upload does not exist: " + fsutil::displayPath(file) + ".";
        return result;
    }

    std::vector<std::string> argv = curlBase();
    argv.insert(argv.end(), {"--fail", "--max-time", "180", "-F", field + "=@" + file.string(), url});

    progress::Spinner spinner(label);
    proc::RunOptions options;
    options.onTick = [&]() { spinner.tick(); };
    const proc::RunResult response = proc::run(argv, options);

    if (!response.ok()) {
        spinner.fail("failed");
        result.error = describeCurlFailure(response.exitCode, response.output);
        result.hint = hintForFailure(response.exitCode, response.output);
        return result;
    }

    result.response = str::trim(response.output);
    const std::string lowered = str::lower(result.response);
    if (result.response.empty()) {
        spinner.fail("empty response");
        result.error = "The server returned an empty response.";
        result.hint = "Try again in a moment.";
        return result;
    }
    if (str::contains(lowered, "wrong") || str::contains(lowered, "error") ||
        str::contains(lowered, "failed")) {
        spinner.fail("rejected");
        result.error = "The server rejected the upload.";
        result.hint = result.response;
        return result;
    }

    spinner.succeed("uploaded");
    result.ok = true;
    return result;
}

bool endpointReachable(const std::string& url, std::string& error) {
    if (!proc::exists("curl")) {
        error = "curl is not installed.";
        return false;
    }
    const proc::RunResult result =
        proc::run({"curl", "--silent", "--show-error", "--head", "--location", "--max-time", "15",
                   "--proto", "=https", "--output", "/dev/null", "--write-out", "%{http_code}",
                   url},
                  {});
    if (!result.ok()) {
        error = describeCurlFailure(result.exitCode, result.output);
        return false;
    }
    const int status = std::atoi(str::trim(result.output).c_str());
    if (status <= 0) {
        error = "The server did not return an HTTP response.";
        return false;
    }
    return true;
}

std::string describeCurlFailure(int code, const std::string& output) {
    switch (code) {
        case 6:
            return "Could not resolve the server address (DNS).";
        case 7:
            return "Could not connect to the server.";
        case 28:
            return "The connection timed out.";
        case 35:
        case 60:
            return "The TLS/SSL connection could not be established.";
        case 22: {
            const int status = statusFromError(output);
            if (failureStatus(status)) {
                return failureText(status);
            }
            if (status >= 500) {
                return "The server reported an internal error (HTTP " + std::to_string(status) +
                       ").";
            }
            if (str::contains(output, "404")) {
                return failureText(404);
            }
            if (str::contains(output, "403")) {
                return failureText(403);
            }
            return "The server rejected the request.";
        }
        case 127:
            return "curl could not be started.";
        default:
            break;
    }
    const std::string trimmed = str::trim(output);
    if (!trimmed.empty()) {
        return trimmed;
    }
    return "The network request failed (curl exit code " + std::to_string(code) + ").";
}

std::string hintForFailure(int code, const std::string& output) {
    switch (code) {
        case 6:
            return "Check your internet connection and DNS configuration.";
        case 7:
            return "Check your internet connection or proxy settings.";
        case 28:
            return "The server did not respond in time. Try again.";
        case 35:
        case 60:
            return "Check the system date and installed CA certificates.";
        case 22:
            if (str::contains(output, "404")) {
                return "Check the name and try again.";
            }
            return "The server rejected the request. Try again later.";
        case 127:
            return "Run `studentmachine install` to install curl.";
        default:
            return "Check your network connection and try again.";
    }
}

}  // namespace sm::net
