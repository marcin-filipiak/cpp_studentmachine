#include "commands/commands.hpp"

#include <iostream>

#include "core/config.hpp"
#include "core/fsutil.hpp"
#include "core/git.hpp"
#include "core/net.hpp"
#include "core/proc.hpp"
#include "core/progress.hpp"
#include "core/ssh.hpp"
#include "core/strings.hpp"
#include "core/terminal.hpp"
#include "core/types.hpp"
#include "core/ui.hpp"
#include "core/workspace.hpp"

namespace sm::commands {
namespace {

struct Options {
    std::string mode;
    std::string login;
};

std::string normalizeMode(const std::string& value) {
    const std::string lowered = str::lower(str::trim(value));
    if (lowered == "u" || lowered == "use" || lowered == "1") {
        return "use";
    }
    if (lowered == "r" || lowered == "register" || lowered == "2") {
        return "register";
    }
    if (lowered == "n" || lowered == "none" || lowered == "t" || lowered == "thanks" ||
        lowered == "3") {
        return "none";
    }
    return {};
}

bool parseOptions(const std::vector<std::string>& arguments, Options& options,
                  std::string& error) {
    for (std::size_t i = 0; i < arguments.size(); ++i) {
        const std::string& argument = arguments[i];
        if (argument == "--github") {
            if (i + 1 >= arguments.size()) {
                error = "Option --github requires a value: use, register or none.";
                return false;
            }
            options.mode = normalizeMode(arguments[++i]);
        } else if (str::startsWith(argument, "--github=")) {
            options.mode = normalizeMode(argument.substr(9));
        } else if (argument == "--login") {
            if (i + 1 >= arguments.size()) {
                error = "Option --login requires a GitHub user name.";
                return false;
            }
            options.login = str::trim(arguments[++i]);
        } else if (str::startsWith(argument, "--login=")) {
            options.login = str::trim(argument.substr(8));
        } else {
            error = "Unknown option for systemup: " + argument;
            return false;
        }

        if (str::startsWith(argument, "--github") && options.mode.empty()) {
            error = "Invalid GitHub mode. Use use, register or none.";
            return false;
        }
    }
    return true;
}

std::vector<std::string> missingTools(bool github) {
    std::vector<std::string> missing;
    if (!proc::exists("curl")) {
        missing.push_back("curl");
    }
    if (!proc::exists("tar")) {
        missing.push_back("tar");
    }
    if (github) {
        const char* const required[] = {"git", "ssh", "ssh-keygen", "ssh-add", "ssh-agent"};
        for (const char* tool : required) {
            if (!proc::exists(tool)) {
                missing.push_back(tool);
            }
        }
    }
    return missing;
}

void showInstructions(const std::string& login) {
    ui::section("Finish the GitHub setup");
    ui::bullet("Create a private repository named \"student_projects\":");
    ui::hint("https://github.com/new");
    ui::bullet("Add the SSH key to your GitHub account:");
    ui::hint("https://github.com/settings/keys");
    ui::bullet("The public key is available at:");
    ui::hint(std::string(kKeyringWeb) + login + ".pub");
    ui::bullet("You can also download the key by user name from the keyring:");
    ui::hint(kKeyringWeb);
    std::cout << "\n";
    ui::info("The repository must be empty (do not add a README or .gitignore).");
}

bool confirmReset() {
    if (workspace::exists()) {
        const git::Info info = git::inspect(workspace::dir());
        if (info.changedFiles > 0) {
            ui::warn("The workspace contains " + std::to_string(info.changedFiles) +
                     " uncommitted change(s).");
            if (!ui::confirm("Discard these changes and reset the workspace?", false)) {
                return false;
            }
        }
    }
    ui::warn("This will erase the workspace, ~/.config and the shell history.");
    return ui::confirm("Reset this machine and start a new session?", false);
}

bool clearLocalState(std::string& error) {
    progress::Spinner spinner("Resetting local state");

    if (ssh::agentRunning()) {
        proc::run({"ssh-add", "-D"}, {});
    }

    if (!workspace::reset(error)) {
        spinner.fail();
        return false;
    }
    if (!fsutil::removeAll(config::dir(), error)) {
        spinner.fail();
        return false;
    }
    if (!fsutil::removeAll(fsutil::home() / ".config", error)) {
        spinner.fail();
        return false;
    }

    std::error_code ec;
    fs::remove(fsutil::home() / ".bash_history", ec);
    fs::remove(fsutil::home() / ".zsh_history", ec);

    spinner.succeed();
    return true;
}

bool prepareKey(const std::string& login, const std::string& email, bool registerMode,
                std::string& error, std::string& hint) {
    if (registerMode) {
        bool reused = false;
        progress::Spinner keySpinner("Preparing the SSH key");
        if (!ssh::ensureKeys(login, email, reused, error, hint)) {
            keySpinner.fail();
            return false;
        }
        keySpinner.succeed(reused ? "existing key reused" : "new key generated");
        if (reused) {
            ui::info("An existing key for this account was found and will be reused.");
        }

        if (!ssh::uploadKeys(login, error, hint)) {
            return false;
        }
        ui::success("Keys registered in the StudentMachine keyring.");
    } else {
        if (!ssh::hasKeyPair(login)) {
            if (!ssh::downloadKeys(login, error, hint)) {
                return false;
            }
            ui::success("SSH keys restored from the keyring.");
        } else {
            ui::info("Using the SSH key already present on this machine.");
        }
    }

    if (!ssh::addKey(login, error, hint)) {
        return false;
    }
    return true;
}

bool restoreSettings() {
    progress::Spinner spinner("Restoring saved system settings");
    std::string error;
    const std::size_t copied = workspace::restoreSettingsFromRepo(error);
    if (!error.empty()) {
        spinner.fail();
        ui::warn("Could not restore system settings: " + error);
        return false;
    }
    spinner.succeed(copied == 0 ? "no saved settings" : std::to_string(copied) + " files");
    return true;
}

}  // namespace

int systemup(const Context& context, const std::vector<std::string>& arguments) {
    Options options;
    std::string parseError;
    if (!parseOptions(arguments, options, parseError)) {
        ui::error(parseError);
        return ExitUsage;
    }

    ui::section("Starting a new session");

    if (options.mode.empty()) {
        if (context.assumeYes) {
            ui::error("When using --yes, specify --github=use|register|none.",
                      "Example: studentmachine systemup --yes --github=none");
            return ExitUsage;
        }
        const std::vector<ui::MenuItem> items = {
            {'1', "Use keyring keys", "Existing GitHub account", false},
            {'2', "Register a new key", "Create and upload a key pair", false},
            {'3', "Continue without GitHub", "Work locally only", false},
        };
        const std::optional<char> selected =
            ui::menu("How should this machine connect to GitHub?", items);
        if (!selected.has_value()) {
            ui::info("Nothing was changed.");
            return ExitAborted;
        }
        options.mode = normalizeMode(std::string(1, *selected));
        if (options.mode.empty()) {
            ui::error("Unknown option.");
            return ExitUsage;
        }
    }

    const bool useGithub = options.mode != "none";
    if (useGithub) {
        if (options.login.empty()) {
            const std::optional<std::string> login =
                ui::prompt("GitHub login", {}, config::validateLogin);
            if (!login.has_value()) {
                ui::error("A GitHub login is required for this option.",
                          "Run the command again when you have your GitHub user name.");
                return ExitAborted;
            }
            options.login = *login;
        }

        std::string loginError;
        if (!config::validateLogin(options.login, loginError)) {
            ui::error(loginError);
            return ExitUsage;
        }
    }

    const std::vector<std::string> missing = missingTools(useGithub);
    if (!missing.empty()) {
        ui::error("Missing required tools: " + str::join(missing, ", ") + ".",
                  "Run `studentmachine install` to install them.");
        return ExitDependency;
    }

    if (options.mode == "use" && !ssh::hasKeyPair(options.login)) {
        progress::Spinner check("Checking the keyring for " + options.login);
        const net::Probe probe =
            net::probe(std::string(kApiBase) + "/keyring/" + options.login);
        if (!probe.ok) {
            check.fail();
            ui::error("Cannot check the keyring for this account.", probe.error);
            return ExitNetwork;
        }
        if (probe.status == 404) {
            check.fail("not found");
            ui::error("No SSH keys are registered for \"" + options.login + "\".",
                      "Choose the register option instead: studentmachine systemup --github=register");
            return ExitFailure;
        }
        if (probe.status >= 400 || probe.status == 0) {
            check.fail();
            ui::error("The keyring returned HTTP " + std::to_string(probe.status) + ".",
                      "Try again later or choose the register option.");
            return ExitNetwork;
        }
        check.succeed();
    }

    if (!confirmReset()) {
        ui::info("Nothing was changed.");
        return ExitAborted;
    }

    std::string stateError;
    if (!clearLocalState(stateError)) {
        ui::error("Could not reset the local state.", stateError);
        return ExitFailure;
    }

    const std::string email = kDefaultEmail;

    if (options.mode == "none") {
        ui::info("GitHub integration is disabled. Your work will stay on this machine.");
        const workspace::TemplatesResult templates = workspace::downloadTemplates();
        if (!templates.ok) {
            ui::error("Could not prepare the workspace.", templates.hint);
            if (!templates.error.empty()) {
                ui::hint(templates.error);
            }
            return ExitExternal;
        }

        ui::summary(true, "Session is ready",
                    {"GitHub: not configured (local mode)",
                     "Workspace: " + fsutil::displayPath(workspace::dir()),
                     "Run `studentmachine savework` to save when GitHub is configured."});
        return ExitOk;
    }

    config::Data data;
    data.login = options.login;
    data.email = email;

    if (!git::configure(data.login, data.email, stateError)) {
        ui::error("Could not configure Git.", stateError);
        return ExitExternal;
    }

    std::string keyError;
    std::string keyHint;
    if (!prepareKey(data.login, data.email, options.mode == "register", keyError, keyHint)) {
        ui::error(keyError, keyHint);
        return ExitExternal;
    }

    if (!config::save(data, stateError)) {
        ui::error("Could not save the StudentMachine configuration.", stateError);
        return ExitFailure;
    }

    if (options.mode == "use") {
        if (!git::clone(data.login, workspace::dir(), stateError, keyHint)) {
            ui::error(stateError, keyHint);
            return ExitExternal;
        }
        restoreSettings();
    } else {
        showInstructions(data.login);
        while (true) {
            if (!ui::confirm("Repository created and the key added to GitHub?", false)) {
                ui::warn("Setup is paused. Your key is registered on the server.");
                ui::hint("Create the repository and run `studentmachine systemup` again.");
                return ExitAborted;
            }

            if (git::clone(data.login, workspace::dir(), stateError, keyHint)) {
                break;
            }
            ui::error(stateError, keyHint);
            if (!ui::confirm("Try cloning again?", true)) {
                return ExitExternal;
            }
        }

        const workspace::TemplatesResult templates = workspace::downloadTemplates();
        if (!templates.ok) {
            ui::error("The repository is ready, but templates could not be installed.",
                      templates.hint);
            if (!templates.error.empty()) {
                ui::hint(templates.error);
            }
            return ExitExternal;
        }
    }

    ui::summary(true, "Session is ready",
                {"GitHub account: " + data.login,
                 "Workspace: " + fsutil::displayPath(workspace::dir()),
                 "Use `studentmachine savework` to publish your work.",
                 "Use `studentmachine exercise <name>` to download an exercise."});
    return ExitOk;
}

}  // namespace sm::commands
