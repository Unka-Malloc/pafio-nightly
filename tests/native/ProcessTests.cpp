#include "SpioCore/Process.hpp"

#include <csignal>
#include <chrono>
#include <string>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST(ProcessTests, CapturesStdoutAndStderrWithoutDeadlocking)
{
  const spio::ProcessResult result = spio::RunProcess({
      .program = "/bin/sh",
      .args = {
          "-c",
          "i=0; while [ \"$i\" -lt 32768 ]; do printf x >&2; i=$((i + 1)); done; printf ok",
      },
      .search_path = false,
      .timeout = 5s,
      .max_stderr_bytes = 1U << 20,
      .error_context = "process test",
  });

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_FALSE(result.timed_out);
  EXPECT_EQ(result.stdout_text, "ok");
  EXPECT_EQ(result.stderr_text.size(), 32768U);
}

TEST(ProcessTests, TimesOutAndTerminatesProcessGroup)
{
  const spio::ProcessResult result = spio::RunProcess({
      .program = "/bin/sh",
      .args = {"-c", "sleep 2"},
      .search_path = false,
      .timeout = 100ms,
      .error_context = "timeout test",
  });

  EXPECT_TRUE(result.timed_out);
  EXPECT_TRUE(result.terminated_by_signal);
  EXPECT_EQ(result.signal_number, SIGKILL);
}

TEST(ProcessTests, TracksSignalTermination)
{
  const spio::ProcessResult result = spio::RunProcess({
      .program = "/bin/sh",
      .args = {"-c", "kill -TERM $$"},
      .search_path = false,
      .timeout = 5s,
      .error_context = "signal test",
  });

  EXPECT_TRUE(result.terminated_by_signal);
  EXPECT_EQ(result.signal_number, SIGTERM);
  EXPECT_EQ(result.exit_code, 128 + SIGTERM);
}

TEST(ProcessTests, MarksTruncatedOutput)
{
  const spio::ProcessResult result = spio::RunProcess({
      .program = "/bin/sh",
      .args = {
          "-c",
          "i=0; while [ \"$i\" -lt 8192 ]; do printf y; printf z >&2; i=$((i + 1)); done",
      },
      .search_path = false,
      .timeout = 5s,
      .max_stdout_bytes = 1024,
      .max_stderr_bytes = 1024,
      .error_context = "truncate test",
  });

  EXPECT_EQ(result.exit_code, 0);
  EXPECT_TRUE(result.stdout_truncated);
  EXPECT_TRUE(result.stderr_truncated);
  EXPECT_EQ(result.stdout_text.size(), 1024U);
  EXPECT_EQ(result.stderr_text.size(), 1024U);
}
