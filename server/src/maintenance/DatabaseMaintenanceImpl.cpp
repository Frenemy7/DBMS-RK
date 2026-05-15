#include "DatabaseMaintenanceImpl.h"

#include <filesystem>

namespace Maintenance {

    bool DatabaseMaintenanceImpl::backupDatabase(const std::string& dbName, const std::string& path) {
        namespace fs = std::filesystem;
        const fs::path source = fs::path("data") / dbName;
        const fs::path target = fs::path(path);

        if (!fs::exists(source) || !fs::is_directory(source)) return false;

        std::error_code ec;
        if (fs::exists(target, ec)) fs::remove_all(target, ec);
        fs::create_directories(target.parent_path(), ec);
        fs::copy(source, target, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        return !ec;
    }

    bool DatabaseMaintenanceImpl::restoreDatabase(const std::string& dbName, const std::string& path) {
        namespace fs = std::filesystem;
        const fs::path source = fs::path(path);
        const fs::path target = fs::path("data") / dbName;

        if (!fs::exists(source) || !fs::is_directory(source)) return false;

        std::error_code ec;
        if (fs::exists(target, ec)) fs::remove_all(target, ec);
        fs::copy(source, target, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
        return !ec;
    }

    bool DatabaseMaintenanceImpl::backupExists(const std::string& path) {
        namespace fs = std::filesystem;
        return fs::exists(path) && fs::is_directory(path);
    }

}
