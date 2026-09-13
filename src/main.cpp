#include "app/Application.h"
#include "app/AppConfig.h"

int main(int argc, char* argv[]) {
    auto config = archive::app::AppConfig::default_config();
    archive::app::Application app(std::move(config));
    return app.run(argc, argv);
}
