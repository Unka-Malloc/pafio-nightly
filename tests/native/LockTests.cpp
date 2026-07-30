#include <gtest/gtest.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "PafioCLI/CLI.hpp"
#include "PafioCore/Errors.hpp"
#include "PafioManifest/Lockfile.hpp"
#include "PafioResolve/Resolver.hpp"

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{

struct ChildProcessResult
{
  int exit_code = 0;
  std::string stdout_text;
  std::string stderr_text;
};

class ScopedEnvVar
{
public:
  ScopedEnvVar(const std::string &name, const std::string &value) :
      name_(name) {
    if (const char *existing = std::getenv(name.c_str()); existing != nullptr) {
      had_previous_ = true;
      previous_value_ = existing;
    }
    setenv(name.c_str(), value.c_str(), 1);
  }

  ~ScopedEnvVar() {
    if (had_previous_) {
      setenv(name_.c_str(), previous_value_.c_str(), 1);
    }
    else {
      unsetenv(name_.c_str());
    }
  }

private:
  std::string name_;
  bool had_previous_ = false;
  std::string previous_value_;
};

ChildProcessResult
RunChildProcess(const std::string &binary, const std::vector<std::string> &args) {
  int stdout_pipe[2];
  int stderr_pipe[2];
  if (pipe(stdout_pipe) != 0 || pipe(stderr_pipe) != 0) {
    throw std::runtime_error("failed to create test pipes");
  }

  const pid_t child = fork();
  if (child < 0) {
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);
    throw std::runtime_error("failed to fork test process");
  }

  if (child == 0) {
    dup2(stdout_pipe[1], STDOUT_FILENO);
    dup2(stderr_pipe[1], STDERR_FILENO);
    close(stdout_pipe[0]);
    close(stdout_pipe[1]);
    close(stderr_pipe[0]);
    close(stderr_pipe[1]);

    std::vector<char *> argv;
    argv.reserve(args.size() + 2U);
    argv.push_back(const_cast<char *>(binary.c_str()));
    for (const std::string &arg : args) {
      argv.push_back(const_cast<char *>(arg.c_str()));
    }
    argv.push_back(nullptr);

    execvp(binary.c_str(), argv.data());
    _exit(127);
  }

  close(stdout_pipe[1]);
  close(stderr_pipe[1]);

  auto read_all = [](int fd)
  {
    std::string text;
    std::array<char, 4096> buffer{};
    ssize_t read_size = 0;
    while ((read_size = read(fd, buffer.data(), buffer.size())) > 0) {
      text.append(buffer.data(), static_cast<size_t>(read_size));
    }
    close(fd);
    return text;
  };

  ChildProcessResult result;
  result.stdout_text = read_all(stdout_pipe[0]);
  result.stderr_text = read_all(stderr_pipe[0]);

  int status = 0;
  waitpid(child, &status, 0);
  if (WIFEXITED(status)) {
    result.exit_code = WEXITSTATUS(status);
  }
  else {
    result.exit_code = 1;
  }

  return result;
}

std::string
TrimTrailingNewline(std::string text) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
    text.pop_back();
  }
  return text;
}

fs::path
CanonicalAbsolutePath(const fs::path &path) {
  return fs::absolute(path).lexically_normal();
}

fs::path
MakeTempDir(const std::string &label) {
  const fs::path root = fs::temp_directory_path() / "pafio-native-lock-tests" / label;
  fs::remove_all(root);
  fs::create_directories(root);
  return root;
}

void
WriteFile(const fs::path &path, const std::string &content) {
  fs::create_directories(path.parent_path());
  std::ofstream out(path);
  ASSERT_TRUE(out.good());
  out << content;
  ASSERT_TRUE(out.good());
}

std::string
ReadFile(const fs::path &path) {
  std::ifstream in(path);
  std::ostringstream buffer;
  buffer << in.rdbuf();
  std::string text = buffer.str();
  text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
  return text;
}

std::string
FileUrl(const fs::path &path) {
  return "file://" + CanonicalAbsolutePath(path).generic_string();
}

void
RunGitOrAssert(const std::vector<std::string> &args) {
  const ChildProcessResult result = RunChildProcess("git", args);
  ASSERT_EQ(result.exit_code, 0) << TrimTrailingNewline(result.stderr_text.empty() ? result.stdout_text : result.stderr_text);
}

void
PublishIntoFilesystemRegistryOrAssert(const fs::path &manifest_path, const fs::path &registry_root) {
  ASSERT_EQ(
    pafio::RunCli({
      "publish",
      "--manifest-path",
      manifest_path.string(),
      "--registry",
      registry_root.string(),
    }),
    pafio::kExitSuccess
  );
}

std::optional<std::string>
TryResolveLockfileMessage(const fs::path &manifest_path) {
  try {
    (void)pafio::ResolveSingleVersionLockfile(manifest_path);
    return std::nullopt;
  }
  catch (const pafio::ResolutionError &error) {
    return std::string(error.what());
  }
}

std::string
GitHeadRev(const fs::path &repo_root) {
  const ChildProcessResult result = RunChildProcess("git", {"-C", repo_root.string(), "rev-parse", "HEAD"});
  EXPECT_EQ(result.exit_code, 0) << TrimTrailingNewline(result.stderr_text.empty() ? result.stdout_text : result.stderr_text);
  return TrimTrailingNewline(result.stdout_text);
}

std::string
CreateWorkspaceGitRepo(const fs::path &repo_root, const std::string &util_version) {
  fs::create_directories(repo_root);
  RunGitOrAssert({"init", "--initial-branch=main", repo_root.string()});

  WriteFile(
    repo_root / "pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[workspace]\n"
    "members = [\"packages/feed\", \"packages/util\"]\n"
    "resolver = \"1\"\n"
  );
  WriteFile(
    repo_root / "packages/feed/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/feed\"\n"
    "version = \"1.2.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n\n"
    "[dependencies]\n"
    "util = { package = \"acme/util\", path = \"../util\" }\n"
  );
  WriteFile(
    repo_root / "packages/util/pafio.toml",
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/util\"\n"
      "version = \""
    ) + util_version
      +
      "\"\n"
      "edition = \"2026\"\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n"
  );

  RunGitOrAssert(
    {"-C", repo_root.string(), "-c", "user.email=pafio-tests@example.com", "-c", "user.name=pafio-tests", "add", "."}
  );
  RunGitOrAssert(
    {"-C", repo_root.string(), "-c", "user.email=pafio-tests@example.com", "-c", "user.name=pafio-tests", "commit", "--quiet", "-m", "initial"}
  );
  return GitHeadRev(repo_root);
}

}  // namespace

TEST(LockGenerationTests, GeneratesLockfileForSinglePackage) {
  const fs::path root = MakeTempDir("single-package");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  WriteFile(
    root / "pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n"
  );

  const auto generated = pafio::ResolveSingleVersionLockfile(root / "pafio.toml");

  EXPECT_EQ(generated.lockfile.generated_by, "pafio 0.1.0-dev");
  EXPECT_EQ(generated.lockfile.resolver, "single-version-v1");
  ASSERT_EQ(generated.lockfile.packages.size(), 1U);
  EXPECT_EQ(generated.lockfile.packages[0].id, "workspace:acme/app@0.1.0");
  EXPECT_EQ(generated.lockfile.packages[0].source_kind, "workspace");
  EXPECT_TRUE(generated.lockfile.packages[0].dependencies.empty());
}

TEST(LockGenerationTests, GeneratesWorkspaceAndPathGraph) {
  const fs::path root = MakeTempDir("workspace-and-path");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  WriteFile(
    root / "pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[workspace]\n"
    "members = [\"packages/app\", \"packages/util\"]\n"
    "resolver = \"1\"\n"
  );
  WriteFile(
    root / "packages/app/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.5.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n\n"
    "[dependencies]\n"
    "core = { package = \"acme/core\", path = \"../../vendor/core\" }\n\n"
    "[dev-dependencies]\n"
    "util = { package = \"acme/util\", path = \"../util\" }\n"
  );
  WriteFile(
    root / "packages/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.2.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );
  WriteFile(
    root / "vendor/core/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/core\"\n"
    "version = \"0.3.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );

  const auto generated = pafio::ResolveSingleVersionLockfile(root / "pafio.toml");

  ASSERT_EQ(generated.lockfile.packages.size(), 3U);
  EXPECT_EQ(
    pafio::SerializeLockfileCanonical(generated.lockfile),
    std::string(
      "lock-version = 1\n\n"
      "[metadata]\n"
      "generated-by = \"pafio 0.1.0-dev\"\n"
      "resolver = \"single-version-v1\"\n\n"
      "[[package]]\n"
      "id = \"path:acme/core@0.3.0\"\n"
      "name = \"acme/core\"\n"
      "version = \"0.3.0\"\n"
      "source-kind = \"path\"\n"
      "dependencies = []\n\n"
      "[[package]]\n"
      "id = \"workspace:acme/app@0.5.0\"\n"
      "name = \"acme/app\"\n"
      "version = \"0.5.0\"\n"
      "source-kind = \"workspace\"\n"
      "dependencies = [\"path:acme/core@0.3.0\", \"workspace:acme/util@0.2.0\"]\n\n"
      "[[package]]\n"
      "id = \"workspace:acme/util@0.2.0\"\n"
      "name = \"acme/util\"\n"
      "version = \"0.2.0\"\n"
      "source-kind = \"workspace\"\n"
      "dependencies = []\n"
    )
  );
}

TEST(ResolverTests, ResolvesPinnedGitWorkspaceAndTransitivePathDependencies) {
  const fs::path root = MakeTempDir("git-workspace");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path git_repo = root / "remote-feed";
  const std::string rev = CreateWorkspaceGitRepo(git_repo, "0.9.0");

  WriteFile(
    root / "pafio.toml",
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "feed = { package = \"acme/feed\", git = \""
    ) + CanonicalAbsolutePath(git_repo).generic_string()
      + "\", rev = \"" + rev + "\" }\n"
  );

  const auto generated = pafio::ResolveSingleVersionLockfile(root / "pafio.toml");

  ASSERT_EQ(generated.lockfile.packages.size(), 3U);
  const std::string expected =
    std::string(
      "lock-version = 1\n\n"
      "[metadata]\n"
      "generated-by = \"pafio 0.1.0-dev\"\n"
      "resolver = \"single-version-v1\"\n\n"
      "[[package]]\n"
      "id = \"git:acme/feed@1.2.0#"
    )
    + rev +
    "\"\n"
    "name = \"acme/feed\"\n"
    "version = \"1.2.0\"\n"
    "source-kind = \"git\"\n"
    "git = \""
    + CanonicalAbsolutePath(git_repo).generic_string() +
    "\"\n"
    "rev = \""
    + rev +
    "\"\n"
    "dependencies = [\"git:acme/util@0.9.0#"
    + rev +
    "\"]\n\n"
    "[[package]]\n"
    "id = \"git:acme/util@0.9.0#"
    + rev +
    "\"\n"
    "name = \"acme/util\"\n"
    "version = \"0.9.0\"\n"
    "source-kind = \"git\"\n"
    "git = \""
    + CanonicalAbsolutePath(git_repo).generic_string() +
    "\"\n"
    "rev = \""
    + rev +
    "\"\n"
    "dependencies = []\n\n"
    "[[package]]\n"
    "id = \"workspace:acme/app@0.1.0\"\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "source-kind = \"workspace\"\n"
    "dependencies = [\"git:acme/feed@1.2.0#"
    + rev + "\"]\n";
  EXPECT_EQ(pafio::SerializeLockfileCanonical(generated.lockfile), expected);
}

TEST(ResolverTests, ResolvesLargeGitSnapshotsWithoutArchiveTruncation) {
  const fs::path root = MakeTempDir("git-workspace-large-snapshot");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path git_repo = root / "remote-feed";
  (void)CreateWorkspaceGitRepo(git_repo, "0.9.0");

  WriteFile(git_repo / "packages/feed/assets/large.txt", std::string(1U << 21, 'x'));
  RunGitOrAssert(
    {"-C", git_repo.string(), "-c", "user.email=pafio-tests@example.com", "-c", "user.name=pafio-tests", "add", "."}
  );
  RunGitOrAssert(
    {"-C", git_repo.string(), "-c", "user.email=pafio-tests@example.com", "-c", "user.name=pafio-tests", "commit", "--quiet", "-m", "large snapshot"}
  );
  const std::string rev = GitHeadRev(git_repo);

  WriteFile(
    root / "pafio.toml",
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "feed = { package = \"acme/feed\", git = \""
    ) + CanonicalAbsolutePath(git_repo).generic_string()
      + "\", rev = \"" + rev + "\" }\n"
  );

  const auto generated = pafio::ResolveSingleVersionLockfile(root / "pafio.toml");
  ASSERT_EQ(generated.lockfile.packages.size(), 3U);
  EXPECT_EQ(generated.lockfile.packages.back().source_kind, "workspace");
}

TEST(ResolverTests, ReportsFullCyclePathOnLockResolution) {
  const fs::path root = MakeTempDir("lock-cycle-path");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n\n"
    "[dependencies]\n"
    "util = { package = \"acme/util\", path = \"vendor/util\" }\n"
  );
  WriteFile(
    root / "vendor/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.2.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n\n"
    "[dependencies]\n"
    "app = { package = \"acme/app\", path = \"../..\" }\n"
  );

  const std::optional<std::string> message = TryResolveLockfileMessage(manifest_path);
  ASSERT_TRUE(message.has_value());
  EXPECT_NE(message->find("acyclic dependency graph"), std::string::npos);
  EXPECT_NE(message->find("workspace:acme/app@0.1.0"), std::string::npos);
  EXPECT_NE(message->find("path:acme/util@0.2.0"), std::string::npos);
}

TEST(ResolverTests, ReportsBothChainsOnDiamondVersionConflict) {
  const fs::path root = MakeTempDir("diamond-version-conflict");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());

  WriteFile(
    root / "pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n\n"
    "[dependencies]\n"
    "left = { package = \"acme/left\", path = \"vendor/left\" }\n"
    "right = { package = \"acme/right\", path = \"vendor/right\" }\n"
  );
  WriteFile(
    root / "vendor/left/pafio.toml",
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/left\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n\n"
      "[dependencies]\n"
      "util = { package = \"acme/util\", path = \"../util-a\" }\n"
    )
  );
  WriteFile(
    root / "vendor/right/pafio.toml",
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/right\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n\n"
      "[dependencies]\n"
      "util = { package = \"acme/util\", path = \"../util-b\" }\n"
    )
  );
  WriteFile(
    root / "vendor/util-a/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );
  WriteFile(
    root / "vendor/util-b/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.2.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );

  const std::optional<std::string> message = TryResolveLockfileMessage(root / "pafio.toml");
  ASSERT_TRUE(message.has_value());
  EXPECT_NE(message->find("single-version-v1 conflict"), std::string::npos);
  EXPECT_NE(message->find("required by:"), std::string::npos);
  EXPECT_NE(message->find("0.1.0"), std::string::npos);
  EXPECT_NE(message->find("0.2.0"), std::string::npos);
  const size_t first_chain = message->find("required by:");
  ASSERT_NE(first_chain, std::string::npos);
  const size_t second_chain = message->find("required by:", first_chain + 1U);
  EXPECT_NE(second_chain, std::string::npos);
}

TEST(ResolverTests, ProducesDeterministicLockfileAcrossRepeatedResolution) {
  const fs::path root = MakeTempDir("golden-lock-repeat");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  WriteFile(
    root / "pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[workspace]\n"
    "members = [\"packages/app\", \"packages/util\"]\n"
    "resolver = \"1\"\n"
  );
  WriteFile(
    root / "packages/app/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.5.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n\n"
    "[dependencies]\n"
    "core = { package = \"acme/core\", path = \"../../vendor/core\" }\n\n"
    "[dev-dependencies]\n"
    "util = { package = \"acme/util\", path = \"../util\" }\n"
  );
  WriteFile(
    root / "packages/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.2.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );
  WriteFile(
    root / "vendor/core/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/core\"\n"
    "version = \"0.3.1\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );

  const auto first = pafio::ResolveSingleVersionLockfile(root / "pafio.toml");
  const auto second = pafio::ResolveSingleVersionLockfile(root / "pafio.toml");

  ASSERT_EQ(first.lockfile.packages.size(), second.lockfile.packages.size());
  for (size_t i = 0; i < first.lockfile.packages.size(); ++i) {
    EXPECT_EQ(first.lockfile.packages[i].id, second.lockfile.packages[i].id);
    EXPECT_EQ(first.lockfile.packages[i].version, second.lockfile.packages[i].version);
    EXPECT_EQ(first.lockfile.packages[i].source_kind, second.lockfile.packages[i].source_kind);
    EXPECT_EQ(first.lockfile.packages[i].dependencies, second.lockfile.packages[i].dependencies);
  }
}

TEST(ResolverTests, RejectsSingleVersionConflictsAcrossPathAndGit) {
  const fs::path root = MakeTempDir("single-version-conflict");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path git_repo = root / "remote-feed";
  const std::string rev = CreateWorkspaceGitRepo(git_repo, "0.9.0");

  WriteFile(
    root / "pafio.toml",
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "feed = { package = \"acme/feed\", git = \""
    ) + CanonicalAbsolutePath(git_repo).generic_string()
      + "\", rev = \"" + rev +
      "\" }\n"
      "util = { package = \"acme/util\", path = \"vendor/util\" }\n"
  );
  WriteFile(
    root / "vendor/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );

  EXPECT_THROW(pafio::ResolveSingleVersionLockfile(root / "pafio.toml"), pafio::ResolutionError);
}

TEST(ResolverTests, ResolvesRegistryPackagesFromFilesystemRegistry) {
  const fs::path root = MakeTempDir("registry-workspace");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path registry_root = root / "registry";
  const std::string registry_url = FileUrl(registry_root);

  WriteFile(
    root / "publish/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.2.0\"\n"
    "edition = \"2026\"\n"
    "publish = true\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );
  WriteFile(root / "publish/util/src/lib.styio", "# util\n");
  PublishIntoFilesystemRegistryOrAssert(root / "publish/util/pafio.toml", registry_root);

  WriteFile(
    root / "publish/feed/pafio.toml",
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/feed\"\n"
      "version = \"1.2.0\"\n"
      "edition = \"2026\"\n"
      "publish = true\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[lib]\n"
      "path = \"src/lib.styio\"\n\n"
      "[dependencies]\n"
      "util = { package = \"acme/util\", version = \"0.2.0\", registry = \""
    ) + registry_url
      + "\" }\n"
  );
  WriteFile(root / "publish/feed/src/lib.styio", "# feed\n");
  PublishIntoFilesystemRegistryOrAssert(root / "publish/feed/pafio.toml", registry_root);

  WriteFile(
    root / "pafio.toml",
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "feed = { package = \"acme/feed\", version = \"1.2.0\", registry = \""
    ) + registry_url
      + "\" }\n"
  );

  const auto generated = pafio::ResolveSingleVersionLockfile(root / "pafio.toml");
  ASSERT_EQ(generated.lockfile.packages.size(), 3U);

  const auto find_package = [&](const std::string &name) -> const pafio::LockPackage &
  {
    const auto it = std::find_if(
      generated.lockfile.packages.begin(),
      generated.lockfile.packages.end(),
      [&](const pafio::LockPackage &package)
      {
        return package.name == name;
      }
    );
    EXPECT_NE(it, generated.lockfile.packages.end());
    return *it;
  };

  const pafio::LockPackage &feed = find_package("acme/feed");
  EXPECT_EQ(feed.source_kind, "registry");
  EXPECT_EQ(feed.registry.value_or(""), registry_url);
  ASSERT_TRUE(feed.sha256.has_value());
  EXPECT_EQ(feed.sha256->size(), 64U);
  EXPECT_TRUE(feed.id.starts_with("registry:acme/feed@1.2.0#"));
  ASSERT_EQ(feed.dependencies.size(), 1U);
  EXPECT_TRUE(feed.dependencies[0].starts_with("registry:acme/util@0.2.0#"));

  const pafio::LockPackage &util = find_package("acme/util");
  EXPECT_EQ(util.source_kind, "registry");
  EXPECT_EQ(util.registry.value_or(""), registry_url);
  ASSERT_TRUE(util.sha256.has_value());
  EXPECT_EQ(util.sha256->size(), 64U);

  const pafio::LockPackage &app = find_package("acme/app");
  EXPECT_EQ(app.source_kind, "workspace");
  ASSERT_EQ(app.dependencies.size(), 1U);
  EXPECT_EQ(app.dependencies[0], feed.id);

  EXPECT_TRUE(fs::exists(root / ".pafio-home" / "registry" / "blobs" / "sha256"));
  EXPECT_TRUE(fs::exists(root / ".pafio-home" / "registry" / "checkouts" / "acme" / "feed" / "1.2.0" / feed.sha256.value()));
}

TEST(TreeCliTests, RendersAsciiTreeForPinnedGitWorkspaceGraph) {
  const fs::path root = MakeTempDir("tree-git-workspace");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path git_repo = root / "remote-feed";
  const std::string rev = CreateWorkspaceGitRepo(git_repo, "0.9.0");
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "feed = { package = \"acme/feed\", git = \""
    ) + CanonicalAbsolutePath(git_repo).generic_string()
      + "\", rev = \"" + rev + "\" }\n"
  );

  testing::internal::CaptureStdout();
  EXPECT_EQ(pafio::RunCli({"tree", "--manifest-path", manifest_path.string()}), pafio::kExitSuccess);
  const std::string output = testing::internal::GetCapturedStdout();

  EXPECT_EQ(
    output,
    std::string("workspace:acme/app@0.1.0\n") + "\\- git:acme/feed@1.2.0#" + rev + "\n" + "   \\- git:acme/util@0.9.0#" + rev + "\n"
  );
}

TEST(TreeCliTests, EmitsJsonGraphWithRootIds) {
  const fs::path root = MakeTempDir("tree-json");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n"
  );
  testing::internal::CaptureStdout();
  EXPECT_EQ(pafio::RunCli({"--json", "tree", "--manifest-path", manifest_path.string()}), pafio::kExitSuccess);
  const json payload = json::parse(testing::internal::GetCapturedStdout());

  EXPECT_EQ(payload.at("command").get<std::string>(), "tree");
  EXPECT_EQ(payload.at("manifest_path").get<std::string>(), CanonicalAbsolutePath(manifest_path).string());
  ASSERT_EQ(payload.at("root_ids").size(), 1U);
  EXPECT_EQ(payload.at("root_ids")[0].get<std::string>(), "workspace:acme/app@0.1.0");
  ASSERT_EQ(payload.at("packages").size(), 1U);
  EXPECT_EQ(payload.at("packages")[0].at("id").get<std::string>(), "workspace:acme/app@0.1.0");
  EXPECT_EQ(payload.at("packages")[0].at("dependencies").size(), 0U);
}

TEST(TreeCliTests, MarksCyclesInAsciiOutput) {
  // single-version-v1 is fail-closed on cycles: `pafio tree` must reject the
  // graph with a ResolutionError that includes the full cycle path, not render
  // a soft "(cycle)" marker under a successful exit.
  const fs::path root = MakeTempDir("tree-cycle");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n\n"
    "[dependencies]\n"
    "util = { package = \"acme/util\", path = \"vendor/util\" }\n"
  );
  WriteFile(
    root / "vendor/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.2.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n\n"
    "[dependencies]\n"
    "app = { package = \"acme/app\", path = \"../..\" }\n"
  );

  testing::internal::CaptureStderr();
  EXPECT_EQ(pafio::RunCli({"tree", "--manifest-path", manifest_path.string()}), 13);
  const std::string err = testing::internal::GetCapturedStderr();
  EXPECT_NE(err.find("acyclic dependency graph"), std::string::npos);
  EXPECT_NE(err.find("workspace:acme/app@0.1.0"), std::string::npos);
  EXPECT_NE(err.find("path:acme/util@0.2.0"), std::string::npos);
}

TEST(AddCliTests, AddsPathDependencyAndRefreshesLockfile) {
  const fs::path root = MakeTempDir("add-path");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n"
  );
  WriteFile(
    root / "vendor/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.2.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );

  EXPECT_EQ(
    pafio::RunCli({"add", "acme/util", "--path", "vendor/util", "--manifest-path", manifest_path.string()}),
    pafio::kExitSuccess
  );

  EXPECT_EQ(
    ReadFile(manifest_path),
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "util = { package = \"acme/util\", path = \"vendor/util\" }\n"
    )
  );

  const auto lockfile = pafio::LoadLockfile(root / "pafio.lock");
  ASSERT_EQ(lockfile.packages.size(), 2U);
  EXPECT_EQ(lockfile.packages[0].id, "path:acme/util@0.2.0");
  EXPECT_EQ(lockfile.packages[1].id, "workspace:acme/app@0.1.0");
}

TEST(AddCliTests, AddsGitDependencyToDevSectionAndRefreshesLockfile) {
  const fs::path root = MakeTempDir("add-git-dev");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path git_repo = root / "remote-feed";
  const std::string rev = CreateWorkspaceGitRepo(git_repo, "0.9.0");
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n"
  );

  EXPECT_EQ(
    pafio::RunCli(
      {"add", "acme/feed", "--git", CanonicalAbsolutePath(git_repo).generic_string(), "--rev", rev, "--dev", "--manifest-path", manifest_path.string()}
    ),
    pafio::kExitSuccess
  );

  EXPECT_EQ(
    ReadFile(manifest_path),
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dev-dependencies]\n"
      "feed = { package = \"acme/feed\", git = \""
    ) + CanonicalAbsolutePath(git_repo).generic_string()
      + "\", rev = \"" + rev + "\" }\n"
  );

  const auto lockfile = pafio::LoadLockfile(root / "pafio.lock");
  ASSERT_EQ(lockfile.packages.size(), 3U);
}

TEST(AddCliTests, AddsRegistryDependencyAndRefreshesLockfile) {
  const fs::path root = MakeTempDir("add-registry");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path registry_root = root / "registry";
  const std::string registry_url = FileUrl(registry_root);
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    root / "publish/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.2.0\"\n"
    "edition = \"2026\"\n"
    "publish = true\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );
  WriteFile(root / "publish/util/src/lib.styio", "# util\n");
  PublishIntoFilesystemRegistryOrAssert(root / "publish/util/pafio.toml", registry_root);

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n"
  );

  EXPECT_EQ(
    pafio::RunCli(
      {"add", "acme/util", "--registry", registry_url, "--version", "0.2.0", "--manifest-path", manifest_path.string()}
    ),
    pafio::kExitSuccess
  );

  EXPECT_EQ(
    ReadFile(manifest_path),
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n\n"
      "[dependencies]\n"
      "util = { package = \"acme/util\", version = \"0.2.0\", registry = \""
    ) + registry_url
      + "\" }\n"
  );

  const auto lockfile = pafio::LoadLockfile(root / "pafio.lock");
  ASSERT_EQ(lockfile.packages.size(), 2U);
  EXPECT_TRUE(lockfile.packages[0].id.starts_with("registry:acme/util@0.2.0#"));
  EXPECT_EQ(lockfile.packages[0].registry.value_or(""), registry_url);
  EXPECT_EQ(lockfile.packages[1].id, "workspace:acme/app@0.1.0");
}

TEST(AddCliTests, RollsBackManifestAndLockWhenResolutionFails) {
  const fs::path root = MakeTempDir("add-rollback");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path git_repo = root / "remote-feed";
  const std::string rev = CreateWorkspaceGitRepo(git_repo, "0.9.0");
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n\n"
    "[dependencies]\n"
    "util = { package = \"acme/util\", path = \"vendor/util\" }\n"
  );
  WriteFile(
    root / "vendor/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );

  ASSERT_EQ(pafio::RunCli({"sync", "--manifest-path", manifest_path.string()}), pafio::kExitSuccess);
  const std::string original_manifest = ReadFile(manifest_path);
  const std::string original_lock = ReadFile(root / "pafio.lock");

  EXPECT_EQ(
    pafio::RunCli({"add", "acme/feed", "--git", CanonicalAbsolutePath(git_repo).generic_string(), "--rev", rev, "--manifest-path", manifest_path.string()}),
    pafio::kExitResolve
  );
  EXPECT_EQ(ReadFile(manifest_path), original_manifest);
  EXPECT_EQ(ReadFile(root / "pafio.lock"), original_lock);
}

TEST(RemoveCliTests, RemovesDependencyByPackageNameAndRefreshesLockfile) {
  const fs::path root = MakeTempDir("remove-package");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n\n"
    "[dependencies]\n"
    "util = { package = \"acme/util\", path = \"vendor/util\" }\n"
  );
  WriteFile(
    root / "vendor/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.2.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );

  ASSERT_EQ(pafio::RunCli({"sync", "--manifest-path", manifest_path.string()}), pafio::kExitSuccess);
  EXPECT_EQ(
    pafio::RunCli({"remove", "acme/util", "--manifest-path", manifest_path.string()}),
    pafio::kExitSuccess
  );

  EXPECT_EQ(
    ReadFile(manifest_path),
    std::string(
      "[pafio]\n"
      "manifest-version = 1\n\n"
      "[package]\n"
      "name = \"acme/app\"\n"
      "version = \"0.1.0\"\n"
      "edition = \"2026\"\n"
      "publish = false\n\n"
      "[build]\n"
      "implicit-std = true\n\n"
      "[[bin]]\n"
      "name = \"app\"\n"
      "path = \"src/main.styio\"\n"
    )
  );

  const auto lockfile = pafio::LoadLockfile(root / "pafio.lock");
  ASSERT_EQ(lockfile.packages.size(), 1U);
  EXPECT_EQ(lockfile.packages[0].id, "workspace:acme/app@0.1.0");
}

TEST(CheckCliTests, RejectsBrokenPathDependencyWithoutLockfile) {
  const fs::path root = MakeTempDir("check-broken-path");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n\n"
    "[dependencies]\n"
    "util = { package = \"acme/util\", path = \"vendor/missing\" }\n"
  );

  EXPECT_EQ(pafio::RunCli({"check", "--manifest-path", manifest_path.string()}), pafio::kExitResolve);
}

TEST(CheckCliTests, RejectsStaleLockfileAgainstResolver) {
  const fs::path root = MakeTempDir("check-stale-lock");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n"
  );

  ASSERT_EQ(pafio::RunCli({"sync", "--manifest-path", manifest_path.string()}), pafio::kExitSuccess);
  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n\n"
    "[dependencies]\n"
    "util = { package = \"acme/util\", path = \"vendor/util\" }\n"
  );
  WriteFile(
    root / "vendor/util/pafio.toml",
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/util\"\n"
    "version = \"0.2.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[lib]\n"
    "path = \"src/lib.styio\"\n"
  );

  EXPECT_EQ(
    pafio::RunCli({"check", "--manifest-path", manifest_path.string(), "--locked"}),
    pafio::kExitLock
  );
}

TEST(CheckCliTests, EmitsPackageCountForResolvedGraph) {
  const fs::path root = MakeTempDir("check-json");
  const ScopedEnvVar pafio_home("PAFIO_HOME", (root / ".pafio-home").string());
  const fs::path manifest_path = root / "pafio.toml";

  WriteFile(
    manifest_path,
    "[pafio]\n"
    "manifest-version = 1\n\n"
    "[package]\n"
    "name = \"acme/app\"\n"
    "version = \"0.1.0\"\n"
    "edition = \"2026\"\n\n"
    "[build]\n"
    "implicit-std = true\n\n"
    "[[bin]]\n"
    "name = \"app\"\n"
    "path = \"src/main.styio\"\n"
  );
  WriteFile(root / "src/main.styio", ">_(\"app\")\n");

  testing::internal::CaptureStdout();
  EXPECT_EQ(
    pafio::RunCli({"--json", "check", "--manifest-path", manifest_path.string(), "--dry-run"}),
    pafio::kExitSuccess
  );
  const json payload = json::parse(testing::internal::GetCapturedStdout());

  EXPECT_EQ(payload.at("action").get<std::string>(), "check");
  EXPECT_EQ(payload.at("command").get<std::string>(), "check");
  EXPECT_EQ(payload.at("status").get<std::string>(), "planned");
  EXPECT_EQ(payload.at("sync").at("status").get<std::string>(), "succeeded");
  EXPECT_EQ(payload.at("sync").at("packages").get<size_t>(), 1U);
}
