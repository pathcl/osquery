/*
 *  Copyright (c) 2014-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include <sys/stat.h>

#include <boost/filesystem.hpp>

#include <osquery/filesystem.h>
#include <osquery/logger.h>
#include <osquery/tables.h>

namespace fs = boost::filesystem;

namespace osquery {
namespace tables {

const std::map<fs::file_type, std::string> kTypeNames{
    {fs::regular_file, "regular"},
    {fs::directory_file, "directory"},
    {fs::symlink_file, "symlink"},
    {fs::block_file, "block"},
    {fs::character_file, "character"},
    {fs::fifo_file, "fifo"},
    {fs::socket_file, "socket"},
    {fs::type_unknown, "unknown"},
    {fs::status_error, "error"},
};

void genFileInfo(const fs::path& path,
                 const fs::path& parent,
                 const std::string& pattern,
                 QueryData& results) {
  // Must provide the path, filename, directory separate from boost path->string
  // helpers to match any explicit (query-parsed) predicate constraints.
  struct stat file_stat, link_stat;
  if (lstat(path.string().c_str(), &link_stat) < 0 ||
      stat(path.string().c_str(), &file_stat)) {
    // Path was not real, had too may links, or could not be accessed.
    return;
  }

  Row r;
  r["path"] = path.string();
  r["filename"] = path.filename().string();
  r["directory"] = parent.string();

  r["inode"] = BIGINT(file_stat.st_ino);
  r["uid"] = BIGINT(file_stat.st_uid);
  r["gid"] = BIGINT(file_stat.st_gid);
  r["mode"] = lsperms(file_stat.st_mode);
  r["device"] = BIGINT(file_stat.st_rdev);
  r["size"] = BIGINT(file_stat.st_size);
  r["block_size"] = INTEGER(file_stat.st_blksize);
  r["hard_links"] = INTEGER(file_stat.st_nlink);

  // Times
  r["atime"] = BIGINT(file_stat.st_atime);
  r["mtime"] = BIGINT(file_stat.st_mtime);
  r["ctime"] = BIGINT(file_stat.st_ctime);
#if defined(__linux__)
  // No 'birth' or create time in Linux.
  r["btime"] = "0";
#else
  r["btime"] = BIGINT(file_stat.st_birthtimespec.tv_sec);
#endif

  // Type booleans
  boost::system::error_code ec;
  auto status = fs::status(path, ec);
  if (kTypeNames.count(status.type())) {
    r["type"] = kTypeNames.at(status.type());
  } else {
    r["type"] = "unknown";
  }

  r["is_file"] = (!S_ISDIR(file_stat.st_mode)) ? "1" : "0";
  r["is_dir"] = (S_ISDIR(file_stat.st_mode)) ? "1" : "0";
  r["is_link"] = (S_ISLNK(link_stat.st_mode)) ? "1" : "0";
  r["is_char"] = (S_ISCHR(file_stat.st_mode)) ? "1" : "0";
  r["is_block"] = (S_ISBLK(file_stat.st_mode)) ? "1" : "0";
  results.push_back(r);
}

QueryData genFile(QueryContext& context) {
  QueryData results;

  // Resolve file paths for EQUALS and LIKE operations.
  auto paths = context.constraints["path"].getAll(EQUALS);
  context.expandConstraints(
      "path",
      LIKE,
      paths,
      ([&](const std::string& pattern, std::set<std::string>& out) {
        std::vector<std::string> patterns;
        auto status =
            resolveFilePattern(pattern, patterns, GLOB_ALL | GLOB_NO_CANON);
        if (status.ok()) {
          for (const auto& resolved : patterns) {
            out.insert(resolved);
          }
        }
        return status;
      }));

  // Iterate through each of the resolved/supplied paths.
  for (const auto& path_string : paths) {
    fs::path path = path_string;
    genFileInfo(path, path.parent_path(), "", results);
  }

  // Resolve directories for EQUALS and LIKE operations.
  auto directories = context.constraints["directory"].getAll(EQUALS);
  context.expandConstraints(
      "directory",
      LIKE,
      directories,
      ([&](const std::string& pattern, std::set<std::string>& out) {
        std::vector<std::string> patterns;
        auto status =
            resolveFilePattern(pattern, patterns, GLOB_FOLDERS | GLOB_NO_CANON);
        if (status.ok()) {
          for (const auto& resolved : patterns) {
            out.insert(resolved);
          }
        }
        return status;
      }));

  // Now loop through constraints using the directory column constraint.
  for (const auto& directory_string : directories) {
    if (!isReadable(directory_string) || !isDirectory(directory_string)) {
      continue;
    }

    try {
      // Iterate over the directory and generate info for each regular file.
      fs::directory_iterator begin(directory_string), end;
      for (; begin != end; ++begin) {
        genFileInfo(begin->path(), directory_string, "", results);
      }
    } catch (const fs::filesystem_error& e) {
      continue;
    }
  }

  return results;
}
}
}
