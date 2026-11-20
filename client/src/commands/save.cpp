#include "commands/commands.hpp"

#include "core/config.hpp"
#include "core/fsutil.hpp"
#include "core/git.hpp"
#include "core/progress.hpp"
#include "core/ssh.hpp"
#include "core/types.hpp"
#include "core/ui.hpp"
#include "core/workspace.hpp"

namespace sm::commands {

SaveReport saveSession(const std::string& message) {
    SaveReport report;
    const config::Data data = config::load();
    report.login = data.login;

    if (!data.hasGithub()) {
        report.noGithub = true;
        report.error = "GitHub is not configured.";
        report.hint = "Run `studentmachine systemup` and choose a GitHub option.";
        return report;
    }

    report.repository = git::repositoryUrl(data.login);

    if (!workspace::exists()) {
        report.error = "The workspace " + fsutil::displayPath(workspace::dir()) + " does not exist.";
        report.hint = "Run `studentmachine systemup` or `studentmachine templates` first.";
        return report;
    }

    progress::Spinner syncSpinner("Syncing system settings");
    std::string syncError;
    const std::size_t copied = workspace::syncSettingsToRepo(syncError);
    if (!syncError.empty()) {
        syncSpinner.fail();
        report.error = syncError;
        report.hint = "Check the permissions of ~/.config.";
        return report;
    }
    syncSpinner.succeed(copied == 0 ? "nothing to sync"
                                    : std::to_string(copied) + " files");

    if (!ssh::hasKeyPair(data.login)) {
        ui::info("The SSH key is missing locally. Restoring it from the keyring.");
        std::string downloadError;
        std::string downloadHint;
        if (!ssh::downloadKeys(data.login, downloadError, downloadHint)) {
            report.error = downloadError;
            report.hint = downloadHint;
            return report;
        }
    }

    std::string keyError;
    std::string keyHint;
    if (!ssh::addKey(data.login, keyError, keyHint)) {
        report.error = keyError;
        report.hint = keyHint;
        return report;
    }

    const git::SaveResult saved =
        git::save(workspace::dir(), data.login, data.email, message);

    switch (saved.state) {
        case git::SaveState::Saved:
            report.ok = true;
            report.commit = saved.commit;
            report.repository = git::repositoryUrl(data.login);
            break;
        case git::SaveState::NothingToSave:
            report.ok = true;
            report.nothingToSave = true;
            break;
        case git::SaveState::NotRepository:
        case git::SaveState::Error:
            report.error = saved.detail;
            report.hint = saved.hint;
            break;
    }
    return report;
}

}  // namespace sm::commands
