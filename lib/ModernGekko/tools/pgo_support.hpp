#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace moderngekko::pgo
{
namespace fs = std::filesystem;

enum class PgoMode
{
  Off,
  Generate,
  Use,
};

std::string_view PgoModeName(PgoMode mode);

struct PgoBuildOptions
{
  PgoMode mode = PgoMode::Off;
  fs::path merged_profile;
  std::string merged_profile_sha256;
  bool reject_stale_profile = true;
};

struct Workspace
{
  fs::path root;
  fs::path raw;
  fs::path generate_modules;
  fs::path merged_profile;
  fs::path manifest;
};

Workspace DeriveWorkspace(const fs::path& profile_dir, const fs::path& output,
                          std::string_view disc_id);

inline constexpr std::size_t WINDOWS_PATH_LIMIT = 260;

std::size_t LongestModuleObjectPathLength(const fs::path& cache_root, std::string_view disc_id);

std::string PathText(const fs::path& path);

std::string ClangPathText(const fs::path& path);

struct ProfdataSummary
{
  std::uint64_t total_functions = 0;
  std::uint64_t maximum_function_count = 0;
  std::string error;

  explicit operator bool() const { return error.empty(); }
};

ProfdataSummary ParseProfdataSummary(std::string_view text);

std::optional<int> ParseLlvmMajorVersion(std::string_view text);

std::vector<fs::path> DiscoverRawProfiles(const fs::path& raw_directory);

struct LlvmProfdataSources
{
  fs::path explicit_path;
  fs::path environment_path;
  fs::path clang_path;
  fs::path print_prog_name;
  fs::path xcrun_path;
};

std::vector<fs::path> LlvmProfdataCandidates(const LlvmProfdataSources& sources);

std::string PgoCacheIdentity(const PgoBuildOptions& options);

struct ManifestFacts
{
  std::string backend;
  std::string clang_version;
  std::string llvm_profdata_version;
  std::size_t raw_profile_count = 0;
  std::uint64_t total_functions = 0;
  std::uint64_t maximum_function_count = 0;
};

std::string FormatPgoManifest(const PgoBuildOptions& options, const ManifestFacts& facts);
}  // namespace moderngekko::pgo
