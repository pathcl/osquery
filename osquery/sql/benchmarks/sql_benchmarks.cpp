/*
 *  Copyright (c) 2014-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree. An additional grant
 *  of patent rights can be found in the PATENTS file in the same directory.
 *
 */

#include <benchmark/benchmark.h>

#include <osquery/core.h>
#include <osquery/registry.h>
#include <osquery/sql.h>
#include <osquery/tables.h>

#include "osquery/sql/virtual_table.h"

namespace osquery {

class BenchmarkTablePlugin : public TablePlugin {
 private:
  TableColumns columns() const {
    return {
      std::make_tuple("test_int", INTEGER_TYPE, DEFAULT),
      std::make_tuple("test_text", TEXT_TYPE, DEFAULT),
    };
  }

  QueryData generate(QueryContext& ctx) {
    QueryData results;
    results.push_back({{"test_int", "0"}});
    results.push_back({{"test_int", "0"}, {"test_text", "hello"}});
    return results;
  }
};

static void SQL_virtual_table_registry(benchmark::State& state) {
  // Add a sample virtual table plugin.
  // Profile calling the plugin's column data.
  Registry::add<BenchmarkTablePlugin>("table", "benchmark");
  while (state.KeepRunning()) {
    PluginResponse res;
    Registry::call("table", "benchmark", {{"action", "generate"}}, res);
  }
}

BENCHMARK(SQL_virtual_table_registry);

static void SQL_virtual_table_internal(benchmark::State& state) {
  Registry::add<BenchmarkTablePlugin>("table", "benchmark");
  PluginResponse res;
  Registry::call("table", "benchmark", {{"action", "columns"}}, res);

  // Attach a sample virtual table.
  auto dbc = SQLiteDBManager::get();
  attachTableInternal("benchmark", columnDefinition(res), dbc);

  while (state.KeepRunning()) {
    QueryData results;
    queryInternal("select * from benchmark", results, dbc->db());
  }
}

BENCHMARK(SQL_virtual_table_internal);

static void SQL_virtual_table_internal_global(benchmark::State& state) {
  Registry::add<BenchmarkTablePlugin>("table", "benchmark");
  PluginResponse res;
  Registry::call("table", "benchmark", {{"action", "columns"}}, res);

  while (state.KeepRunning()) {
    // Get a connection to the persistent database.
    auto dbc = SQLiteDBManager::get();
    attachTableInternal("benchmark", columnDefinition(res), dbc);

    QueryData results;
    queryInternal("select * from benchmark", results, dbc->db());
  }
}

BENCHMARK(SQL_virtual_table_internal_global);

static void SQL_virtual_table_internal_unique(benchmark::State& state) {
  Registry::add<BenchmarkTablePlugin>("table", "benchmark");
  PluginResponse res;
  Registry::call("table", "benchmark", {{"action", "columns"}}, res);

  while (state.KeepRunning()) {
    // Get a new database connection (to a unique database).
    auto dbc = SQLiteDBManager::getUnique();
    attachTableInternal("benchmark", columnDefinition(res), dbc);

    QueryData results;
    queryInternal("select * from benchmark", results, dbc->db());
  }
}

BENCHMARK(SQL_virtual_table_internal_unique);

class BenchmarkLongTablePlugin : public TablePlugin {
 private:
  TableColumns columns() const {
    return {
      std::make_tuple("test_int", INTEGER_TYPE, DEFAULT),
      std::make_tuple("test_text", TEXT_TYPE, DEFAULT),
    };
  }

  QueryData generate(QueryContext& ctx) {
    QueryData results;
    for (int i = 0; i < 1000; i++) {
      results.push_back({{"test_int", "0"}, {"test_text", "hello"}});
    }
    return results;
  }
};

static void SQL_virtual_table_internal_long(benchmark::State& state) {
  Registry::add<BenchmarkLongTablePlugin>("table", "long_benchmark");
  PluginResponse res;
  Registry::call("table", "long_benchmark", {{"action", "columns"}}, res);

  // Attach a sample virtual table.
  auto dbc = SQLiteDBManager::getUnique();
  attachTableInternal("long_benchmark", columnDefinition(res), dbc);

  while (state.KeepRunning()) {
    QueryData results;
    queryInternal("select * from long_benchmark", results, dbc->db());
  }
}

BENCHMARK(SQL_virtual_table_internal_long);

class BenchmarkWideTablePlugin : public TablePlugin {
 private:
  TableColumns columns() const override {
    TableColumns cols;
    for (int i = 0; i < 20; i++) {
      cols.push_back(
        std::make_tuple("test_" + std::to_string(i), INTEGER_TYPE, DEFAULT));
    }
    return cols;
  }

  QueryData generate(QueryContext& ctx) override {
    QueryData results;
    for (int k = 0; k < 50; k++) {
      Row r;
      for (int i = 0; i < 20; i++) {
        r["test_" + std::to_string(i)] = "0";
      }
      results.push_back(r);
    }
    return results;
  }
};

static void SQL_virtual_table_internal_wide(benchmark::State& state) {
  Registry::add<BenchmarkWideTablePlugin>("table", "wide_benchmark");
  PluginResponse res;
  Registry::call("table", "wide_benchmark", {{"action", "columns"}}, res);

  // Attach a sample virtual table.
  auto dbc = SQLiteDBManager::getUnique();
  attachTableInternal("wide_benchmark", columnDefinition(res), dbc);

  while (state.KeepRunning()) {
    QueryData results;
    queryInternal("select * from wide_benchmark", results, dbc->db());
  }
}

BENCHMARK(SQL_virtual_table_internal_wide);

static void SQL_select_metadata(benchmark::State& state) {
  auto dbc = SQLiteDBManager::get();
  while (state.KeepRunning()) {
    QueryData results;
    queryInternal("select count(*) from sqlite_temp_master;", results,
                  dbc->db());
  }
}

BENCHMARK(SQL_select_metadata);

static void SQL_select_basic(benchmark::State& state) {
  // Profile executing a query against an internal, already attached table.
  while (state.KeepRunning()) {
    auto results = SQLInternal("select * from benchmark");
  }
}

BENCHMARK(SQL_select_basic);
}
