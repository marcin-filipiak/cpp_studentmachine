#include "core/archive.hpp"

#include <set>

#include "core/fsutil.hpp"
#include "core/proc.hpp"
#include "core/strings.hpp"
#include "core/types.hpp"

namespace sm::archive {
namespace {

bool unsafeComponent(const fs::path& entry) {
    for (const fs::path& component : entry) {
        if (component == "..") {
            return true;
        }
    }
    return false;
}

}  // namespace

bool isSafeEntry(const std::string& entry) {
    if (entry.empty()) {
        return true;
    }
    const fs::path path(entry);
    if (path.is_absolute() || entry.front() == '/') {
        return false;
    }
    return !unsafeComponent(path);
}

Listing list(const fs::path& archive) {
    Listing listing;
    const proc::RunResult result = proc::run({"tar", "-tf", archive.string()}, {});
    if (!result.ok()) {
        listing.error = result.output.empty() ? "Cannot read the archive." : str::trim(result.output);
        return listing;
    }

    for (const std::string& raw : str::splitLines(result.output)) {
        const std::string entry = str::trim(raw);
        if (entry.empty()) {
            continue;
        }
        if (!isSafeEntry(entry)) {
            listing.error = "The archive contains an unsafe path: " + entry;
            return listing;
        }
        listing.entries.push_back(entry);
    }

    if (listing.entries.empty()) {
        listing.error = "The archive is empty.";
        return listing;
    }

    listing.ok = true;
    return listing;
}

bool extract(const fs::path& archive, const fs::path& destination, std::string& error) {
    const Listing listing = list(archive);
    if (!listing.ok) {
        error = listing.error;
        return false;
    }

    std::string directoryError;
    if (fsutil::ensureDir(destination, directoryError).empty()) {
        error = directoryError;
        return false;
    }

    const proc::RunResult result =
        proc::run({"tar", "-xf", archive.string(), "-C", destination.string(), "--no-same-owner"},
                  {});
    if (!result.ok()) {
        error = result.output.empty() ? "Cannot extract the archive." : str::trim(result.output);
        return false;
    }
    return true;
}

std::size_t countCollisions(const std::vector<std::string>& entries,
                            const fs::path& destination) {
    std::set<std::string> unique;
    for (const std::string& entry : entries) {
        if (!entry.empty() && entry.back() == '/') {
            continue;
        }
        std::error_code ec;
        if (fs::exists(destination / entry, ec)) {
            unique.insert(entry);
        }
    }
    return unique.size();
}

std::vector<std::string> topLevelFolders(const std::vector<std::string>& entries) {
    std::vector<std::string> folders;
    std::set<std::string> seen;
    for (const std::string& entry : entries) {
        const fs::path path(entry);
        auto iterator = path.begin();
        if (iterator == path.end()) {
            continue;
        }
        const std::string first = iterator->string();
        if (first.empty() || first == "." || first == "run.sh") {
            continue;
        }
        if (seen.insert(first).second) {
            folders.push_back(first);
        }
    }
    return folders;
}

}  // namespace sm::archive
