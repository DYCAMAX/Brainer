// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Where savestates live, what they are called, and what order they are listed
// in. One definition, because more than one thing needs to agree about it: the
// emulator writes and lists them from its own menu, and a frontend launching a
// game offers the same set before boot. When those disagree the same states come
// back in a different order depending on where you look, which is a confusing
// bug to chase and an easy one to introduce by editing only one copy.
//
// Deliberately depends on nothing but the standard library. A frontend should be
// able to include this without linking any of Dolphin.

#include <algorithm>
#include <cstddef>
#include <ctime>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace State::Layout
{
namespace fs = std::filesystem;

// Savestates carry this extension. Dolphin's numbered slot saves (.s01 and
// friends) live in the same directory and are deliberately not matched: they are
// managed by slot, not by name, and listing them here would mix two schemes.
inline constexpr std::string_view EXTENSION = ".sav";

// States written on a timer or at a checkpoint are told apart from ones a player
// asked for by a filename prefix rather than by a separate directory, so a single
// listing pass returns both and the two cannot get out of step. A game may append
// whatever its own trigger is called -- room, chapter, checkpoint.
inline constexpr std::string_view AUTOMATIC_PREFIX = "recovery-";

// Prefix for a state the player asked for.
inline constexpr std::string_view MANUAL_PREFIX = "state-";

inline std::tm LocalTime(std::time_t when)
{
  std::tm out{};
#if defined(_WIN32)
  localtime_s(&out, &when);
#else
  localtime_r(&when, &out);
#endif
  return out;
}

// Named by wall clock rather than by slot, so repeated saves accumulate instead
// of overwriting one another, and so name order matches time order.
inline std::string TimestampedName(std::time_t when,
                                   std::string_view prefix = MANUAL_PREFIX)
{
  const std::tm local = LocalTime(when);
  char stamp[32] = {};
  std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
  return std::string(prefix) + stamp + std::string(EXTENSION);
}

// Adds a stable suffix when more than one state is written during the same
// second. The unsuffixed overload above stays the common-case filename.
inline std::string TimestampedName(std::time_t when, std::size_t sequence,
                                   std::string_view prefix)
{
  if (sequence == 0)
    return TimestampedName(when, prefix);

  std::string name = TimestampedName(when, prefix);
  name.insert(name.size() - EXTENSION.size(), "-" + std::to_string(sequence));
  return name;
}

// A directory that is not there yields an empty list rather than throwing:
// callers ask before anything has been saved.
inline std::vector<fs::path> List(const fs::path& directory)
{
  struct StateFile
  {
    fs::path path;
    fs::file_time_type write_time{};
    bool has_write_time = false;
  };

  std::vector<StateFile> files;
  std::error_code ec;
  if (!fs::is_directory(directory, ec))
    return {};

  fs::directory_iterator entry(directory, ec);
  const fs::directory_iterator end;
  while (!ec && entry != end)
  {
    std::error_code type_ec;
    if (entry->is_regular_file(type_ec) && entry->path().extension() == EXTENSION)
    {
      std::error_code time_ec;
      const fs::file_time_type write_time = entry->last_write_time(time_ec);
      files.push_back({entry->path(), write_time, !time_ec});
    }
    entry.increment(ec);
  }

  // Snapshot metadata before sorting. Reading the filesystem from a comparator
  // can change its answer midway through sort if a state is removed.
  std::sort(files.begin(), files.end(), [](const StateFile& left, const StateFile& right) {
    if (left.has_write_time != right.has_write_time)
      return left.has_write_time;
    if (left.has_write_time && left.write_time != right.write_time)
      return left.write_time > right.write_time;
    return left.path.filename().native() < right.path.filename().native();
  });

  std::vector<fs::path> paths;
  paths.reserve(files.size());
  for (StateFile& file : files)
    paths.push_back(std::move(file.path));
  return paths;
}

inline std::vector<fs::path> ListAutomatic(const fs::path& directory,
                                           std::string_view prefix = AUTOMATIC_PREFIX)
{
  std::vector<fs::path> paths;
  for (const fs::path& path : List(directory))
  {
    if (path.filename().string().starts_with(prefix))
      paths.push_back(path);
  }
  return paths;
}

inline std::optional<fs::path> LatestAutomatic(const fs::path& directory,
                                               std::string_view prefix = AUTOMATIC_PREFIX)
{
  const std::vector<fs::path> paths = ListAutomatic(directory, prefix);
  return paths.empty() ? std::nullopt : std::optional<fs::path>(paths.front());
}

// Keeps the newest `keep` automatic states and removes the rest, returning how
// many went. Only prefixed files are ever considered, so a player's own saves
// survive no matter how many automatic ones pile up.
inline std::size_t PruneAutomatic(const fs::path& directory, std::size_t keep,
                                  std::string_view prefix = AUTOMATIC_PREFIX)
{
  const std::vector<fs::path> automatic = ListAutomatic(directory, prefix);
  std::error_code ec;
  std::size_t removed = 0;
  for (std::size_t index = keep; index < automatic.size(); ++index)
  {
    ec.clear();
    if (fs::remove(automatic[index], ec))
      ++removed;
  }
  return removed;
}
}  // namespace State::Layout
