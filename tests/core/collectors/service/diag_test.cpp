#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <winsvc.h>
#include "ServiceCollector.hpp"
using namespace monix::collectors::service;
int main() {
    std::cerr << "Creating collector..." << std::endl;
    ServiceCollector collector;
    std::cerr << "Starting..." << std::endl;
    bool ok = collector.start();
    std::cerr << "start() = " << ok << std::endl;
    std::cerr << "isRunning() = " << collector.isRunning() << std::endl;
    auto snap = collector.snapshot();
    std::cerr << "snapshot size = " << snap.size() << std::endl;
    std::cerr << "eventsEmitted = " << collector.eventsEmitted() << std::endl;
    collector.stop();
    std::cerr << "Done" << std::endl;
    return 0;
}
