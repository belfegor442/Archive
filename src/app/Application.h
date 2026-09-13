#pragma once

#include "AppConfig.h"

namespace archive::app {

class Application {
public:
    explicit Application(AppConfig config);

    int run(int argc, char* argv[]);

private:
    AppConfig config_;
};

} // namespace archive::app
