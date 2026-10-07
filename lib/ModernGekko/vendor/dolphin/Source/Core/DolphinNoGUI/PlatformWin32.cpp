// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinNoGUI/Platform.h"

#include "Core/Config/MainSettings.h"
#include "Core/Config/ConfigManager.h"
#include "Core/Core.h"
#include "Core/SavestateLayout.h"
#include "Core/State.h"
#include "Core/System.h"

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>
#include <climits>
#include <dwmapi.h>
#include <thread>

#include "VideoCommon/Present.h"
#include "resource.h"

namespace
{
// Menu command ids. Load State entries are allocated a contiguous range,
// since the list is rebuilt from disk each time the menu opens.
constexpr UINT ID_SAVE_STATE = 41001;
constexpr UINT ID_PAUSE = 41002;
constexpr UINT ID_MUTE = 41003;
constexpr UINT ID_FULLSCREEN = 41004;
constexpr UINT ID_LOAD_STATE_FIRST = 41100;
constexpr UINT ID_LOAD_STATE_LAST = 41199;

// Hold-to-fast-forward target. 2x is fast enough to skip a cutscene without
// outrunning what most hosts can actually emulate.
constexpr float FAST_FORWARD_SPEED = 2.0f;

class PlatformWin32 final : public Platform
{
public:
  ~PlatformWin32() override;

  bool Init() override;
  void SetTitle(const std::string& string) override;
  void MainLoop() override;

  WindowSystemInfo GetWindowSystemInfo() const override;

private:
  static constexpr TCHAR WINDOW_CLASS_NAME[] = _T("DolphinNoGUI");

  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  static bool RegisterRenderWindowClass();
  bool CreateRenderWindow();
  bool CreateMenus();
  void RefreshMenu(HMENU menu);
  void SaveStateToStatesDirectory();
  void ToggleFullscreen();
  void UpdateWindowPosition();
  void ProcessEvents();

  HWND m_hwnd{};
  HMENU m_menu{};
  HMENU m_file_menu{};
  HMENU m_load_menu{};
  HMENU m_view_menu{};
  // Parallel to the Load State menu entries, rebuilt whenever it opens.
  std::vector<std::string> m_load_state_paths;
  std::time_t m_last_save_time{};
  std::size_t m_save_sequence{};
  bool m_fullscreen = false;
  LONG m_windowed_style = 0;
  RECT m_windowed_rect{};

  int m_window_x = Config::Get(Config::MAIN_RENDER_WINDOW_XPOS);
  int m_window_y = Config::Get(Config::MAIN_RENDER_WINDOW_YPOS);
  int m_window_width = Config::Get(Config::MAIN_RENDER_WINDOW_WIDTH);
  int m_window_height = Config::Get(Config::MAIN_RENDER_WINDOW_HEIGHT);
};

PlatformWin32::~PlatformWin32()
{
  if (m_hwnd)
    DestroyWindow(m_hwnd);
}

bool PlatformWin32::RegisterRenderWindowClass()
{
  WNDCLASSEX wc = {};
  wc.cbSize = sizeof(WNDCLASSEX);
  wc.style = 0;
  wc.lpfnWndProc = WndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = GetModuleHandle(nullptr);
  wc.hIcon = LoadIcon(nullptr, IDI_ICON1);
  wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  wc.lpszMenuName = nullptr;
  wc.lpszClassName = WINDOW_CLASS_NAME;
  wc.hIconSm = LoadIcon(nullptr, IDI_ICON1);

  if (!RegisterClassEx(&wc))
  {
    MessageBox(nullptr, _T("Window registration failed."), _T("Error"), MB_ICONERROR | MB_OK);
    return false;
  }

  return true;
}

bool PlatformWin32::CreateRenderWindow()
{
  m_hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, WINDOW_CLASS_NAME, _T("Dolphin"), WS_OVERLAPPEDWINDOW,
                          m_window_x < 0 ? CW_USEDEFAULT : m_window_x,
                          m_window_y < 0 ? CW_USEDEFAULT : m_window_y, m_window_width,
                          m_window_height, nullptr, nullptr, GetModuleHandle(nullptr), this);
  if (!m_hwnd)
  {
    MessageBox(nullptr, _T("CreateWindowEx failed."), _T("Error"), MB_ICONERROR | MB_OK);
    return false;
  }

  ShowWindow(m_hwnd, SW_SHOW);
  UpdateWindow(m_hwnd);
  return true;
}

bool PlatformWin32::CreateMenus()
{
  m_menu = CreateMenu();
  m_file_menu = CreatePopupMenu();
  m_load_menu = CreatePopupMenu();
  m_view_menu = CreatePopupMenu();
  if (!m_menu || !m_file_menu || !m_load_menu || !m_view_menu)
    return false;

  AppendMenuW(m_file_menu, MF_STRING, ID_SAVE_STATE, L"&Save State\tF1");
  AppendMenuW(m_file_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(m_load_menu), L"&Load State");
  AppendMenuW(m_file_menu, MF_SEPARATOR, 0, nullptr);
  // Checked state is refreshed from the core when the menu opens, so it cannot
  // drift out of step with an emulation that was paused some other way.
  AppendMenuW(m_file_menu, MF_STRING, ID_PAUSE, L"&Pause");

  AppendMenuW(m_view_menu, MF_STRING, ID_FULLSCREEN, L"&Fullscreen\tAlt+Enter");
  AppendMenuW(m_view_menu, MF_STRING, ID_MUTE, L"&Mute Audio");

  AppendMenuW(m_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(m_file_menu), L"&File");
  AppendMenuW(m_menu, MF_POPUP, reinterpret_cast<UINT_PTR>(m_view_menu), L"&View");
  return SetMenu(m_hwnd, m_menu) != FALSE;
}

// Rebuilt on open rather than cached: states are written by this process while
// the menu is closed, and by the launcher between sessions.
void PlatformWin32::RefreshMenu(const HMENU menu)
{
  if (menu == m_file_menu)
  {
    auto& system = Core::System::GetInstance();
    const bool paused = Core::GetState(system) == Core::State::Paused;
    CheckMenuItem(m_file_menu, ID_PAUSE, MF_BYCOMMAND | (paused ? MF_CHECKED : MF_UNCHECKED));
    return;
  }

  if (menu == m_view_menu)
  {
    CheckMenuItem(m_view_menu, ID_MUTE,
                  MF_BYCOMMAND |
                      (Config::Get(Config::MAIN_AUDIO_MUTED) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(m_view_menu, ID_FULLSCREEN,
                  MF_BYCOMMAND | (m_fullscreen ? MF_CHECKED : MF_UNCHECKED));
    return;
  }

  if (menu != m_load_menu)
    return;

  while (DeleteMenu(m_load_menu, 0, MF_BYPOSITION))
  {
  }
  m_load_state_paths.clear();

  // Location, extension and order all come from State::Layout, so this menu and
  // any frontend listing the same directory cannot disagree.
  const std::vector<std::filesystem::path> states =
      State::Layout::List(StringToPath(File::GetUserPath(D_STATESAVES_IDX)));

  if (states.empty())
  {
    AppendMenuW(m_load_menu, MF_STRING | MF_GRAYED, 0, L"(no savestates)");
    return;
  }

  const std::size_t limit = std::min<std::size_t>(
      states.size(), ID_LOAD_STATE_LAST - ID_LOAD_STATE_FIRST + 1);
  for (std::size_t i = 0; i < limit; ++i)
  {
    AppendMenuW(m_load_menu, MF_STRING, ID_LOAD_STATE_FIRST + i,
                states[i].filename().wstring().c_str());
    m_load_state_paths.push_back(PathToString(states[i]));
  }
}

void PlatformWin32::SaveStateToStatesDirectory()
{
  const std::string directory = File::GetUserPath(D_STATESAVES_IDX);
  File::CreateFullPath(directory);
  const std::time_t now = std::time(nullptr);
  if (now == m_last_save_time)
    ++m_save_sequence;
  else
  {
    m_last_save_time = now;
    m_save_sequence = 0;
  }

  const std::filesystem::path path =
      StringToPath(directory) /
      State::Layout::TimestampedName(now, m_save_sequence, State::Layout::MANUAL_PREFIX);
  State::SaveAs(Core::System::GetInstance(), PathToString(path));
}

void PlatformWin32::ToggleFullscreen()
{
  if (!m_fullscreen)
  {
    GetWindowRect(m_hwnd, &m_windowed_rect);
    m_windowed_style = GetWindowLong(m_hwnd, GWL_STYLE);

    MONITORINFO monitor{};
    monitor.cbSize = sizeof(monitor);
    if (!GetMonitorInfo(MonitorFromWindow(m_hwnd, MONITOR_DEFAULTTONEAREST), &monitor))
      return;

    SetMenu(m_hwnd, nullptr);
    SetWindowLong(m_hwnd, GWL_STYLE, m_windowed_style & ~WS_OVERLAPPEDWINDOW);
    SetWindowPos(m_hwnd, HWND_TOP, monitor.rcMonitor.left, monitor.rcMonitor.top,
                 monitor.rcMonitor.right - monitor.rcMonitor.left,
                 monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                 SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    m_fullscreen = true;
    return;
  }

  SetWindowLong(m_hwnd, GWL_STYLE, m_windowed_style);
  SetMenu(m_hwnd, m_menu);
  SetWindowPos(m_hwnd, nullptr, m_windowed_rect.left, m_windowed_rect.top,
               m_windowed_rect.right - m_windowed_rect.left,
               m_windowed_rect.bottom - m_windowed_rect.top,
               SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_NOZORDER);
  m_fullscreen = false;
}

bool PlatformWin32::Init()
{
  if (!RegisterRenderWindowClass() || !CreateRenderWindow() || !CreateMenus())
    return false;

  // TODO: Enter fullscreen if enabled.
  if (Config::Get(Config::MAIN_FULLSCREEN))
  {
    ProcessEvents();
  }

  if (Config::Get(Config::MAIN_DISABLE_SCREENSAVER))
    SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED);

  UpdateWindowPosition();
  return true;
}

void PlatformWin32::SetTitle(const std::string& string)
{
  SetWindowTextW(m_hwnd, UTF8ToWString(string).c_str());
}

void PlatformWin32::MainLoop()
{
  while (IsRunning())
  {
    UpdateRunningFlag();
    Core::HostDispatchJobs(Core::System::GetInstance());
    ProcessEvents();
    UpdateWindowPosition();

    // TODO: Is this sleep appropriate?
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

WindowSystemInfo PlatformWin32::GetWindowSystemInfo() const
{
  WindowSystemInfo wsi;
  wsi.type = WindowSystemType::Windows;
  wsi.render_window = reinterpret_cast<void*>(m_hwnd);
  wsi.render_surface = reinterpret_cast<void*>(m_hwnd);
  return wsi;
}

void PlatformWin32::UpdateWindowPosition()
{
  if (m_window_fullscreen)
    return;

  RECT rc = {};
  if (!GetWindowRect(m_hwnd, &rc))
    return;

  m_window_x = rc.left;
  m_window_y = rc.top;
  m_window_width = rc.right - rc.left;
  m_window_height = rc.bottom - rc.top;
}

void PlatformWin32::ProcessEvents()
{
  MSG msg;
  while (PeekMessage(&msg, m_hwnd, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}

LRESULT PlatformWin32::WndProc(const HWND hwnd, const UINT msg, const WPARAM wParam,
                               const LPARAM lParam)
{
  PlatformWin32* platform = reinterpret_cast<PlatformWin32*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
  switch (msg)
  {
  case WM_NCCREATE:
  {
    platform = static_cast<PlatformWin32*>(reinterpret_cast<CREATESTRUCT*>(lParam)->lpCreateParams);
    SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(platform));
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }

  case WM_CREATE:
  {
    if (hwnd)
    {
      // Remove rounded corners from the render window on Windows 11
      constexpr DWM_WINDOW_CORNER_PREFERENCE corner_preference = DWMWCP_DONOTROUND;
      DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner_preference,
                            sizeof(corner_preference));
    }
  }
  break;

  case WM_SIZE:
  {
    if (g_presenter)
      g_presenter->ResizeSurface();
  }
  break;

  case WM_KEYDOWN:
    // Bit 30 of lParam is the previous key state: ignore auto-repeat, or holding
    // F1 down would write a state every few milliseconds.
    if (wParam == VK_F1 && (static_cast<ULONG_PTR>(lParam) & (1u << 30)) == 0)
    {
      platform->SaveStateToStatesDirectory();
      return 0;
    }
    else if (wParam == VK_SPACE)
    {
      Config::SetCurrent(Config::MAIN_EMULATION_SPEED, FAST_FORWARD_SPEED);
      return 0;
    }
    else if (wParam == VK_F11)
    {
      platform->ToggleFullscreen();
      return 0;
    }
    else if (wParam == VK_ESCAPE && platform->m_fullscreen)
    {
      platform->ToggleFullscreen();
      return 0;
    }
    else if (wParam == VK_ESCAPE)
    {
      platform->RequestShutdown();
    }
    break;

  case WM_KEYUP:
    if (wParam == VK_SPACE)
    {
      Config::SetCurrent(Config::MAIN_EMULATION_SPEED, 1.0f);
      return 0;
    }
    break;

  case WM_KILLFOCUS:
    // Never leave emulation running fast because Space was released while
    // another window had focus and the key-up went elsewhere.
    Config::SetCurrent(Config::MAIN_EMULATION_SPEED, 1.0f);
    break;

  case WM_SYSKEYDOWN:
    if (wParam == VK_RETURN && (GetKeyState(VK_MENU) & 0x8000) != 0)
    {
      platform->ToggleFullscreen();
      return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);

  case WM_INITMENUPOPUP:
    if (platform)
      platform->RefreshMenu(reinterpret_cast<HMENU>(wParam));
    break;

  case WM_COMMAND:
  {
    if (!platform)
      break;
    const UINT command = LOWORD(wParam);
    auto& system = Core::System::GetInstance();
    if (command == ID_SAVE_STATE)
    {
      platform->SaveStateToStatesDirectory();
    }
    else if (command == ID_PAUSE)
    {
      const bool paused = Core::GetState(system) == Core::State::Paused;
      Core::SetState(system, paused ? Core::State::Running : Core::State::Paused);
    }
    else if (command == ID_MUTE)
    {
      Config::SetCurrent(Config::MAIN_AUDIO_MUTED, !Config::Get(Config::MAIN_AUDIO_MUTED));
    }
    else if (command == ID_FULLSCREEN)
    {
      platform->ToggleFullscreen();
    }
    else if (command >= ID_LOAD_STATE_FIRST && command <= ID_LOAD_STATE_LAST)
    {
      const std::size_t index = command - ID_LOAD_STATE_FIRST;
      if (index < platform->m_load_state_paths.size())
        State::LoadAs(system, platform->m_load_state_paths[index]);
    }
    break;
  }

  case WM_CLOSE:
    platform->RequestShutdown();
    break;

  default:
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }

  return 0;
}
}  // namespace

std::unique_ptr<Platform> Platform::CreateWin32Platform()
{
  return std::make_unique<PlatformWin32>();
}
