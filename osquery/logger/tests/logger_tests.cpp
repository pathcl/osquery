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

#include <osquery/core.h>
#include <osquery/logger.h>

namespace osquery {

class LoggerTests : public testing::Test {
 public:
  void SetUp() {
    // Backup the logging status, then disable.
    logging_status_ = FLAGS_disable_logging;
    FLAGS_disable_logging = false;

    // Setup / initialize static members.
    log_lines.clear();
    status_messages.clear();
    statuses_logged = 0;
    last_status = {O_INFO, "", -1, ""};
  }

  void TearDown() { FLAGS_disable_logging = logging_status_; }

  // Track lines emitted to logString
  static std::vector<std::string> log_lines;

  // Track the results of init
  static StatusLogLine last_status;
  static std::vector<std::string> status_messages;

  // Count calls to logStatus
  static int statuses_logged;
  static int events_logged;
  // Count added and removed snapshot rows
  static int snapshot_rows_added;
  static int snapshot_rows_removed;

 private:
  /// Save the status of logging before running tests, restore afterward.
  bool logging_status_{true};
};

std::vector<std::string> LoggerTests::log_lines;
StatusLogLine LoggerTests::last_status;
std::vector<std::string> LoggerTests::status_messages;
int LoggerTests::statuses_logged = 0;
int LoggerTests::events_logged = 0;
int LoggerTests::snapshot_rows_added = 0;
int LoggerTests::snapshot_rows_removed = 0;

class TestLoggerPlugin : public LoggerPlugin {
 protected:
  bool usesLogStatus() override { return shouldLogStatus; }
  bool usesLogEvent() override { return shouldLogEvent; }

  Status logEvent(const std::string& e) override {
    LoggerTests::events_logged++;
    return Status(0, "OK");
  }

  Status logString(const std::string& s) override {
    LoggerTests::log_lines.push_back(s);
    return Status(0, s);
  }

  void init(const std::string& name,
            const std::vector<StatusLogLine>& log) override {
    for (const auto& status : log) {
      LoggerTests::status_messages.push_back(status.message);
    }

    if (log.size() > 0) {
      LoggerTests::last_status = log.back();
    }
  }

  Status logStatus(const std::vector<StatusLogLine>& log) override {
    ++LoggerTests::statuses_logged;
    return Status(0, "OK");
  }

  Status logSnapshot(const std::string& s) override {
    LoggerTests::snapshot_rows_added += 1;
    LoggerTests::snapshot_rows_removed += 0;
    return Status(0, "OK");
  }

 public:
  /// Allow test methods to change status logging state.
  bool shouldLogStatus{true};

  /// Allow test methods to change event logging state.
  bool shouldLogEvent{true};
};

TEST_F(LoggerTests, test_plugin) {
  Registry::add<TestLoggerPlugin>("logger", "test");
  Registry::setUp();

  auto s = Registry::call("logger", "test", {{"string", "foobar"}});
  EXPECT_TRUE(s.ok());
  EXPECT_EQ(LoggerTests::log_lines.back(), "foobar");
}

TEST_F(LoggerTests, test_logger_init) {
  // Expect the logger to have been registered from the first test.
  EXPECT_TRUE(Registry::exists("logger", "test"));
  EXPECT_TRUE(Registry::setActive("logger", "test").ok());

  initStatusLogger("logger_test");
  // This will be printed to stdout.
  LOG(WARNING) << "Logger test is generating a warning status (1)";
  initLogger("logger_test");

  // The warning message will have been buffered and sent to the active logger
  // which is test.
  EXPECT_EQ(LoggerTests::status_messages.size(), 1U);

  // The logStatus API should NOT have been called. It will only be used if
  // (1) The active logger's init returns success within initLogger and
  // (2) for status logs generated after initLogger is called.
  EXPECT_EQ(LoggerTests::statuses_logged, 0);
}

TEST_F(LoggerTests, test_log_string) {
  // So far, tests have only used the logger registry/plugin API.
  EXPECT_TRUE(logString("{\"json\": true}", "event"));
  ASSERT_EQ(LoggerTests::log_lines.size(), 1U);
  EXPECT_EQ(LoggerTests::log_lines.back(), "{\"json\": true}");

  // Expect the logString method to fail if we explicitly request a logger
  // plugin that has not been added to the registry.
  EXPECT_FALSE(logString("{\"json\": true}", "event", "does_not_exist"));

  // Expect the plugin not to receive logs if status logging is disabled.
  FLAGS_disable_logging = true;
  EXPECT_TRUE(logString("test", "event"));
  EXPECT_EQ(LoggerTests::log_lines.size(), 1U);
  FLAGS_disable_logging = false;

  // If logging is re-enabled, logs should send as usual.
  EXPECT_TRUE(logString("test", "event"));
  EXPECT_EQ(LoggerTests::log_lines.size(), 2U);
}

TEST_F(LoggerTests, test_logger_log_status) {
  // This will be printed to stdout.
  LOG(WARNING) << "Logger test is generating a warning status (2)";

  // The second warning status will be sent to the logger plugin.
  EXPECT_EQ(LoggerTests::statuses_logged, 1);
}

TEST_F(LoggerTests, test_feature_request) {
  // Retrieve the test logger plugin.
  auto plugin = Registry::get("logger", "test");
  auto logger = std::dynamic_pointer_cast<TestLoggerPlugin>(plugin);

  logger->shouldLogEvent = false;
  logger->shouldLogStatus = false;
  auto status = Registry::call("logger", "test", {{"action", "features"}});
  EXPECT_EQ(status.getCode(), 0);

  logger->shouldLogStatus = true;
  status = Registry::call("logger", "test", {{"action", "features"}});
  EXPECT_EQ(status.getCode(), LOGGER_FEATURE_LOGSTATUS);
}

TEST_F(LoggerTests, test_logger_variations) {
  // Retrieve the test logger plugin.
  auto plugin = Registry::get("logger", "test");
  auto logger = std::dynamic_pointer_cast<TestLoggerPlugin>(plugin);
  // Change the behavior.
  logger->shouldLogStatus = false;

  // Call the logger initialization again, then reset the behavior.
  initLogger("duplicate_logger");
  logger->shouldLogStatus = true;

  // This will be printed to stdout.
  LOG(WARNING) << "Logger test is generating a warning status (3)";

  // Since the initLogger call triggered a failed init, meaning the logger
  // does NOT handle Glog logs, there will be no statuses logged.
  EXPECT_EQ(LoggerTests::statuses_logged, 0);
}

TEST_F(LoggerTests, test_logger_snapshots) {
  // A snapshot query should not include removed items.
  QueryLogItem item;
  item.name = "test_query";
  item.identifier = "unknown_test_host";
  item.time = 0;
  item.calendar_time = "no_time";

  // Add a fake set of results.
  item.results.added.push_back({{"test_column", "test_value"}});
  logSnapshotQuery(item);

  // Expect the plugin to optionally handle snapshot logging.
  EXPECT_EQ(LoggerTests::snapshot_rows_added, 1);
}

class SecondTestLoggerPlugin : public LoggerPlugin {
 public:
  Status logString(const std::string& s) override { return Status(0); }

 protected:
  void init(const std::string& binary_name,
            const std::vector<StatusLogLine>& log) override {}
};

TEST_F(LoggerTests, test_multiple_loggers) {
  Registry::add<SecondTestLoggerPlugin>("logger", "second_test");
  EXPECT_TRUE(Registry::setActive("logger", "test,second_test").ok());

  // With two active loggers, the string should be added twice.
  // But the 'test' logger is the only item incrementing the log_lines counter.
  logString("this is a test", "added");
  EXPECT_EQ(LoggerTests::log_lines.size(), 1U);

  LOG(WARNING) << "Logger test is generating a warning status (4)";
  // Refer to the above notes about status logs not emitting until the logger
  // it initialized. We do a 0-test to check for dead locks around attempting
  // to forward Glog-based sinks recursively into our sinks.
  EXPECT_EQ(LoggerTests::statuses_logged, 0);

  // Now try to initialize multiple loggers (1) forwards, (2) does not.
  Registry::setActive("logger", "test,second_test");
  initLogger("logger_test");
  LOG(WARNING) << "Logger test is generating a warning status (5)";
  // Now that the "test" logger is initialized, the status log will be
  // forwarded.
  EXPECT_EQ(LoggerTests::statuses_logged, 1);
}

TEST_F(LoggerTests, test_logger_scheduled_query) {
  QueryLogItem item;
  item.name = "test_query";
  item.identifier = "unknown_test_host";
  item.time = 0;
  item.calendar_time = "no_time";
  item.results.added.push_back({{"test_column", "test_value"}});
  logQueryLogItem(item);
  EXPECT_EQ(LoggerTests::log_lines.size(), 1U);

  item.results.removed.push_back({{"test_column", "test_new_value\n"}});
  logQueryLogItem(item);
  ASSERT_EQ(LoggerTests::log_lines.size(), 3U);

  // Make sure the JSON output does not have a newline.
  std::string expected =
      "{\"name\":\"test_query\",\"hostIdentifier\":\"unknown_test_host\","
      "\"calendarTime\":\"no_time\",\"unixTime\":\"0\",\"columns\":{\"test_"
      "column\":\"test_new_value\\n\"},\"action\":\"removed\"}";
  EXPECT_EQ(LoggerTests::log_lines.back(), expected);
}
}
