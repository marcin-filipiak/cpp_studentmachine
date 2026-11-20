#include "core/workspace.hpp"

#include <system_error>

#include "core/archive.hpp"
#include "core/fsutil.hpp"
#include "core/net.hpp"
#include "core/proc.hpp"
#include "core/progress.hpp"
#include "core/strings.hpp"

namespace sm::workspace {
namespace {

std::string tail(const std::string& text, std::size_t lines) {
    const std::vector<std::string> all = str::splitLines(str::trim(text));
    if (all.empty()) {
        return {};
    }
    const std::size_t start = all.size() > lines ? all.size() - lines : 0;
    std::string result;
    for (std::size_t i = start; i < all.size(); ++i) {
        if (!result.empty()) {
            result += "\n";
        }
        result += all[i];
    }
    return result;
}

bool isTemplatesArchive(const std::vector<std::string>& entries) {
    for (const std::string& entry : entries) {
        std::string clean = entry;
        while (str::startsWith(clean, "./")) {
            clean = clean.substr(2);
        }
        if (clean.empty()) {
            continue;
        }
        const fs::path path(clean);
        auto iterator = path.begin();
        if (iterator == path.end() || iterator->string() != "student_projects") {
            return false;
        }
    }
    return true;
}

bool runSetupScript(std::string& error) {
    const fs::path script = dir() / "run.sh";
    std::error_code ec;
    if (!fs::is_regular_file(script, ec)) {
        return true;
    }

    fs::permissions(script,
                    fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec |
                        fs::perms::group_read | fs::perms::group_exec,
                    fs::perm_options::replace, ec);

    progress::Spinner spinner("Configuring workspace");
    proc::RunOptions options;
    options.workingDirectory = dir().string();
    options.timeoutSeconds = 600;
    options.onTick = [&]() { spinner.tick(); };
    const proc::RunResult result = proc::run({"sh", "run.sh"}, options);

    if (!result.ok()) {
        spinner.fail();
        error = "The workspace setup script failed.";
        const std::string detail = tail(result.output, 8);
        if (!detail.empty()) {
            error += "\n" + detail;
        }
        return false;
    }

    spinner.succeed();
    fs::remove(script, ec);
    return true;
}

}  // namespace

fs::path dir() {
    return fsutil::home() / "student_projects";
}

fs::path osConfigDir() {
    return dir() / "os_config" / "config";
}

bool exists() {
    std::error_code ec;
    return fs::is_directory(dir(), ec);
}

bool reset(std::string& error) {
    return fsutil::removeAll(dir(), error);
}

std::size_t syncSettingsToRepo(std::string& error) {
    const fs::path source = fsutil::home() / ".config";
    std::error_code ec;
    if (!fs::is_directory(source, ec)) {
        return 0;
    }

    if (fsutil::ensureDir(osConfigDir(), error).empty()) {
        return 0;
    }

    std::size_t copied = 0;
    if (!fsutil::copyTree(source, osConfigDir(), error, &copied)) {
        return 0;
    }
    return copied;
}

std::size_t restoreSettingsFromRepo(std::string& error) {
    std::error_code ec;
    if (!fs::is_directory(osConfigDir(), ec)) {
        return 0;
    }

    const fs::path destination = fsutil::home() / ".config";
    if (fsutil::ensureDir(destination, error).empty()) {
        return 0;
    }

    std::size_t copied = 0;
    if (!fsutil::copyTree(osConfigDir(), destination, error, &copied)) {
        return 0;
    }
    return copied;
}

TemplatesResult downloadTemplates() {
    TemplatesResult result;
    if (!proc::exists("tar")) {
        result.error = "tar is not installed.";
        result.hint = "Run `studentmachine install` to install the required tools.";
        return result;
    }

    fsutil::TempDir temporary;
    if (!temporary.valid()) {
        result.error = "Cannot create a temporary directory.";
        return result;
    }

    const std::string url = std::string(kApiBase) + "/files/student_projects.tar";
    const net::DownloadResult downloaded =
        net::download(url, temporary.path() / "student_projects.tar", "Downloading project templates");
    if (!downloaded.ok) {
        result.error = downloaded.error;
        result.hint = downloaded.hint;
        return result;
    }

    const archive::Listing listing = archive::list(temporary.path() / "student_projects.tar");
    if (!listing.ok) {
        result.error = listing.error;
        result.hint = "The template archive is invalid.";
        return result;
    }
    if (!isTemplatesArchive(listing.entries)) {
        result.error = "The template archive has an unexpected layout.";
        result.hint = "The server may be misconfigured. Try again later.";
        return result;
    }

    progress::Spinner extracting("Extracting templates");
    std::string extractError;
    if (!archive::extract(temporary.path() / "student_projects.tar", fsutil::home(),
                          extractError)) {
        extracting.fail();
        result.error = extractError;
        result.hint = "Check the free space in your home directory.";
        return result;
    }
    extracting.succeed();

    if (!exists()) {
        result.error = "The templates did not create a student_projects directory.";
        result.hint = "The server may be misconfigured. Try again later.";
        return result;
    }

    std::string setupError;
    if (!runSetupScript(setupError)) {
        result.error = setupError;
        result.hint = "The run.sh script is kept in " + fsutil::displayPath(dir()) +
                      " for inspection.";
        return result;
    }

    result.ok = true;
    return result;
}

bool validExerciseName(const std::string& name, std::string& error) {
    if (name.empty()) {
        error = "Exercise name cannot be empty.";
        return false;
    }
    if (name.size() > 64) {
        error = "Exercise name is too long.";
        return false;
    }
    if (name.front() == '.' || name.find("..") != std::string::npos) {
        error = "Exercise name is not valid.";
        return false;
    }
    for (char character : name) {
        const bool allowed = (character >= 'a' && character <= 'z') ||
                             (character >= 'A' && character <= 'Z') ||
                             (character >= '0' && character <= '9') || character == '-' ||
                             character == '_' || character == '.';
        if (!allowed) {
            error = "Exercise name may only contain letters, digits, dots, hyphens and underscores.";
            return false;
        }
    }
    return true;
}

ExerciseResult downloadExercise(const std::string& name,
                                const std::function<bool(std::size_t)>& confirmOverwrite) {
    ExerciseResult result;

    std::string nameError;
    if (!validExerciseName(name, nameError)) {
        result.error = nameError;
        return result;
    }
    if (!exists()) {
        result.error = "The workspace " + fsutil::displayPath(dir()) + " does not exist.";
        result.hint = "Run `studentmachine templates` first.";
        return result;
    }
    if (!proc::exists("tar")) {
        result.error = "tar is not installed.";
        result.hint = "Run `studentmachine install` to install the required tools.";
        return result;
    }

    fsutil::TempDir temporary;
    if (!temporary.valid()) {
        result.error = "Cannot create a temporary directory.";
        return result;
    }

    const std::string url = std::string(kApiBase) + "/files/" + name + ".tar";
    const net::DownloadResult downloaded =
        net::download(url, temporary.path() / (name + ".tar"), "Downloading exercise " + name);
    if (!downloaded.ok) {
        if (downloaded.httpStatus == 404 || str::contains(downloaded.error, "404")) {
            result.status = ExerciseResult::Status::NotFound;
            result.error = "Exercise \"" + name + "\" was not found on the server.";
            result.hint = "Check the exercise name and try again.";
            return result;
        }
        result.error = downloaded.error;
        result.hint = downloaded.hint;
        return result;
    }

    const fs::path archivePath = temporary.path() / (name + ".tar");
    const archive::Listing listing = archive::list(archivePath);
    if (!listing.ok) {
        result.error = listing.error;
        result.hint = "The exercise archive is invalid.";
        return result;
    }

    result.collisions = archive::countCollisions(listing.entries, dir());
    if (result.collisions > 0) {
        if (!confirmOverwrite) {
            result.status = ExerciseResult::Status::Collisions;
            return result;
        }
        if (!confirmOverwrite(result.collisions)) {
            result.status = ExerciseResult::Status::Cancelled;
            return result;
        }
    }

    progress::Spinner extracting("Extracting exercise");
    std::string extractError;
    if (!archive::extract(archivePath, dir(), extractError)) {
        extracting.fail();
        result.error = extractError;
        result.hint = "Check the free space in your home directory.";
        return result;
    }
    extracting.succeed();

    result.folders = archive::topLevelFolders(listing.entries);

    std::string setupError;
    if (!runSetupScript(setupError)) {
        result.error = setupError;
        result.hint = "The run.sh script is kept in " + fsutil::displayPath(dir()) +
                      " for inspection.";
        return result;
    }

    result.status = ExerciseResult::Status::Ok;
    return result;
}

}  // namespace sm::workspace
