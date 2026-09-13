#include "Application.h"
#include "../ui/MainWindow.h"

#include <QApplication>

namespace archive::app {

Application::Application(AppConfig config)
    : config_(std::move(config))
{}

int Application::run(int argc, char* argv[]) {
    config_.ensure_directories();

    QApplication qt_app(argc, argv);
    qt_app.setApplicationName("Archive");
    qt_app.setOrganizationName("Archive");

    MainWindow window(config_);
    window.show();

    return qt_app.exec();
}

} // namespace archive::app
