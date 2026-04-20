#include "SpioCloud/Execution.hpp"

#include <cstdlib>

namespace
{

bool IsAllowedCloudValue(const std::string &value, const std::initializer_list<const char *> &allowed)
{
  for (const char *candidate : allowed)
  {
    if (value == candidate)
    {
      return true;
    }
  }
  return false;
}

std::string DefaultSecurityProfile(const std::string &risk_class, const std::string &execution_lane)
{
  if (execution_lane == "warm-shared")
  {
    return "trusted-warm";
  }
  if (risk_class == "partner-controlled")
  {
    return "partner-restricted";
  }
  return "sandbox-default";
}

std::string DefaultWorkerTrustTier(const std::string &risk_class, const std::string &execution_lane)
{
  if (execution_lane == "warm-shared")
  {
    return "internal-warm";
  }
  if (risk_class == "trusted-internal")
  {
    return "trusted-isolated";
  }
  if (risk_class == "partner-controlled")
  {
    return "partner-isolated";
  }
  return "untrusted-isolated";
}

std::string CompilerFingerprintForState(const spio::ProjectToolchainState &state)
{
  if (state.mode == "build")
  {
    return state.source_revision.value_or("source-revision-unset");
  }
  return "published-" + state.channel + "-" + state.build_mode;
}

}  // namespace

namespace spio
{

std::string DetectCloudPlatform()
{
#if defined(__linux__)
  return "linux";
#elif defined(_WIN32)
  return "windows";
#elif defined(__APPLE__)
  return "macos";
#else
  return "unknown";
#endif
}

std::string DetectCloudArchitecture()
{
#if defined(__aarch64__) || defined(_M_ARM64)
  return "aarch64";
#elif defined(__x86_64__) || defined(_M_X64)
  return "x86_64";
#elif defined(__arm__) || defined(_M_ARM)
  return "arm";
#else
  return "unknown";
#endif
}

std::string DetectCloudBaseImageRevision()
{
  if (const char *revision = std::getenv("SPIO_CLOUD_BASE_IMAGE_REVISION"); revision != nullptr && revision[0] != '\0')
  {
    return revision;
  }
  return "dev-unset";
}

CloudExecutionPolicy ResolveCloudExecutionPolicy(const ProjectToolchainState &state)
{
  CloudExecutionPolicy policy;
  policy.risk_class = state.risk_class;
  policy.requested_execution_lane = state.preferred_execution_lane;
  policy.execution_lane = state.preferred_execution_lane;
  policy.requested_security_profile = state.security_profile;
  policy.security_profile = state.security_profile;

  if (!IsAllowedCloudValue(policy.risk_class, {"trusted-internal", "partner-controlled", "untrusted-user"}))
  {
    policy.risk_class = "untrusted-user";
    policy.fallback_applied = true;
    policy.routing_reason = "unknown risk class fell back to untrusted-user";
  }

  if (!IsAllowedCloudValue(policy.execution_lane, {"isolated", "warm-shared"}))
  {
    policy.execution_lane = "isolated";
    policy.fallback_applied = true;
    policy.routing_reason = "unknown execution lane fell back to isolated";
  }

  if (policy.risk_class == "untrusted-user" && policy.execution_lane != "isolated")
  {
    policy.execution_lane = "isolated";
    policy.fallback_applied = true;
    policy.routing_reason = "untrusted-user jobs require isolated execution";
  }
  else if (policy.risk_class == "partner-controlled" && policy.execution_lane == "warm-shared")
  {
    policy.execution_lane = "isolated";
    policy.fallback_applied = true;
    policy.routing_reason = "partner-controlled jobs require explicit allowlist before warm-shared execution";
  }
  else if (policy.routing_reason.empty())
  {
    if (policy.execution_lane == "warm-shared")
    {
      policy.routing_reason = "trusted internal workload is eligible for warm-shared execution";
    }
    else
    {
      policy.routing_reason = "default isolated execution protects the platform before enabling warm-shared reuse";
    }
  }

  const std::string expected_security = DefaultSecurityProfile(policy.risk_class, policy.execution_lane);
  if (!IsAllowedCloudValue(policy.security_profile, {"sandbox-default", "partner-restricted", "trusted-warm"}) ||
      policy.security_profile != expected_security)
  {
    policy.security_profile = expected_security;
    policy.fallback_applied = true;
  }

  policy.worker_trust_tier = DefaultWorkerTrustTier(policy.risk_class, policy.execution_lane);
  policy.cache_policy.worker_local_reuse = policy.execution_lane == "warm-shared";
  policy.cache_policy.shared_cache_promotion_eligible = policy.risk_class == "trusted-internal";
  policy.worker_pool_key = {
      .platform = DetectCloudPlatform(),
      .architecture = DetectCloudArchitecture(),
      .execution_lane = policy.execution_lane,
      .worker_trust_tier = policy.worker_trust_tier,
      .toolchain_mode = state.mode,
      .channel = state.channel,
      .build_mode = state.build_mode,
      .compiler_fingerprint = CompilerFingerprintForState(state),
      .base_image_revision = DetectCloudBaseImageRevision(),
  };
  return policy;
}

}  // namespace spio
