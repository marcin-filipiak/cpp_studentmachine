#include "commands/commands.hpp"

#include <iostream>

#include "core/fsutil.hpp"
#include "core/proc.hpp"
#include "core/progress.hpp"
#include "core/strings.hpp"
#include "core/terminal.hpp"
#include "core/ui.hpp"
#include "core/types.hpp"

namespace sm::commands {
namespace {

const std::vector<std::string> kPackages = {
    "g++",       "nano",              "mc",       "vim", "git",
    "apache2",   "mariadb-server",    "mariadb-client", "phpmyadmin",
    "curl",      "ca-certificates",   "openssh-client",
};

std::vector<std::string> aptCommand(const std::vector<std::string>& arguments) {
    std::vector<std::string> command;
    if (!proc::isRoot()) {
        command.push_back("sudo");
    }
    command.push_back("env");
    command.push_back("DEBIAN_FRONTEND=noninteractive");
    command.push_back("apt-get");
    command.insert(command.end(), arguments.begin(), arguments.end());
    return command;
}

void showOutputTail(const std::string& output) {
    const std::vector<std::string> lines = str::splitLines(str::trim(output));
    const std::size_t start = lines.size() > 8 ? lines.size() - 8 : 0;
    for (std::size_t i = start; i < lines.size(); ++i) {
        std::cout << "    " << term::dim(lines[i]) << "\n";
    }
}

}  // namespace

int install(const Context& context, const std::vector<std::string>& arguments) {
    if (!arguments.empty()) {
        ui::error("The install command does not take any arguments.");
        return ExitUsage;
    }

    ui::section("Installing required tools");

    if (!proc::exists("apt-get")) {
        ui::error("apt-get is not available on this system.",
                  "StudentMachine installs packages with APT (Debian/Ubuntu).");
        return ExitDependency;
    }

    if (!proc::isRoot()) {
        if (!proc::exists("sudo")) {
            ui::error("sudo is not installed and StudentMachine is not running as root.",
                      "Run this command as root or install sudo first.");
            return ExitDependency;
        }
        const proc::RunResult passwordless = [&]() {
            proc::RunOptions quick;
            quick.timeoutSeconds = 5;
            return proc::run({"sudo", "-n", "true"}, quick);
        }();
        if (!passwordless.ok() && !term::isInputTty()) {
            ui::error("sudo requires a password, but no terminal is available.",
                      "Run the command as root or configure passwordless sudo.");
            return ExitDependency;
        }
        if (!passwordless.ok()) {
            ui::info("sudo may ask for your password.");
        }
    }

    ui::kv("Packages", str::join(kPackages, ", "), 10);
    if (!ui::confirm("Install or update these packages now?", true)) {
        ui::info("Nothing was changed.");
        return ExitAborted;
    }

    progress::Spinner refresh("Updating package lists");
    proc::RunOptions options;
    options.timeoutSeconds = 900;
    options.onTick = [&]() { refresh.tick(); };
    proc::RunResult result = proc::run(aptCommand({"update"}), options);
    if (!result.ok()) {
        refresh.fail();
        ui::error("Could not update the package list.",
                  "Check your internet connection and APT configuration.");
        showOutputTail(result.output);
        return ExitExternal;
    }
    refresh.succeed();

    progress::Spinner packages("Installing packages (this may take several minutes)");
    options.timeoutSeconds = 3600;
    options.onTick = [&]() { packages.tick(); };
    std::vector<std::string> installArguments = {"install", "-y"};
    installArguments.insert(installArguments.end(), kPackages.begin(), kPackages.end());
    result = proc::run(aptCommand(installArguments), options);
    if (!result.ok()) {
        packages.fail();
        ui::error("Package installation failed.",
                  "Review the output below, then run the command again.");
        showOutputTail(result.output);
        return ExitExternal;
    }
    packages.succeed();

    const int updateResult = selfUpdate(true);
    if (updateResult != ExitOk) {
        ui::warn("The tools are installed, but StudentMachine could not be updated.");
    }

    ui::summary(true, "Installation complete", {"Required tools are available.",
                                                "You can now run `studentmachine systemup`."});
    (void)context;
    return ExitOk;
}

}  // namespace sm::commands
