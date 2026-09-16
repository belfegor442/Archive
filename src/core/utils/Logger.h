#pragma once

#include <string>
#include <mutex>

namespace archive::core {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Fatal };

class Logger {
public:
    static Logger& instance();

    void set_level(LogLevel level);
    LogLevel level() const;

    void log(LogLevel level, const char* file, int line, const std::string& msg);

private:
    Logger() = default;
    LogLevel level_ = LogLevel::Info;
    mutable std::mutex mutex_;
    static const char* level_name(LogLevel level);
};

void log_trace(const char* file, int line, const std::string& msg);
void log_debug(const char* file, int line, const std::string& msg);
void log_info(const char* file, int line, const std::string& msg);
void log_warn(const char* file, int line, const std::string& msg);
void log_error(const char* file, int line, const std::string& msg);
void log_fatal(const char* file, int line, const std::string& msg);

} // namespace archive::core

#define LOG_TRACE(msg) ::archive::core::log_trace(__FILE__, __LINE__, msg)
#define LOG_DEBUG(msg) ::archive::core::log_debug(__FILE__, __LINE__, msg)
#define LOG_INFO(msg)  ::archive::core::log_info(__FILE__, __LINE__, msg)
#define LOG_WARN(msg)  ::archive::core::log_warn(__FILE__, __LINE__, msg)
#define LOG_ERROR(msg) ::archive::core::log_error(__FILE__, __LINE__, msg)
#define LOG_FATAL(msg) ::archive::core::log_fatal(__FILE__, __LINE__, msg)
