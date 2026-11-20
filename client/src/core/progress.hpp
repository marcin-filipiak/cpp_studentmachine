#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace sm::progress {

std::string humanBytes(std::uint64_t bytes);
std::string humanDuration(double seconds);

class Spinner {
public:
    explicit Spinner(std::string label = {});
    ~Spinner();

    Spinner(const Spinner&) = delete;
    Spinner& operator=(const Spinner&) = delete;

    void setLabel(const std::string& label);
    void tick();
    void succeed(const std::string& detail = {});
    void fail(const std::string& detail = {});
    void cancel();

private:
    void draw();
    void stop(bool success, const std::string& detail);

    std::string label_;
    bool tty_ = false;
    bool active_ = false;
    std::size_t frame_ = 0;
    std::chrono::steady_clock::time_point started_;
    std::chrono::steady_clock::time_point lastDraw_;
};

class Bar {
public:
    Bar(std::string label, std::uint64_t totalBytes);
    ~Bar();

    Bar(const Bar&) = delete;
    Bar& operator=(const Bar&) = delete;

    void update(std::uint64_t bytes);
    void finish(const std::string& detail = {});
    void fail(const std::string& detail = {});

private:
    void draw();
    std::string details() const;

    std::string label_;
    std::uint64_t total_ = 0;
    std::uint64_t bytes_ = 0;
    std::uint64_t lastBytes_ = 0;
    double speed_ = 0.0;
    bool tty_ = false;
    bool active_ = false;
    std::size_t frame_ = 0;
    std::chrono::steady_clock::time_point started_;
    std::chrono::steady_clock::time_point lastDraw_;
    std::chrono::steady_clock::time_point lastSample_;
};

}  // namespace sm::progress
