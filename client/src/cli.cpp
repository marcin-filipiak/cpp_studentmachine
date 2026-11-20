#include "cli.hpp"

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "commands/commands.hpp"
#include "core/fsutil.hpp"
#include "core/logger.hpp"
#include "core/proc.hpp"
#include "core/strings.hpp"
#include "core/terminal.hpp"
#include "core/types.hpp"
#include "core/ui.hpp"
#include "core/workspace.hpp"
#include "version.hpp"

namespace sm::cli {
namespace {

using Command = int (*)(const Context&, const std::vector<std::string>&);

std::string padded(const std::string& value, std::size_t width) {
    if (value.size() >= width) {
        return value;
    }
    return value + std::string(width - value.size(), ' ');
}

void commandLine(const std::string& command, const std::string& description) {
    std::cout << "  " << term::cyan(padded(command, 20)) << term::dim(description) << "\n";
}

int dispatch(const std::string& command, const Context& context,
             const std::vector<std::string>& arguments) {
    static const std::map<std::string, Command> commands = {
        {"install", commands::install},   {"update", commands::update},
        {"systemup", commands::systemup}, {"templates", commands::templates},
        {"exercise", commands::exercise}, {"savework", commands::savework},
        {"systemdown", commands::systemdown}, {"status", commands::status},
        {"doctor", commands::doctor},
    };

    const auto found = commands.find(command);
    if (found == commands.end()) {
        ui::error("Unknown command \"" + command + "\".",
                  "Run `studentmachine help` to see the available commands.");
        return ExitUsage;
    }
    return found->second(context, arguments);
}

int runLogged(const std::string& command, const Context& context,
              const std::vector<std::string>& arguments) {
    logger::start(command);
    int result = ExitFailure;
    try {
        result = dispatch(command, context, arguments);
    } catch (const std::exception& exception) {
        ui::error("Unexpected error: " + std::string(exception.what()));
        result = ExitFailure;
    }

    const fs::path logPath = logger::path();
    logger::stop();

    if (result != ExitOk && result != ExitAborted && result != ExitUsage && !logPath.empty()) {
        ui::hint("Full log: " + fsutil::displayPath(logPath));
    }
    return result;
}

int interactiveMenu(const Context& context) {
    while (true) {
        ui::clearScreen();
        ui::banner();
        ui::section("Main menu");
        const std::vector<ui::MenuItem> items = {
            {'1', "Start or reconfigure", "Prepare the workspace and GitHub access", false},
            {'2', "Download templates", "Refresh the project templates", false},
            {'3', "Download exercise", "Add an exercise to the workspace", false},
            {'4', "Save work", "Commit and push your work to GitHub", false},
            {'5', "Finish and power off", "Save, clean up and shut the machine down", true},
            {'6', "Show status", "Configuration and workspace overview", false},
            {'7', "Run diagnostics", "Check tools, network and configuration", false},
            {'8', "Install tools", "Install the required system packages", false},
            {'9', "Update application", "Download the latest StudentMachine", false},
            {'h', "Help", "Show commands, options and examples", false},
            {'x', "Exit", "Leave StudentMachine", false},
        };

        const std::optional<char> selected = ui::menu("What would you like to do?", items);
        if (!selected.has_value()) {
            ui::info("Goodbye.");
            return ExitOk;
        }

        int result = ExitOk;
        switch (*selected) {
            case '1':
                result = runLogged("systemup", context, {});
                break;
            case '2':
                result = runLogged("templates", context, {});
                break;
            case '3': {
                const std::optional<std::string> name = ui::prompt(
                    "Exercise name", {}, [](const std::string& value, std::string& error) {
                        return workspace::validExerciseName(value, error);
                    });
                if (!name.has_value()) {
                    continue;
                }
                result = runLogged("exercise", context, {*name});
                break;
            }
            case '4':
                result = runLogged("savework", context, {});
                break;
            case '5':
                result = runLogged("systemdown", context, {});
                break;
            case '6':
                result = runLogged("status", context, {});
                break;
            case '7':
                result = runLogged("doctor", context, {});
                break;
            case '8':
                result = runLogged("install", context, {});
                break;
            case '9':
                result = runLogged("update", context, {});
                break;
            case 'h':
                printHelp(false);
                break;
            case 'x':
                ui::info("Goodbye.");
                return ExitOk;
            default:
                ui::warn("Unknown option.");
                break;
        }

        if (selected != 'h') {
            ui::pause(result == ExitOk ? "Press Enter to return to the menu..."
                                       : "Press Enter to return to the menu.");
        }
    }
}

}  // namespace

void printVersion() {
    std::cout << kAppName << " " << kVersion << " (" << kCodename << ")\n";
}

void printHelp(bool withBanner) {
    if (withBanner) {
        ui::banner();
    }

    std::cout << "  " << term::bold("Usage") << "\n\n";
    std::cout << "    studentmachine <command> [options]\n";
    std::cout << "    studentmachine                     interactive menu\n\n";

    ui::section("Commands");
    commandLine("install", "Install the required system tools");
    commandLine("update", "Update StudentMachine to the latest version");
    commandLine("systemup", "Start a session: workspace and GitHub account setup");
    commandLine("templates", "Download and configure project templates");
    commandLine("exercise <name>", "Download an exercise into the workspace");
    commandLine("savework", "Commit and push your work to GitHub");
    commandLine("systemdown", "Save, clean up and power off the machine");
    commandLine("status", "Show configuration and workspace status");
    commandLine("doctor", "Check tools, network and configuration");
    commandLine("help", "Show this help");
    commandLine("version", "Show version information");

    ui::section("Options");
    commandLine("--yes, -y", "Assume yes for prompts (automation)");
    commandLine("--no-color", "Disable ANSI colors");
    commandLine("--verbose", "Show output of external commands");
    commandLine("--help, -h", "Show this help");
    commandLine("--version, -v", "Show version information");

    ui::section("Examples");
    std::cout << "  " << term::dim("$ studentmachine systemup") << "\n";
    std::cout << "  " << term::dim("$ studentmachine systemup --yes --github=none") << "\n";
    std::cout << "  " << term::dim("$ studentmachine exercise 01-introduction") << "\n";
    std::cout << "  " << term::dim("$ studentmachine savework") << "\n";
    std::cout << "  " << term::dim("$ studentmachine doctor") << "\n\n";
}

int run(int argc, char** argv) {
    Context context;
    bool noColor = false;
    bool helpRequested = false;
    bool versionRequested = false;
    std::string command;
    std::vector<std::string> arguments;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "-y" || argument == "--yes") {
            context.assumeYes = true;
        } else if (argument == "--no-color") {
            noColor = true;
        } else if (argument == "--verbose") {
            context.verbose = true;
        } else if (argument == "-h" || argument == "--help") {
            helpRequested = true;
        } else if (argument == "-v" || argument == "--version") {
            versionRequested = true;
        } else if (!argument.empty() && argument.front() == '-' && command.empty()) {
            std::cerr << "studentmachine: unknown global option \"" << argument << "\".\n";
            std::cerr << "Run `studentmachine help` for usage.\n";
            return ExitUsage;
        } else if (command.empty()) {
            command = argument;
        } else {
            arguments.push_back(argument);
        }
    }

    term::init(noColor);
    ui::setAssumeYes(context.assumeYes);
    proc::setVerbose(context.verbose);

    if (helpRequested) {
        printHelp();
        return ExitOk;
    }
    if (versionRequested) {
        printVersion();
        return ExitOk;
    }

    if (command == "help") {
        printHelp();
        return ExitOk;
    }
    if (command == "version") {
        printVersion();
        return ExitOk;
    }

    if (command.empty()) {
        if (fsutil::home().empty()) {
            ui::error("Could not determine your home directory.",
                      "Set the HOME environment variable before running StudentMachine.");
            return ExitFailure;
        }
        if (term::isInputTty()) {
            return interactiveMenu(context);
        }
        printHelp();
        return ExitOk;
    }

    if (fsutil::home().empty()) {
        ui::error("Could not determine your home directory.",
                  "Set the HOME environment variable before running StudentMachine.");
        return ExitFailure;
    }

    return runLogged(command, context, arguments);
}

}  // namespace sm::cli
