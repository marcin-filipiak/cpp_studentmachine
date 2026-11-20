#include "core/progress.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include "core/strings.hpp"
#include "core/terminal.hpp"

namespace sm::progress {
namespace {

const char* const kUtf8Frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
const char kAsciiFrames[] = {'|', '/', '-', '\\'};
constexpr std::size_t kAsciiFrameCount = sizeof(kAsciiFrames) / sizeof(kAsciiFrames[0]);
constexpr std::size_t kUtf8FrameCount = sizeof(kUtf8Frames) / sizeof(kUtf8Frames[0]);

std::string frame(std::size_t index) {
    if (term::unicode()) {
        return kUtf8Frames[index % kUtf8FrameCount];
    }
    return std::string(1, kAsciiFrames[index % kAsciiFrameCount]);
}

std::string checkMark(bool success) {
    if (success) {
        return term::green(term::glyph("✔", "[OK]"));
    }
    return term::red(term::glyph("✖", "[FAIL]"));
}

double secondsSince(std::chrono::steady_clock::time_point start) {
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

}  // namespace

std::string humanBytes(std::uint64_t bytes) {
    static const char* const units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    std::size_t unit = 0;
    while (value >= 1024.0 && unit + 1 < sizeof(units) / sizeof(units[0])) {
        value /= 1024.0;
        ++unit;
    }
    std::ostringstream text;
    if (unit == 0) {
        text << bytes << " " << units[unit];
    } else {
        text << std::fixed << std::setprecision(value < 10.0 ? 1 : 0) << value << " " << units[unit];
    }
    return text.str();
}

std::string humanDuration(double seconds) {
    std::ostringstream text;
    if (seconds < 1.0) {
        text << "<1 s";
    } else if (seconds < 60.0) {
        text << std::fixed << std::setprecision(seconds < 10.0 ? 1 : 0) << seconds << " s";
    } else {
        const int totalSeconds = static_cast<int>(std::round(seconds));
        text << (totalSeconds / 60) << "m " << (totalSeconds % 60) << "s";
    }
    return text.str();
}

Spinner::Spinner(std::string label)
    : label_(std::move(label)), tty_(term::isOutputTty()), active_(true) {
    started_ = std::chrono::steady_clock::now();
    lastDraw_ = started_;
    if (tty_) {
        draw();
    }
}

Spinner::~Spinner() {
    if (active_) {
        fail("interrupted");
    }
}

void Spinner::setLabel(const std::string& label) {
    label_ = label;
    if (active_ && tty_) {
        draw();
    }
}

void Spinner::draw() {
    std::cout << "\r\033[K  " << term::cyan(frame(frame_)) << " " << label_ << "..."
              << std::flush;
}

void Spinner::tick() {
    if (!active_ || !tty_) {
        return;
    }
    const auto now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDraw_).count() < 90) {
        return;
    }
    ++frame_;
    lastDraw_ = now;
    draw();
}

void Spinner::stop(bool success, const std::string& detail) {
    if (!active_) {
        return;
    }
    active_ = false;

    if (tty_) {
        std::cout << "\r\033[K";
    }
    std::cout << "  " << checkMark(success) << " " << label_;
    if (!detail.empty()) {
        std::cout << "  " << term::dim(detail);
    }
    std::cout << "  " << term::dim("(" + humanDuration(secondsSince(started_)) + ")") << "\n"
              << std::flush;
}

void Spinner::succeed(const std::string& detail) {
    stop(true, detail);
}

void Spinner::fail(const std::string& detail) {
    stop(false, detail);
}

void Spinner::cancel() {
    if (!active_) {
        return;
    }
    active_ = false;
    if (tty_) {
        std::cout << "\r\033[K" << std::flush;
    }
}

Bar::Bar(std::string label, std::uint64_t totalBytes)
    : label_(std::move(label)), total_(totalBytes), tty_(term::isOutputTty()), active_(true) {
    started_ = std::chrono::steady_clock::now();
    lastDraw_ = started_;
    lastSample_ = started_;
    if (tty_) {
        draw();
    }
}

Bar::~Bar() {
    if (active_) {
        fail("interrupted");
    }
}

std::string Bar::details() const {
    std::ostringstream text;
    if (total_ > 0) {
        const int percent = static_cast<int>(std::min<std::uint64_t>(100, bytes_ * 100 / total_));
        text << percent << "%  " << humanBytes(bytes_) << " / " << humanBytes(total_);
    } else {
        text << humanBytes(bytes_);
    }
    if (speed_ > 0.5) {
        text << "  " << humanBytes(static_cast<std::uint64_t>(speed_)) << "/s";
    }
    return text.str();
}

void Bar::draw() {
    if (!tty_) {
        return;
    }

    const int width = term::width();
    const std::string detail = details();
    std::string bar;
    std::string label = label_;

    if (total_ > 0) {
        const int reserved = static_cast<int>(detail.size()) + 8;
        const int barWidth =
            std::clamp(width - reserved - static_cast<int>(label.size()) - 6, 8, 32);
        const int filled =
            static_cast<int>(std::min<std::uint64_t>(barWidth, bytes_ * barWidth / total_));
        std::string filledText;
        for (int i = 0; i < filled; ++i) {
            filledText += term::unicode() ? "█" : "#";
        }
        std::string emptyText;
        for (int i = filled; i < barWidth; ++i) {
            emptyText += term::unicode() ? "░" : "-";
        }
        bar = "[" + term::green(filledText) + term::dim(emptyText) + "] ";
        label = str::ellipsize(label, static_cast<std::size_t>(
                                          std::max(8, width - reserved - barWidth - 6)));
    } else {
        label = str::ellipsize(label, static_cast<std::size_t>(std::max(8, width - 24)));
        bar = term::cyan(frame(frame_)) + " ";
    }

    std::cout << "\r\033[K  " << label << " " << bar << detail << std::flush;
}

void Bar::update(std::uint64_t bytes) {
    if (!active_) {
        return;
    }
    bytes_ = bytes;

    const auto now = std::chrono::steady_clock::now();
    const double elapsed =
        std::chrono::duration<double>(now - lastSample_).count();
    if (elapsed >= 0.4) {
        const double current =
            static_cast<double>(bytes_ - lastBytes_) / std::max(0.001, elapsed);
        speed_ = speed_ <= 0.0 ? current : (speed_ * 0.65 + current * 0.35);
        lastSample_ = now;
        lastBytes_ = bytes_;
    }

    if (!tty_) {
        return;
    }
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastDraw_).count() < 100) {
        return;
    }
    ++frame_;
    lastDraw_ = now;
    draw();
}

void Bar::finish(const std::string& detail) {
    if (!active_) {
        return;
    }
    active_ = false;
    const std::string info = detail.empty() ? details() : detail;
    if (tty_) {
        std::cout << "\r\033[K";
    }
    std::cout << "  " << checkMark(true) << " " << label_ << "  " << term::dim(info) << "  "
              << term::dim("(" + humanDuration(secondsSince(started_)) + ")") << "\n"
              << std::flush;
}

void Bar::fail(const std::string& detail) {
    if (!active_) {
        return;
    }
    active_ = false;
    if (tty_) {
        std::cout << "\r\033[K";
    }
    std::cout << "  " << checkMark(false) << " " << label_;
    if (!detail.empty()) {
        std::cout << "  " << term::dim(detail);
    }
    std::cout << "  " << term::dim("(" + humanDuration(secondsSince(started_)) + ")") << "\n"
              << std::flush;
}

}  // namespace sm::progress
