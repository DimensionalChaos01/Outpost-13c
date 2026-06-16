#pragma once
#include <string>

// Checks whether a file exists on disk.
bool file_exists(const std::string& path);

// If `path` does not exist, creates parent directories as needed and writes
// `template_content` to the file, then returns true.
// If the file already exists, does nothing and returns true.
// Returns false only if the file could not be created.
//
// Usage pattern for any loader:
//   ensure_file(path, k_my_template);
//   // now open and parse path normally — it is guaranteed to exist
bool ensure_file(const std::string& path, const std::string& template_content);
