#include "FileUtils.h"
#include <filesystem>
#include <fstream>
#include <iostream>

bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

bool ensure_file(const std::string& path, const std::string& template_content) {
    if (file_exists(path))
        return true;

    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(p.parent_path(), ec);
        if (ec) {
            std::cerr << "ensure_file: cannot create directories for "
                      << path << ": " << ec.message() << "\n";
            return false;
        }
    }

    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "ensure_file: cannot write " << path << "\n";
        return false;
    }
    out << template_content;
    std::cout << "ensure_file: created template -> " << path << "\n";
    return true;
}
