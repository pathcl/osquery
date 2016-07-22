/*
 *  Copyright (c) 2014-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include <vector>

#include <boost/filesystem/operations.hpp>

#include <osquery/config.h>
#include <osquery/filesystem.h>
#include <osquery/flags.h>
#include <osquery/logger.h>

namespace fs = boost::filesystem;
namespace errc = boost::system::errc;

namespace osquery {

CLI_FLAG(string,
         config_path,
         OSQUERY_HOME "/osquery.conf",
         "Path to JSON config file");

class FilesystemConfigPlugin : public ConfigPlugin {
 public:
  Status genConfig(std::map<std::string, std::string>& config);
  Status genPack(const std::string& name,
                 const std::string& value,
                 std::string& pack);
};

REGISTER(FilesystemConfigPlugin, "config", "filesystem");

Status FilesystemConfigPlugin::genConfig(
    std::map<std::string, std::string>& config) {
  boost::system::error_code ec;
  if (!fs::is_regular_file(FLAGS_config_path, ec) ||
      ec.value() != errc::success) {
    return Status(1, "config file does not exist: " + FLAGS_config_path);
  }

  std::vector<std::string> conf_files;
  resolveFilePattern(FLAGS_config_path + ".d/%.conf", conf_files);
  std::sort(conf_files.begin(), conf_files.end());
  conf_files.push_back(FLAGS_config_path);

  for (const auto& path : conf_files) {
    std::string content;
    if (readFile(path, content).ok()) {
      config[path] = content;
    }
  }

  return Status(0, "OK");
}

Status FilesystemConfigPlugin::genPack(const std::string& name,
                                       const std::string& value,
                                       std::string& pack) {
  boost::system::error_code ec;
  if (!fs::is_regular_file(value, ec) || ec.value() != errc::success) {
    return Status(1, value + " is not a valid path");
  }
  return readFile(value, pack);
}
}
