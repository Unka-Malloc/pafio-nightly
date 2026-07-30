#include "SpioCore/Paths.hpp"

#include "SpioCore/Errors.hpp"

#include <cstdlib>

namespace fs = std::filesystem;

namespace spio
{

fs::path ProjectRoot()
{
  return fs::path(SPIO_PROJECT_ROOT);
}

fs::path CanonicalAbsolutePath(const fs::path &path)
{
  return fs::absolute(path).lexically_normal();
}

fs::path ProjectStateRootForManifest(const fs::path &manifest_path)
{
  return CanonicalAbsolutePath(CanonicalAbsolutePath(manifest_path).parent_path() / ".spio");
}

fs::path ProjectVendorRootForManifest(const fs::path &manifest_path)
{
  return CanonicalAbsolutePath(ProjectStateRootForManifest(manifest_path) / "vendor");
}

std::optional<fs::path> ResolveOptionalSpioHome()
{
  if (const char *explicit_home = std::getenv("SPIO_HOME"); explicit_home != nullptr && explicit_home[0] != '\0')
  {
    return CanonicalAbsolutePath(explicit_home);
  }
  if (const char *home = std::getenv("HOME"); home != nullptr && home[0] != '\0')
  {
    return CanonicalAbsolutePath(fs::path(home) / ".spio");
  }
  return std::nullopt;
}

fs::path ResolveSpioHome()
{
  const std::optional<fs::path> spio_home = ResolveOptionalSpioHome();
  if (!spio_home.has_value())
  {
    throw CacheError("unable to resolve SPIO_HOME: set SPIO_HOME or HOME");
  }
  return *spio_home;
}

fs::path RegistryCacheRoot(const fs::path &spio_home)
{
  return CanonicalAbsolutePath(CanonicalAbsolutePath(spio_home) / "registry");
}

fs::path RegistryTrustRoot(const fs::path &spio_home)
{
  return CanonicalAbsolutePath(RegistryCacheRoot(spio_home) / "trust");
}

fs::path RegistryTrustStorePath(const fs::path &spio_home)
{
  return CanonicalAbsolutePath(RegistryTrustRoot(spio_home) / "registry-trust.json");
}

fs::path RegistryIndexCacheRoot(const fs::path &spio_home)
{
  return CanonicalAbsolutePath(RegistryCacheRoot(spio_home) / "index");
}

fs::path RegistryBlobCacheRoot(const fs::path &spio_home)
{
  return CanonicalAbsolutePath(RegistryCacheRoot(spio_home) / "blobs" / "sha256");
}

fs::path RegistryCheckoutRoot(const fs::path &spio_home)
{
  return CanonicalAbsolutePath(RegistryCacheRoot(spio_home) / "checkouts");
}

fs::path StaticRegistryKeyRoot(const fs::path &spio_home)
{
  return CanonicalAbsolutePath(RegistryCacheRoot(spio_home) / "static" / "keys");
}

}  // namespace spio
