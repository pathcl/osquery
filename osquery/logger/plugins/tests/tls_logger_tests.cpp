/*
 *  Copyright (c) 2014-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include <gtest/gtest.h>

#include <osquery/logger.h>
#include <osquery/database.h>

#include "osquery/core/test_util.h"

#include "osquery/logger/plugins/tls.h"

namespace pt = boost::property_tree;

namespace osquery {

class TLSLoggerTests : public testing::Test {
 public:
  void runCheck(const std::shared_ptr<TLSLogForwarder>& runner) {
    runner->check();
  }
};

TEST_F(TLSLoggerTests, test_database) {
  auto forwarder = std::make_shared<TLSLogForwarder>("fake_key");
  std::string expected = "{\"new_json\": true}";
  forwarder->logString(expected);
  StatusLogLine status;
  status.message = "{\"status\": \"bar\"}";
  forwarder->logStatus({status});

  std::vector<std::string> indexes;
  scanDatabaseKeys(kLogs, indexes);
  EXPECT_EQ(2U, indexes.size());

  // Iterate using an unordered search, and search for the expected string
  // that was just logged.
  bool found_string = false;
  for (const auto& index : indexes) {
    std::string value;
    getDatabaseValue(kLogs, index, value);
    found_string = (found_string || value == expected);
  }
  EXPECT_TRUE(found_string);
}

TEST_F(TLSLoggerTests, test_send) {
  // Start a server.
  TLSServerRunner::start();
  TLSServerRunner::setClientConfig();

  auto forwarder = std::make_shared<TLSLogForwarder>("fake_key");
  for (size_t i = 0; i < 20; i++) {
    std::string expected = "{\"more_json\": true}";
    forwarder->logString(expected);
  }

  runCheck(forwarder);

  // Stop the server.
  TLSServerRunner::unsetClientConfig();
  TLSServerRunner::stop();
}
}
