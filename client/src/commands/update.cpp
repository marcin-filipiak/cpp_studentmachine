#include "commands/commands.hpp"

#include <sys/stat.h>

#include <string>

#include "core/fsutil.hpp"
#include "core/net.hpp"
#include "core/proc.hpp"
#include "core/progress.hpp"
#include "core/strings.hpp"
#include "core/terminal.hpp"
#include "core/types.hpp"
#include "core/ui.hpp"
#include "version.hpp"

namespace sm::commands {
namespace {

bool isElfBinary(const fs::path& file) {
    std::string content;
    if (!fsutil::readFile(file, content) || content.size() < 4) {
        return false;
    }
    return content.compare(0, 4, std::string("\x7f" "ELF", 4)) == 0;
}

}  // namespace

int selfUpdate(bool announce) {
    if (announce) {
        ui::section("Updating StudentMachine");
    }

    if (!proc::exists("curl")) {
        ui::error("curl is not installed.",
                  "Run `studentmachine install` to install the required tools.");
        return ExitDependency;
    }
    if (!proc::isRoot() && !proc::exists("sudo")) {
        ui::error("sudo is required to install StudentMachine to /bin.",
                  "Run this command as root or install sudo.");
        return ExitDependency;
    }

    fsutil::TempDir temporary;
    if (!temporary.valid()) {
        ui::error("Cannot create a temporary directory.");
        return ExitFailure;
    }

    const fs::path binary = temporary.path() / "studentmachine";
    const net::DownloadResult downloaded =
        net::download(kBinaryUrl, binary, "Downloading StudentMachine");
    if (!downloaded.ok) {
        ui::error("Could not download StudentMachine.", downloaded.hint);
        if (!downloaded.error.empty()) {
            ui::hint(downloaded.error);
        }
        return ExitNetwork;
    }

    if (!isElfBinary(binary)) {
        ui::error("The downloaded file is not a valid Linux executable.",
                  "The server may be misconfigured. Please try again later.");
        return ExitExternal;
    }

    ::chmod(binary.c_str(), 0700);
    const proc::RunResult reported = proc::run({binary.string(), "--version"}, {});
    if (!reported.ok()) {
        ui::error("The downloaded StudentMachine binary cannot run on this system.",
                  "Install the required runtime libraries or update the system.");
        return ExitExternal;
    }
    const std::string version = str::trim(reported.output);

    progress::Spinner spinner("Installing StudentMachine to /bin/studentmachine");
    proc::RunOptions installOptions;
    installOptions.timeoutSeconds = 120;
    const proc::RunResult installed =
        proc::run(proc::elevate({"install", "-m", "0755", binary.string(), "/bin/studentmachine"}),
                  installOptions);
    if (!installed.ok()) {
        spinner.fail();
        ui::error("Could not install StudentMachine to /bin/studentmachine.",
                  "Make sure you have administrator rights.");
        const std::string detail = str::trim(installed.output);
        if (!detail.empty()) {
            ui::hint(detail);
        }
        return ExitExternal;
    }
    spinner.succeed();

    if (!version.empty()) {
        ui::success("StudentMachine is up to date: " + version);
    } else {
        ui::success("StudentMachine has been updated.");
    }
    return ExitOk;
}

int update(const Context& context, const std::vector<std::string>& arguments) {
    if (!arguments.empty()) {
        ui::error("The update command does not take any arguments.");
        return ExitUsage;
    }

    const int result = selfUpdate(true);
    if (result == ExitOk) {
        ui::summary(true, "Update complete",
                    {"Your GitHub configuration was preserved.",
                     "Current version: " + std::string(kVersion)});
    }
    (void)context;
    return result;
}

}  // namespace sm::commands
