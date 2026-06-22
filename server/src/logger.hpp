#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <mutex>

namespace metis::antique {

struct LogLine {
    std::string timestamp;
    std::string level;
    std::string message;
};

class Logger {
public:
    static Logger& instance();
    void init(const std::string& log_dir, long max_mb);
    void info(const std::string& msg);
    void warn(const std::string& msg);
    void error(const std::string& msg);
    void system(const std::string& msg);

    // Read last N lines from the log file, optionally filtered by level
    std::vector<LogLine> tail(int n = 200, const std::string& level_filter = "") const;
    std::string log_path() const { return log_path_; }

private:
    Logger() = default;
    void write(const std::string& level, const std::string& msg);
    void rotate_if_needed();
    std::string timestamp() const;

    std::ofstream file_;
    mutable std::mutex mu_;
    std::string   log_path_;
    long          max_bytes_ = 10 * 1024 * 1024;
};

#define LOG_INFO(m)   metis::antique::Logger::instance().info(m)
#define LOG_WARN(m)   metis::antique::Logger::instance().warn(m)
#define LOG_ERROR(m)  metis::antique::Logger::instance().error(m)
#define LOG_SYSTEM(m) metis::antique::Logger::instance().system(m)

} // namespace metis::antique
