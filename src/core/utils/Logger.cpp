#include "Logger.h"

#include <iostream>
#include <chrono>
#include <ctime>

namespace archive::core {

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::set_level(LogLevel level) {
    std::lock_guard lock(mutex_);
    level_ = level;
}

LogLevel Logger::level() const {
    std::lock_guard lock(mutex_);
    return level_;
}

const char* Logger::level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Trace: return "TRACE";
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Fatal: return "FATAL";
    }
    return "?????";
}

void Logger::log(LogLevel level, const char* file, int line, const std::string& msg) {
    std::lock_guard lock(mutex_);
    if (level < level_) return;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::tm utc{};
    gmtime_s(&utc, &time_t);

    char timestamp[20];
    std::snprintf(timestamp, sizeof(timestamp), "%04d-%02d-%02d %02d:%02d:%02d",
                  utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday,
                  utc.tm_hour, utc.tm_min, utc.tm_sec);

    std::cerr << "[" << timestamp << "] [" << level_name(level) << "] " << msg;
    if (file) {
        std::cerr << " (" << file << ":" << line << ")";
    }
    std::cerr << "\n";
}

void log_trace(const char* file, int line, const std::string& msg) {
    Logger::instance().log(LogLevel::Trace, file, line, msg);
}

void log_debug(const char* file, int line, const std::string& msg) {
    Logger::instance().log(LogLevel::Debug, file, line, msg);
}

void log_info(const char* file, int line, const std::string& msg) {
    Logger::instance().log(LogLevel::Info, file, line, msg);
}

void log_warn(const char* file, int line, const std::string& msg) {
    Logger::instance().log(LogLevel::Warn, file, line, msg);
}

void log_error(const char* file, int line, const std::string& msg) {
    Logger::instance().log(LogLevel::Error, file, line, msg);
}

void log_fatal(const char* file, int line, const std::string& msg) {
    Logger::instance().log(LogLevel::Fatal, file, line, msg);
}

} // namespace archive::core
