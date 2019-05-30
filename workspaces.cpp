#include "workspaces.h"
#include <stdlib.h>
#include <stdio.h>

using namespace std;

static mon_list_t mon_list;
static moninfo_list_t moninfo_list;

BOOL CALLBACK MonInfoEnumProc(HMONITOR hmon, HDC hdcmon,
                              LPRECT lprcmon, LPARAM dw) {
  MONITORINFOEX mi;
  ZeroMemory(&mi, sizeof(mi));
  mi.cbSize = sizeof(mi);
  GetMonitorInfo(hmon, &mi);
  printf("  DisplayDevice: %s\n", mi.szDevice);

  // get monitor info
  MONITORINFO moninfo;
  moninfo.cbSize = sizeof(MONITORINFO);
  GetMonitorInfo(hmon, &moninfo);

  mon_list.push_back(hmon);
  moninfo_list.push_back(moninfo);

  return TRUE;
}

static void populate_mon_list() {
  printf("EnumDisplayDevices\n");

  DISPLAY_DEVICE dd;
  ZeroMemory(&dd, sizeof(dd));
  dd.cb = sizeof(dd);

  for (int i = 0; EnumDisplayDevices(NULL, i, &dd, 0); i++) {
    printf("Device %d:\n", i);
    printf("  DeviceName:   '%s'\n", dd.DeviceName);
    printf("  DeviceString: '%s'\n", dd.DeviceString);
    printf("  StateFlags:   %s%s%s%s\n",
           ((dd.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) ?
            "desktop " : ""),
           ((dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) ?
            "primary " : ""),
           ((dd.StateFlags & DISPLAY_DEVICE_VGA_COMPATIBLE) ?
            "vga "     : ""),
           ((dd.StateFlags & DISPLAY_DEVICE_MULTI_DRIVER) ?
            "multi "   : ""),
           ((dd.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER) ?
            "mirror "  : ""));

    DISPLAY_DEVICE dd2;
    ZeroMemory(&dd2, sizeof(dd2));
    dd2.cb = sizeof(dd2);
    EnumDisplayDevices(dd.DeviceName, 0, &dd2, 0);
    printf("  DeviceID: '%s'\n", dd2.DeviceID);
    printf("  Monitor Name: '%s'\n", dd2.DeviceString);
  }

  printf("EnumDisplayMonitors\n");
  EnumDisplayMonitors(NULL, NULL, MonInfoEnumProc, 0);
}

static string get_mon_name(HMONITOR hmon) {
  MONITORINFOEX mi;
  ZeroMemory(&mi, sizeof(mi));
  mi.cbSize = sizeof(mi);
  GetMonitorInfo(hmon, &mi);
  return string(mi.szDevice);
}

static bool monitorcmp(HMONITOR hmon1, HMONITOR hmon2) {
  printf("%s == %s", get_mon_name(hmon1).c_str(),
                     get_mon_name(hmon2).c_str());
  return get_mon_name(hmon1).compare(get_mon_name(hmon2)) == 0;
}

workspace_handler_t::workspace_handler_t() {
  // initialize ws arrays
  printf("in workspace_handler_t ctor\n");

  populate_mon_list();

  // iterate over monitors and initialize maps
  for (int i = 0; i < mon_list.size(); i++) {
    string mon_name = get_mon_name(mon_list.at(i));

    // create 4 ws's per monitor
    for (int j = 0; j < NUM_WORKSPACES; j++) {
      ws_t ws = {j, mon_name, std::vector<HWND>()};
      ws_list_map[mon_name].push_back(ws);
    }

    // initialize active ws to 0 for each
    active_ws_map[mon_name] = ws_list_map[mon_name].at(0);
  }
}

ws_t workspace_handler_t::get_active_ws(HMONITOR hmon) {
  string mon_name = get_mon_name(hmon);
  return active_ws_map.find(mon_name)->second;
}

void workspace_handler_t::set_active_ws(HMONITOR hmon, ws_t ws) {
  string mon_name = get_mon_name(hmon);

  // hide windows in the current ws
  ws_t curr_active_ws = get_active_ws(hmon);
  for (vector<HWND>::iterator it = curr_active_ws.hwnds.begin();
       it != curr_active_ws.hwnds.end();
       it++) {
    HWND hwnd = *it;
    ShowWindow(hwnd, SW_HIDE);
  }

  // update active ws to the ws
  active_ws_map[mon_name] = ws;

  // show windows in the new ws
  ws_t new_active_ws = get_active_ws(hmon);
  for (vector<HWND>::iterator it = new_active_ws.hwnds.begin();
       it != new_active_ws.hwnds.end();
       it++) {
    HWND hwnd = *it;
    ShowWindow(hwnd, SW_SHOW);
  }
}

void workspace_handler_t::add_hwnd_to_active_ws(HMONITOR hmon, HWND hwnd) {
  ws_t ws = get_active_ws(hmon);
  ws.hwnds.push_back(hwnd);
}

int workspace_handler_t::remove_hwnd_from_active_ws(HWND hwnd) {
  mon_to_ws_list_map_t::iterator m_it;
  for (m_it = ws_list_map.begin(); m_it != ws_list_map.end(); m_it++) {
    ws_list_t ws_list = m_it->second;

    ws_list_t::iterator w_it;
    for (w_it = ws_list.begin(); w_it != ws_list.end(); w_it++) {
      ws_t ws = *w_it;

      vector<HWND>::iterator h_it;
      for (h_it = ws.hwnds.begin(); h_it != ws.hwnds.end(); h_it++) {
        // if found match, break out of loop and erase
        if (*h_it == hwnd)
          break;
      }

      if (h_it != ws.hwnds.end()) {
        ws.hwnds.erase(h_it);
        return 0;
      }
    }
  }

  return -1;
}
