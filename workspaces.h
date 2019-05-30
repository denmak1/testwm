#include <windows.h>
#include <vector>
#include <map>
#include <string>

#define NUM_WORKSPACES 4

using namespace std;

typedef struct {
  int ws_id;
  string mon_name;
  vector<HWND> hwnds;
} ws_t;

typedef vector<HMONITOR> mon_list_t;
typedef vector<MONITORINFO> moninfo_list_t;
typedef vector<ws_t> ws_list_t;

typedef map<string, ws_list_t> mon_to_ws_list_map_t;
typedef map<string, ws_t> mon_to_active_ws_map_t;

class workspace_handler_t {

public:
  workspace_handler_t();

  void add_hwnd_to_active_ws(HMONITOR hmon, HWND hwnd);
  int remove_hwnd_from_active_ws(HWND hwnd);

  ws_t get_active_ws(HMONITOR hmon);
  void set_active_ws(HMONITOR hmon, ws_t ws);

private:
  mon_to_ws_list_map_t ws_list_map;
  mon_to_active_ws_map_t active_ws_map;
};
