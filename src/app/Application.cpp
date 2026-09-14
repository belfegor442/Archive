#include "Application.h"
#include "../ui/MainWindow.h"
#include "../filesystem/StagingManager.h"

#include <QApplication>
#include <iostream>

namespace archive::app {

Application::Application(AppConfig config)
    : config_(std::move(config))
{}

int Application::run(int argc, char* argv[]) {
    config_.ensure_directories();

    recover_abandoned_staging();

    QApplication qt_app(argc, argv);
    qt_app.setApplicationName("Archive");
    qt_app.setOrganizationName("Archive");

    MainWindow window(config_);
    window.show();

    return qt_app.exec();
}

void Application::recover_abandoned_staging() {
    try {
        filesystem::StagingManager staging(config_.data_dir);
        auto abandoned = staging.detect_abandoned_staging();

        for (const auto& op : abandoned) {
            auto s = filesystem::StagingManager::string_to_state(op.state);

            switch (s) {
                case filesystem::StagingState::Finalizing:
                case filesystem::StagingState::Staged:
                case filesystem::StagingState::Preparing:
                case filesystem::StagingState::Abandoned:
                case filesystem::StagingState::Corrupted:
                case filesystem::StagingState::RollingBack:
                    staging.restore_backups(op.operation_id);
                    break;
                case filesystem::StagingState::RolledBack:
                case filesystem::StagingState::Committed:
                    break;
            }

            staging.cleanup_staging(op.operation_id);
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Failed to recover abandoned staging: " << e.what() << std::endl;
    }
}

} // namespace archive::app
