// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "Core/SavestateLayout.h"

namespace fs = std::filesystem;

namespace
{
class SavestateLayoutTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // Unique per run: a fixed name races when two runs overlap and inherits
    // whatever a run that died before cleanup left behind.
    m_root = fs::temp_directory_path() /
             ("dolphin-savestate-layout-" +
              std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    fs::create_directories(m_root, ec);
    ASSERT_FALSE(ec);
  }

  void TearDown() override
  {
    std::error_code ec;
    fs::remove_all(m_root, ec);
  }

  // `age` places the file in the past so ordering is deterministic rather than
  // depending on how fast the test runs.
  fs::path Touch(const std::string& name, std::chrono::seconds age)
  {
    const fs::path path = m_root / name;
    std::ofstream(path).put('x');
    std::error_code ec;
    fs::last_write_time(path, fs::file_time_type::clock::now() - age, ec);
    return path;
  }

  fs::path m_root;
};

TEST_F(SavestateLayoutTest, ListsNewestFirstAndIgnoresOtherFiles)
{
  Touch("state-old.sav", std::chrono::seconds(120));
  Touch("state-new.sav", std::chrono::seconds(0));
  Touch("notes.txt", std::chrono::seconds(0));
  // Dolphin's numbered slot saves share the directory and must stay out.
  Touch("GC6E01.s01", std::chrono::seconds(0));

  const auto states = State::Layout::List(m_root);
  ASSERT_EQ(states.size(), 2u);
  EXPECT_EQ(states[0].filename().string(), "state-new.sav");
  EXPECT_EQ(states[1].filename().string(), "state-old.sav");
}

TEST_F(SavestateLayoutTest, EqualTimestampsFallBackToFilename)
{
  const auto when = std::chrono::seconds(60);
  Touch("state-b.sav", when);
  Touch("state-a.sav", when);

  const auto states = State::Layout::List(m_root);
  ASSERT_EQ(states.size(), 2u);
  EXPECT_EQ(states[0].filename().string(), "state-a.sav");
  EXPECT_EQ(states[1].filename().string(), "state-b.sav");
}

TEST_F(SavestateLayoutTest, MissingDirectoryIsEmptyNotAnError)
{
  EXPECT_TRUE(State::Layout::List(m_root / "absent").empty());
}

TEST_F(SavestateLayoutTest, AutomaticStatesAreSelectedByPrefix)
{
  Touch("state-mine.sav", std::chrono::seconds(0));
  Touch("recovery-001.sav", std::chrono::seconds(90));
  Touch("recovery-002.sav", std::chrono::seconds(30));

  const auto automatic = State::Layout::ListAutomatic(m_root);
  ASSERT_EQ(automatic.size(), 2u);
  EXPECT_EQ(automatic[0].filename().string(), "recovery-002.sav");

  const auto latest = State::Layout::LatestAutomatic(m_root);
  ASSERT_TRUE(latest.has_value());
  EXPECT_EQ(latest->filename().string(), "recovery-002.sav");
}

TEST_F(SavestateLayoutTest, PruningNeverTouchesPlayerStates)
{
  const auto mine = Touch("state-mine.sav", std::chrono::seconds(0));
  const auto oldest = Touch("recovery-001.sav", std::chrono::seconds(90));
  const auto newest = Touch("recovery-002.sav", std::chrono::seconds(30));

  EXPECT_EQ(State::Layout::PruneAutomatic(m_root, 1), 1u);
  EXPECT_FALSE(fs::exists(oldest));
  EXPECT_TRUE(fs::exists(newest));
  EXPECT_TRUE(fs::exists(mine));
}

TEST_F(SavestateLayoutTest, KeepingMoreThanExistRemovesNothing)
{
  Touch("recovery-001.sav", std::chrono::seconds(30));
  EXPECT_EQ(State::Layout::PruneAutomatic(m_root, 10), 0u);
}

TEST_F(SavestateLayoutTest, ACustomPrefixIgnoresTheDefaultOne)
{
  Touch("recovery-001.sav", std::chrono::seconds(30));
  const auto chapter_old = Touch("chapter-001.sav", std::chrono::seconds(90));
  const auto chapter_new = Touch("chapter-002.sav", std::chrono::seconds(0));

  const auto chapters = State::Layout::ListAutomatic(m_root, "chapter-");
  ASSERT_EQ(chapters.size(), 2u);
  EXPECT_EQ(chapters[0].filename().string(), "chapter-002.sav");

  EXPECT_EQ(State::Layout::PruneAutomatic(m_root, 1, "chapter-"), 1u);
  EXPECT_FALSE(fs::exists(chapter_old));
  EXPECT_TRUE(fs::exists(chapter_new));
  // The default-prefixed state belongs to somebody else and must survive.
  EXPECT_TRUE(fs::exists(m_root / "recovery-001.sav"));
}

TEST_F(SavestateLayoutTest, TimestampedNameSortsInTimeOrder)
{
  // 2026-08-05 13:15:02 and one minute later, built explicitly so the test does
  // not depend on the clock.
  std::tm earlier{};
  earlier.tm_year = 126;
  earlier.tm_mon = 7;
  earlier.tm_mday = 5;
  earlier.tm_hour = 13;
  earlier.tm_min = 15;
  earlier.tm_sec = 2;
  earlier.tm_isdst = -1;
  std::tm later = earlier;
  later.tm_min = 16;

  const std::string first = State::Layout::TimestampedName(std::mktime(&earlier));
  const std::string second = State::Layout::TimestampedName(std::mktime(&later));

  EXPECT_EQ(first, "state-20260805-131502.sav");
  EXPECT_LT(first, second);
  EXPECT_TRUE(first.starts_with(State::Layout::MANUAL_PREFIX));
  EXPECT_TRUE(first.ends_with(State::Layout::EXTENSION));
}

TEST_F(SavestateLayoutTest, TimestampedNamesCanBeDisambiguated)
{
  const std::time_t when = std::time(nullptr);
  const std::string first = State::Layout::TimestampedName(when);
  const std::string second =
      State::Layout::TimestampedName(when, 1, State::Layout::MANUAL_PREFIX);

  EXPECT_NE(first, second);
  EXPECT_TRUE(second.ends_with("-1.sav"));
}
}  // namespace
