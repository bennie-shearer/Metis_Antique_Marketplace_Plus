#pragma once
#include "types.hpp"
#include <string>
#include <mutex>

struct sqlite3;

namespace metis::antique {

class Db {
public:
    static Db& instance();
    bool open(const std::string& path);
    void create_schema();
    bool exec(const std::string& sql, const std::vector<std::string>& params = {});
    Rows query(const std::string& sql, const std::vector<std::string>& params = {});
    std::string last_insert_id();
    bool backup(const std::string& dest_path);
private:
    Db() = default;
    void run_migration(const std::string& sql);
    sqlite3*   db_ = nullptr;
    std::mutex mu_;
};

} // namespace metis::antique
