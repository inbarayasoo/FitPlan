#include "db/MigrationSet.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace fitplan::db {
namespace fs = std::filesystem;

namespace {

// "012_add_x.sql" -> 12. Returns -1 when the name does not start with a digit.
int leading_number(const std::string& filename) {
    std::size_t end = 0;
    while (end < filename.size() &&
           std::isdigit(static_cast<unsigned char>(filename[end])) != 0) {
        ++end;
    }
    if (end == 0) {
        return -1;
    }
    return std::stoi(filename.substr(0, end));
}

std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open migration file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

}  // namespace

std::vector<MigrationFile> load_migration_files(const std::string& dir) {
    if (!fs::is_directory(dir)) {
        throw std::runtime_error("migrations directory not found: " + dir);
    }

    std::vector<MigrationFile> files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".sql") {
            continue;
        }
        const std::string name = entry.path().filename().string();
        const int version = leading_number(name);
        if (version < 0) {
            continue;  // not a "<number>_....sql" file
        }
        files.push_back({version, name, read_file(entry.path())});
    }

    std::sort(files.begin(), files.end(),
              [](const MigrationFile& a, const MigrationFile& b) {
                  return a.version < b.version;
              });

    for (std::size_t i = 1; i < files.size(); ++i) {
        if (files[i].version == files[i - 1].version) {
            throw std::runtime_error("two migrations share version " +
                                     std::to_string(files[i].version));
        }
    }
    return files;
}

}  // namespace fitplan::db
