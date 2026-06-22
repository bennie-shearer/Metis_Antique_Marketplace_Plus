#include "logger.hpp"
#include "ws_log.hpp"
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <fstream>
#include <deque>

namespace metis::antique {

Logger& Logger::instance() { static Logger inst; return inst; }

void Logger::init(const std::string& log_dir, long max_mb) {
    std::lock_guard<std::mutex> lk(mu_);
    max_bytes_ = max_mb * 1024 * 1024;
    std::filesystem::create_directories(log_dir);
    log_path_ = log_dir + "/antique_marketplace_plus.log";
    file_.open(log_path_, std::ios::app);
}

std::string Logger::timestamp() const {
    using namespace std::chrono;
    auto now = system_clock::now();
    auto ms  = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream ss;
    ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S")
       << '.' << std::setw(3) << std::setfill('0') << ms.count();
    return ss.str();
}

void Logger::rotate_if_needed() {
    if (!file_.is_open()) return;
    std::error_code ec;
    auto sz = static_cast<long>(std::filesystem::file_size(log_path_, ec));
    if (!ec && sz >= max_bytes_) {
        file_.close();
        std::filesystem::rename(log_path_, log_path_ + ".1", ec);
        file_.open(log_path_, std::ios::app);
    }
}

void Logger::write(const std::string& level, const std::string& msg) {
    std::lock_guard<std::mutex> lk(mu_);
    rotate_if_needed();
    std::string line = "[" + timestamp() + "] [" + level + "] " + msg + "\n";
    if (file_.is_open()) file_ << line << std::flush;
    std::cerr << line;
    // Broadcast to WebSocket subscribers (strip trailing newline for JSON)
    std::string broadcast_line = line;
    if (!broadcast_line.empty() && broadcast_line.back() == '\n')
        broadcast_line.pop_back();
    LogBroadcaster::instance().publish(broadcast_line);
}

void Logger::info(const std::string& m)   { write("INFO",   m); }
void Logger::warn(const std::string& m)   { write("WARN",   m); }
void Logger::error(const std::string& m)  { write("ERROR",  m); }
void Logger::system(const std::string& m) { write("system", m); }

// Parse a log line: "[2026-06-20 14:32:01.123] [INFO] message text"
static LogLine parse_line(const std::string& line) {
    LogLine out;
    out.message = line; // fallback — raw line
    if (line.empty() || line[0] != '[') return out;
    auto ts_end = line.find(']');
    if (ts_end == std::string::npos) return out;
    out.timestamp = line.substr(1, ts_end - 1);
    auto lv_start = line.find('[', ts_end + 1);
    if (lv_start == std::string::npos) return out;
    auto lv_end = line.find(']', lv_start + 1);
    if (lv_end == std::string::npos) return out;
    out.level   = line.substr(lv_start + 1, lv_end - lv_start - 1);
    out.message = (lv_end + 2 < line.size()) ? line.substr(lv_end + 2) : "";
    return out;
}

std::vector<LogLine> Logger::tail(int n, const std::string& level_filter) const {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<LogLine> result;
    if (log_path_.empty()) return result;

    std::ifstream f(log_path_);
    if (!f.is_open()) return result;

    // Read all lines into a ring buffer of size n*4 (to allow filtering)
    std::deque<std::string> ring;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty()) ring.push_back(line);
        if (static_cast<int>(ring.size()) > n * 8) ring.pop_front();
    }
    f.close();

    // Parse and filter from newest (back) to oldest
    for (auto it = ring.rbegin(); it != ring.rend(); ++it) {
        LogLine ll = parse_line(*it);
        if (!level_filter.empty() && ll.level != level_filter) continue;
        result.push_back(ll);
        if (static_cast<int>(result.size()) >= n) break;
    }
    // Return newest-first (already in that order from rbegin)
    return result;
}

} // namespace metis::antique
