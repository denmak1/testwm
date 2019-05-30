#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0500

#include "workspaces.h"

#include <stdlib.h>
#include <stdio.h>
#include <shellapi.h>

#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iterator>

#define DEFAULT_MODKEY        MOD_CONTROL | MOD_ALT
#define MAX_IGNORE            16
#define DEFAULT_TILING_MODE   MODE_VERTICAL
#define TAGS                  9

#define DEBUG                 0

static const char POS_FILE_NAME[] = "wndpos.txt";

// hotkeys
enum controls {
  KEY_MOVE_MAIN,
  KEY_EXIT,
  KEY_MARGIN_LEFT,
  KEY_IGNORE,
  KEY_MOUSE_LOCK,
  KEY_MAXIMIZE,
  KEY_MOVE_UP,
  KEY_MOVE_DOWN,
  KEY_DISP_CLASS,
  KEY_TILE,
  KEY_UNTILE,
  KEY_INC_AREA,
  KEY_DEC_AREA,
  KEY_CLOSE_WIN,
  KEY_BRING_TO_MOUSE,

  KEY_HALVE_LEFT,
  KEY_HALVE_RIGHT,
  KEY_LEFT,
  KEY_RIGHT,
  KEY_UP,
  KEY_DOWN,
  KEY_SAVE_HWNDS,
  KEY_RESTORE_HWNDS,

  KEY_NUMBER_OF_KEYS
};

typedef enum position {
  POS_LEFT,
  POS_RIGHT,
  POS_TOP,
  POS_BOTTOM,
  POS_TOP_LEFT,
  POS_TOP_RIGHT,
  POS_BOTTOM_LEFT,
  POS_BOTTOM_RIGHT,
  POS_FULL
} position;

typedef enum direction {
  DIR_LEFT,
  DIR_RIGHT,
  DIR_UP,
  DIR_DOWN
} direction;

// Timer modes
enum timer_modes {
  TIMER_UPDATE_MOUSE = 0
};

// Global variables
int screen_x, screen_y, screen_width, screen_height;
unsigned short disableNext = 0;
int modkeys = DEFAULT_MODKEY;
char ignoreClasses[MAX_IGNORE][128]; // Exclude tiling from the classes in here
char includeClasses[MAX_IGNORE][128]; // Only tile the classes in here
unsigned short include_mode = 0; // Exclude by default

// Shell hook stuff
// window smessage id
UINT shellhookid;
BOOL (__stdcall *RegisterShellHookWindow_)(HWND) = NULL;

// list of all hwnds
std::vector<HWND> all_hwnds;

// init ws handler
static workspace_handler_t ws_handler;

// split string into a vector of strings at a given delimiter character
void split_str(const std::string &str, std::vector<std::string> &strs) {
  std::istringstream buffer(str);

  std::copy(std::istream_iterator<std::string>(buffer), 
            std::istream_iterator<std::string>(),
            std::back_inserter(strs));
}

// hotkey stuff
void register_hotkeys(HWND hwnd) {
  // register main hotkeys
  RegisterHotKey(hwnd, KEY_MOVE_MAIN,     modkeys,             VK_RETURN);
  RegisterHotKey(hwnd, KEY_EXIT,          modkeys,             VK_ESCAPE);
  RegisterHotKey(hwnd, KEY_MARGIN_LEFT,   modkeys,             'H');
  RegisterHotKey(hwnd, KEY_IGNORE,        modkeys,             'I');
  RegisterHotKey(hwnd, KEY_MOUSE_LOCK,    modkeys,             'U');
  RegisterHotKey(hwnd, KEY_MAXIMIZE,      modkeys,             VK_SPACE);
  RegisterHotKey(hwnd, KEY_MOVE_UP,       modkeys | MOD_SHIFT, 'K');
  RegisterHotKey(hwnd, KEY_MOVE_DOWN,     modkeys | MOD_SHIFT, 'J');
  RegisterHotKey(hwnd, KEY_DISP_CLASS,    modkeys,             'Y');
  RegisterHotKey(hwnd, KEY_TILE,          modkeys,             'O');
  RegisterHotKey(hwnd, KEY_UNTILE,        modkeys,             'P');
  RegisterHotKey(hwnd, KEY_INC_AREA,      modkeys,             'Z');
  RegisterHotKey(hwnd, KEY_DEC_AREA,      modkeys,             'X');
  RegisterHotKey(hwnd, KEY_BRING_TO_MOUSE,modkeys,             'B');

  RegisterHotKey(hwnd, KEY_HALVE_LEFT,    modkeys,             'L');
  RegisterHotKey(hwnd, KEY_HALVE_RIGHT,   modkeys,             'J');
  RegisterHotKey(hwnd, KEY_CLOSE_WIN,     modkeys,             'C');
  RegisterHotKey(hwnd, KEY_LEFT,          modkeys,             VK_LEFT);
  RegisterHotKey(hwnd, KEY_RIGHT,         modkeys,             VK_RIGHT);
  RegisterHotKey(hwnd, KEY_UP,            modkeys,             VK_UP);
  RegisterHotKey(hwnd, KEY_DOWN,          modkeys,             VK_DOWN);
  RegisterHotKey(hwnd, KEY_SAVE_HWNDS,    modkeys,             'S');
  RegisterHotKey(hwnd, KEY_RESTORE_HWNDS, modkeys,             'R');
}


void unregister_hotkeys(HWND hwnd) {
  for (int i = 1; i <= KEY_NUMBER_OF_KEYS; i++) {
    UnregisterHotKey(hwnd, i);
  }
}


void get_monitor_res(MONITORINFO mon_info, int &x, int &y) {
  if (mon_info.dwFlags == MONITORINFOF_PRIMARY) {
    x = mon_info.rcWork.right;
    y = mon_info.rcWork.bottom;
  }

  else {
    x = abs(mon_info.rcWork.right - mon_info.rcWork.left);
    y = abs(mon_info.rcWork.bottom - mon_info.rcWork.top);
  }
}


MONITORINFO get_monitor_of_window(HWND hwnd) {
  MONITORINFO mon_info;
  mon_info.cbSize = sizeof(MONITORINFO);
  HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  GetMonitorInfo(hmon, &mon_info);
  return mon_info;
}


int is_good_window(HWND hwnd) {
  if (!disableNext && IsWindowVisible(hwnd) && (GetParent(hwnd) == 0)) {
    int exstyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    HWND owner = GetWindow(hwnd, GW_OWNER);

    RECT rect;
    GetWindowRect(hwnd, &rect);

    // make sure window is in the screen
    int res_x, res_y;
    get_monitor_res(get_monitor_of_window(hwnd), res_x, res_y);

    /*
    if (screen_width > 0 && screen_height > 0) {
      if (!((rect.left > screen_x || rect.right > screen_x) &&
            (rect.left < screen_x + screen_width || rect.right < screen_x + screen_width))) {
        return FALSE;
      }

      if (!((rect.top > screen_y || rect.bottom > screen_y) &&
            (rect.top < screen_y + screen_height || rect.bottom < screen_x + screen_height))) {
        return FALSE;
      }
    }
    */

    if ((((exstyle & WS_EX_TOOLWINDOW) == 0) && (owner == 0) ) ||
        ((exstyle & WS_EX_APPWINDOW) && (owner != 0))) {
      int i;
      LPSTR temp = (LPSTR)malloc(sizeof(TCHAR) * 128);
      GetClassName(hwnd, temp, 128);

      if (include_mode == 1) {
        for (i = 0; i < MAX_IGNORE; i++) {
          if (!strcmp(temp, includeClasses[i])) {
            free(temp);
            return TRUE;
          }
        }
       free(temp);
       return FALSE;
      }

      else {
        for (i = 0; i < MAX_IGNORE; i++) {
          if (!strcmp(temp, ignoreClasses[i])) {
            free(temp);
            return FALSE;
          }
        }
      }

      free(temp);
      return TRUE;
    }
  }
  return FALSE;
}


std::string get_window_title(HWND hwnd) {
  int bufsize = GetWindowTextLength(hwnd) + 1;
  char title[bufsize];
  GetWindowText(hwnd, title, bufsize);
  return std::string(title);
}


BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
  if (is_good_window(hwnd)) {
    if (DEBUG) {
      printf("good window = %d = ", hwnd);
      printf("%s\n", get_window_title(hwnd).c_str());
    }
    all_hwnds.push_back(hwnd);
  }

  return TRUE;
}


BOOL CALLBACK EnumWindowsRestore(HWND hwnd, LPARAM lParam) {
  return TRUE;
}


int save_positions_to_file() {
  // repopulate hwnds
  all_hwnds.clear();
  BOOL enumeratingWindowsSucceeded = ::EnumWindows(EnumWindowsProc, 0);
  HWND fghwnd = GetForegroundWindow();

  remove(POS_FILE_NAME);
  FILE *f = fopen(POS_FILE_NAME, "w");

  int i;
  i = 0;
  for (std::vector<HWND>::iterator iter = all_hwnds.begin();
       iter != all_hwnds.end();
       ++iter) {
    RECT rect;
    GetWindowRect(*iter, &rect);

    WINDOWPLACEMENT wndpl;
    wndpl.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(*iter, &wndpl);

    // format is: <hwnd> <left> <top> <right> <bottom> <flags> <showcmd>
    //            <minpos_x> <minpos_y> <maxpos_x> <maxpos_y> <rc_left>
    //            <rc_top> <rc_right> <rc_bottom>
    fprintf(f, "%u %ld %ld %ld %ld %u %u %ld %ld %ld %ld %ld %ld %ld %ld %s\n",
            *iter, rect.left, rect.top, rect.right, rect.bottom,
            wndpl.flags, wndpl.showCmd,
            wndpl.ptMinPosition.x, wndpl.ptMinPosition.y,
            wndpl.ptMaxPosition.x, wndpl.ptMaxPosition.y,
            wndpl.rcNormalPosition.left,wndpl.rcNormalPosition.top,
            wndpl.rcNormalPosition.right, wndpl.rcNormalPosition.bottom,
            get_window_title(*iter).c_str());
    i++;
  }

  fclose(f);
  return i;
}


int restore_positions_from_file() {
  std::ifstream f(POS_FILE_NAME);

  std::string line;
  int i;
  i = 0;
  while (std::getline(f, line)) {
    std::vector<std::string> strs;
    split_str(line, strs);

    // SetWindowPos
    HWND hwnd = (HWND)strtol(strs.at(0).c_str(), NULL, 0);
    int cx = abs(atol(strs.at(3).c_str()) - atol(strs.at(1).c_str()));
    int cy = abs(atol(strs.at(4).c_str()) - atol(strs.at(2).c_str()));
    SetWindowPos((HWND) hwnd, (HWND) HWND_BOTTOM,
                 atol(strs.at(1).c_str()), atol(strs.at(2).c_str()),
                 cx, cy, SWP_NOACTIVATE);

    // SetWindowPlacement
    WINDOWPLACEMENT wndpl;
    wndpl.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hwnd, &wndpl);

    wndpl.flags = atol(strs.at(5).c_str());
    wndpl.showCmd = atol(strs.at(6).c_str());
    wndpl.ptMinPosition =
      POINT{atol(strs.at(7).c_str()), atol(strs.at(8).c_str())};
    wndpl.ptMaxPosition =
      POINT{atol(strs.at(9).c_str()), atol(strs.at(10).c_str())};
    wndpl.rcNormalPosition =
      RECT{atol(strs.at(11).c_str()), atol(strs.at(12).c_str()),
           atol(strs.at(13).c_str()), atol(strs.at(14).c_str())};
    SetWindowPlacement(hwnd, &wndpl);

    // if the window was maximized when saving, restore maximized status
    if (atol(strs.at(5).c_str()) == 2 && atol(strs.at(6).c_str()) == 3) {
      ShowWindow(hwnd, SW_RESTORE);
      ShowWindow(hwnd, SW_MAXIMIZE);
    }

    if (DEBUG) {
      printf("restoring: ");
      for (int j = 0; j < 15; j++)
        printf("%ld ", atoi(strs.at(j).c_str()));
      printf("\n");
    }

    i++;
  }

  f.close();
  return i;
}


int halve_hwnd(HWND hwnd, direction dir) {
  int res_x, res_y;
  MONITORINFO mon_info = get_monitor_of_window(hwnd);
  get_monitor_res(mon_info, res_x, res_y);

  WINDOWPLACEMENT wndpl;
  wndpl.length = sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(hwnd, &wndpl);

  RECT rect;
  GetWindowRect(hwnd, &rect);

  long x, y, c_x, c_y;
  switch (dir) {
    case DIR_RIGHT:
      x = rect.left;
      y = rect.top;
      c_x = abs(rect.right - rect.left) / 2;
      c_y = abs(rect.bottom - rect.top);
      ShowWindow(hwnd, SW_RESTORE);
      return SetWindowPos(hwnd, HWND_TOP, x, y, c_x, c_y, SWP_NOZORDER);
      break;

    case DIR_LEFT:
      x = rect.left + (abs(rect.right - rect.left) / 2);
      y = rect.top;
      c_x = abs(rect.right - rect.left) / 2;
      c_y = abs(rect.bottom - rect.top);
      ShowWindow(hwnd, SW_RESTORE);
      return SetWindowPos(hwnd, HWND_TOP, x, y, c_x, c_y, SWP_NOZORDER);
      break;

    default:
      return -1;
  }

  return -1;
}


int maximize_hwnd(HWND hwnd) {
  WINDOWPLACEMENT wndpl;
  wndpl.length = sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(hwnd, &wndpl);

  if (wndpl.showCmd == SW_MAXIMIZE)
    ShowWindow(hwnd, SW_RESTORE);
  else if (wndpl.showCmd == SW_RESTORE || wndpl.showCmd == SW_SHOWNORMAL)
    ShowWindow(hwnd, SW_MAXIMIZE);
  else
    return -1;

  return 0;
}


int snap_hwnd(HWND hwnd, position pos) {
  int res_x, res_y;
  MONITORINFO mon_info = get_monitor_of_window(hwnd);
  res_x = abs(mon_info.rcWork.right - mon_info.rcWork.left);
  res_y = abs(mon_info.rcWork.top - mon_info.rcWork.bottom);

  // does not account for task bar
  //  get_monitor_res(mon_info, res_x, res_y);

  WINDOWPLACEMENT wndpl;
  wndpl.length = sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(hwnd, &wndpl);

  RECT rect;
  GetWindowRect(hwnd, &rect);

  long x, y, c_x, c_y;
  switch (pos) {
    case POS_LEFT:
      x = mon_info.rcWork.left;
      y = mon_info.rcWork.top;
      c_x = res_x / 2;
      c_y = res_y;
      ShowWindow(hwnd, SW_RESTORE);
      return SetWindowPos(hwnd, HWND_TOP, x, y, c_x, c_y, SWP_NOZORDER);
      break;

    case POS_RIGHT:
      x = mon_info.rcWork.left + (res_x / 2);
      y = mon_info.rcWork.top;
      c_x = res_x / 2;
      c_y = res_y;
      ShowWindow(hwnd, SW_RESTORE);
      return SetWindowPos(hwnd, HWND_TOP, x, y, c_x, c_y, SWP_NOZORDER);
      break;

    case POS_TOP:
      x = mon_info.rcWork.left;
      y = mon_info.rcWork.top;
      c_x = res_x;
      c_y = res_y / 2;
      ShowWindow(hwnd, SW_RESTORE);
      return SetWindowPos(hwnd, HWND_TOP, x, y, c_x, c_y, SWP_NOZORDER);
      break;

    case POS_BOTTOM:
      x = mon_info.rcWork.left;
      y = mon_info.rcWork.top + (res_y / 2);
      c_x = res_x;
      c_y = res_y / 2;
      ShowWindow(hwnd, SW_RESTORE);
      return SetWindowPos(hwnd, HWND_TOP, x, y, c_x, c_y, SWP_NOZORDER);
     break;

    case POS_TOP_LEFT:
      x = mon_info.rcWork.left;
      y = mon_info.rcWork.top;
      c_x = res_x / 2;
      c_y = res_y / 2;
      ShowWindow(hwnd, SW_RESTORE);
      return SetWindowPos(hwnd, HWND_TOP, x, y, c_x, c_y, SWP_NOZORDER);
      break;

    case POS_TOP_RIGHT:
      x = mon_info.rcWork.left + (res_x / 2);
      y = mon_info.rcWork.top;
      c_x = res_x / 2;
      c_y = res_y / 2;
      ShowWindow(hwnd, SW_RESTORE);
      return SetWindowPos(hwnd, HWND_TOP, x, y, c_x, c_y, SWP_NOZORDER);
      break;

    case POS_BOTTOM_LEFT:
      x = mon_info.rcWork.left;
      y = mon_info.rcWork.top + (res_y / 2);
      c_x = res_x / 2;
      c_y = res_y / 2;
      ShowWindow(hwnd, SW_RESTORE);
      return SetWindowPos(hwnd, HWND_TOP, x, y, c_x, c_y, SWP_NOZORDER);
 
      break;

    case POS_BOTTOM_RIGHT:
      x = mon_info.rcWork.left + (res_x / 2);
      y = mon_info.rcWork.top + (res_y / 2);
      c_x = res_x / 2;
      c_y = res_y / 2;
      ShowWindow(hwnd, SW_RESTORE);
      return SetWindowPos(hwnd, HWND_TOP, x, y, c_x, c_y, SWP_NOZORDER);
      break;

    default:
      return -1;
      break;
  }

  return -1;
}


int bring_to_mouse_pos(HWND hwnd) {
  POINT mouse_pos;
  if (GetCursorPos(&mouse_pos)) {
    RECT rect;
    GetWindowRect(hwnd, &rect);

    WINDOWPLACEMENT wndpl;
    wndpl.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(hwnd, &wndpl);

    int size_x = abs(rect.right - rect.left);
    int size_y = abs(rect.bottom - rect.top);

    ShowWindow(hwnd, SW_RESTORE); 
    return SetWindowPos(hwnd, HWND_TOP, mouse_pos.x, mouse_pos.y,
                        size_x, size_y, SWP_NOZORDER);
  }

  return -1;
}


int is_nod_update_hwnd(HWND hwnd) {
  char class_name[60];
  GetClassName(hwnd, class_name, 60);
  std::string hwnd_title = get_window_title(hwnd);

  if (strcmp(hwnd_title.c_str(), "ESET NOD32 Antivirus") == 0 &&
      strcmp(class_name,         "#32770"              ) == 0) {
    return TRUE;
  }

  return FALSE;
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  // printf("msg = %d\n", msg);

  HWND fghwnd = GetForegroundWindow();

  int result;
  result = -1;
  switch (msg)
  {
    case WM_CREATE:
      printf("msg = WM_CREATE\n");
      break;

    case WM_CLOSE:
      printf("msg = WM_CLOSE\n");
      DeregisterShellHookWindow(hwnd);
      unregister_hotkeys(hwnd);
      DestroyWindow(hwnd);
      break;

    case WM_DESTROY:
      printf("msg = WM_DESTROY\n");
      PostQuitMessage(WM_QUIT);
      break;

    case WM_HOTKEY:
      printf("msg = WM_HOTKEY : ");

      switch (wParam) {
        case KEY_MOVE_MAIN:
          printf("KEY_MOVE_MAIN\n");
          break;

        case KEY_EXIT:
          printf("KEY_EXIT\n");
          break;

        case KEY_MARGIN_LEFT:
          break;

        case KEY_IGNORE:
          break;

        case KEY_MOUSE_LOCK:
          break;

        case KEY_MAXIMIZE:
          printf("KEY_MAXIMIZE\n");
          result = maximize_hwnd(fghwnd);
          break;

        case KEY_MOVE_UP:
          break;

        case KEY_MOVE_DOWN:
          break;

        case KEY_DISP_CLASS:
          break;

        case KEY_TILE:
          break;

        case KEY_UNTILE:
          break;

        case KEY_INC_AREA:
          break;

        case KEY_DEC_AREA:
          break;

        case KEY_CLOSE_WIN:
          printf("KEY_CLOSE_WIN\n");
          result = PostMessage(fghwnd, WM_CLOSE, 0, 0);
          break;

        case KEY_BRING_TO_MOUSE:
          printf("KEY_BRING_TO_MOUSE\n");
          result = bring_to_mouse_pos(fghwnd);
          break;



        case KEY_HALVE_LEFT:
          printf("KEY_HALVE_LEFT\n");
          if (is_good_window(fghwnd))
            result = halve_hwnd(fghwnd, DIR_LEFT);
          break;

        case KEY_HALVE_RIGHT:
          printf("KEY_HALVE_RIGHT\n");
          if (is_good_window(fghwnd))
            result = halve_hwnd(fghwnd, DIR_RIGHT);
          break;

        case KEY_LEFT:
          printf("KEY_LEFT\n");
          if (is_good_window(fghwnd)) {
            if (GetKeyState(VK_UP) & 0x8000)
              result = snap_hwnd(fghwnd, POS_TOP_LEFT);
            else if (GetKeyState(VK_DOWN) & 0x8000)
              result = snap_hwnd(fghwnd, POS_BOTTOM_LEFT);
            else
              result = snap_hwnd(fghwnd, POS_LEFT);
          }
          break;

        case KEY_RIGHT:
          printf("KEY_RIGHT\n");
          if (is_good_window(fghwnd)) {
            if (GetKeyState(VK_UP) & 0x8000)
              result = snap_hwnd(fghwnd, POS_TOP_RIGHT);
            else if (GetKeyState(VK_DOWN) & 0x8000)
              result = snap_hwnd(fghwnd, POS_BOTTOM_RIGHT);
            else 
              result = snap_hwnd(fghwnd, POS_RIGHT);
          }
          break;

        case KEY_UP:
          printf("KEY_UP\n");
          if (is_good_window(fghwnd)) {
            if (GetKeyState(VK_LEFT) & 0x8000)
              result = snap_hwnd(fghwnd, POS_TOP_LEFT);
            else if (GetKeyState(VK_RIGHT) & 0x8000)
              result = snap_hwnd(fghwnd, POS_TOP_RIGHT);
            else 
              result = snap_hwnd(fghwnd, POS_TOP);
          }
          break;

        case KEY_DOWN:
          printf("KEY_DOWN\n");
          if (is_good_window(fghwnd)) {
            if (GetKeyState(VK_LEFT) & 0x8000)
              result = snap_hwnd(fghwnd, POS_BOTTOM_LEFT);
            else if (GetKeyState(VK_RIGHT) & 0x8000)
              result = snap_hwnd(fghwnd, POS_BOTTOM_RIGHT);
            else
              result = snap_hwnd(fghwnd, POS_BOTTOM);
          }
          break;

        case KEY_SAVE_HWNDS:
          printf("KEY_SAVE_HWNDS\n");
          result = save_positions_to_file();
          break;
          
        case KEY_RESTORE_HWNDS:
          printf("KEY_RESTORE_HWNDS\n");
          result = restore_positions_from_file();
          break;

        default:
          printf("UNKNOWN\n");
          break;
      }
      printf("result = %d\n", result);
      break;

    case WM_TIMER:
      switch (wParam) {
        case TIMER_UPDATE_MOUSE:
          break;
      }
      break;

    default:
      if (msg == shellhookid) {
        switch (wParam) {
          case HSHELL_WINDOWCREATED:
            printf("HSHELL_WINDOWCREATED : %d\n", lParam);
            if (is_nod_update_hwnd((HWND) lParam)) {
              printf("closing nod_update_hwnd\n");
              result = PostMessage((HWND) lParam, WM_CLOSE, 0, 0);
            }
            break;

          case HSHELL_WINDOWDESTROYED:
            printf("HSHELL_WINDOWDESTROYED : %d\n", lParam);
            break;

          case HSHELL_RUDEAPPACTIVATED:
            printf("HSHELL_RUDEAPPACTIVATED : %d\n", lParam);
            if (is_nod_update_hwnd((HWND) lParam)) {
              printf("closing nod_update_hwnd\n");
              result = PostMessage((HWND) lParam, WM_CLOSE, 0, 0);
            }
            break;

          case HSHELL_WINDOWACTIVATED:
            printf("HSHELL_WINDOWACTIVATED : %d\n", lParam);
            if (is_nod_update_hwnd((HWND) lParam)) {
              printf("closing nod_update_hwnd\n");
              result = PostMessage((HWND) lParam, WM_CLOSE, 0, 0);
            }
            break;
        }
      }
    else {
      return DefWindowProc(hwnd, msg, wParam, lParam);
    }
  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

void init_workspaces() {
  all_hwnds.clear();
  BOOL enumeratingWindowsSucceeded = ::EnumWindows(EnumWindowsProc, 0);

  // insert the already open windows into the proper monitor workspaces
  for (std::vector<HWND>::iterator iter = all_hwnds.begin();
       iter != all_hwnds.end();
       ++iter) {
    HWND hwnd = *iter;
    HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    ws_handler.add_hwnd_to_active_ws(hmon, hwnd);
  }
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine,
                   int nShowCmd) {
  WNDCLASSEX winClass = {};
  HWND hwnd;
  MSG msg;

  // command line
  LPWSTR *argv = NULL;
  int argc;
  int i;

  argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  LocalFree(argv);

  winClass.cbSize = sizeof(WNDCLASSEX);
  winClass.style = 0;
  winClass.lpfnWndProc = WndProc;
  winClass.cbClsExtra = 0;
  winClass.cbWndExtra = 0;
  winClass.hInstance = hInstance;
  winClass.hIcon = LoadIcon(0, IDI_WINLOGO);
  winClass.hIconSm = LoadIcon(0, IDI_WINLOGO);
  winClass.hCursor = LoadCursor(0, IDC_ARROW);
  winClass.hbrBackground = 0;
  winClass.lpszMenuName = 0;
  winClass.lpszClassName = NAME;

  if (!RegisterClassEx(&winClass)) {
    MessageBox(NULL,
               "Error Registering Window Class",
               "Error",
               MB_OK | MB_ICONERROR);
    return 0;
  }

  hwnd = CreateWindowEx(0,
                        NAME,
                        NAME,
                        WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        CW_USEDEFAULT,
                        HWND_MESSAGE,
                        NULL,
                        hInstance,
                        NULL);

  printf("hwnd = %d\n", hwnd);
  if (hwnd == NULL) {
    MessageBox(NULL,
               "Error Creating Window",
               "Error",
               MB_OK | MB_ICONERROR);
    return 0;
  }

  // set defaults for screen
  if (!screen_x && !screen_y && !screen_width && !screen_height) {
    RECT workarea;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &workarea, 0);
    screen_x = workarea.left;
    screen_y = workarea.top;
    screen_width = workarea.right - workarea.left;
    screen_height = workarea.bottom - workarea.top;
  }

  register_hotkeys(hwnd);

  EnumWindows(EnumWindowsRestore, 0);
  EnumWindows(EnumWindowsProc, 0);

  // get function pointer for RegisterShellHookWindow
  if (RegisterShellHookWindow_ == NULL) {
    RegisterShellHookWindow_ =
     (BOOL (__stdcall*)(HWND)) GetProcAddress(GetModuleHandle("USER32.DLL"),
                                              "RegisterShellHookWindow");
    if (RegisterShellHookWindow_ == NULL) {
      MessageBox(NULL,
                 "Could not find RegisterShellHookWindow",
                 "Error",
                 MB_OK | MB_ICONERROR);
      return 0;
    }
  }

  RegisterShellHookWindow_(hwnd);
  shellhookid = RegisterWindowMessage("SHELLHOOK");

  // populate the workspaces with all the currently open windows
  init_workspaces();

  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return msg.wParam;
}
