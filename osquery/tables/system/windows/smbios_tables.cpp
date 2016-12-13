/*
 *  Copyright (c) 2014-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include <osquery/logger.h>
#include <osquery/tables.h>

#include "osquery/core/windows/wmi.h"

namespace osquery {
namespace tables {

QueryData genPlatformInfo(QueryContext& context) {
  QueryData results;

  std::string query =
      "select Manufacturer, SMBIOSBIOSVersion, ReleaseDate, "
      "SystemBiosMajorVersion, SystemBiosMinorVersion from Win32_BIOS";
  WmiRequest request(query);
  if (!request.getStatus().ok()) {
    return results;
  }
  std::vector<WmiResultItem>& wmiResults = request.results();
  if (wmiResults.size() != 1) {
    return results;
  }
  Row r;
  std::string sPlaceholder;
  wmiResults[0].GetString("Manufacturer", r["vendor"]);
  wmiResults[0].GetString("SMBIOSBIOSVersion", r["version"]);
  unsigned char majorRevision = 0x0;
  wmiResults[0].GetUChar("SystemBiosMajorVersion", majorRevision);
  unsigned char minorRevision = 0x0;
  wmiResults[0].GetUChar("SystemBiosMinorVersion", minorRevision);
  r["revision"] =
      std::to_string(majorRevision) + "." + std::to_string(minorRevision);

  results.push_back(r);
  return results;
}
}
}
