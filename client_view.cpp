#include "client_view.h"
#include "world.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <ncursesw/ncurses.h>
#include <sys/ioctl.h>
#include <unistd.h>

static WINDOW* box_win = nullptr;  // kept alive across calls
static int s_cell_h = 0;
static int s_cell_w = 0;
static int s_view_side = 0;

static bool is_pill_char(char c) {
    return c >= '1' && c <= '6';
}

static short color_for(char c) {
    switch (c) {
        case 'X': return COLOR_WHITE;
        case 'G': return COLOR_GREEN;
        case 'B': return COLOR_BLUE;
        case 'R': return COLOR_RED;
        case 'P': return COLOR_YELLOW;
        case 'Y': return COLOR_MAGENTA;
        case '1': case '2': return 208;  // .txt -> orange
        case '3': case '4': return 129;  // .jpg -> purple
        case '5': case '6': return COLOR_CYAN;  // .mp4 -> light brown
        case ' ': return COLOR_BLACK;
        default:  return COLOR_BLACK;
    }
}

static short attr_for(char c) {
    return is_pill_char(c) ? (short)A_BOLD : A_NORMAL;
}

// Pills render on the left terminal column only (half the width of Pac-Man/ghosts).
static short color_for_half(char c, int dc) {
    if (c == ' ') return -1;
    if (is_pill_char(c)) return dc == 0 ? color_for(c) : -1;
    return color_for(c);
}

static short get_color_pair(short fg, short bg, int& next_pair) {
    static std::vector<std::pair<std::pair<short, short>, short>> cache;
    for (const auto& entry : cache) {
        if (entry.first.first == fg && entry.first.second == bg) return entry.second;
    }
    init_pair(next_pair, fg, bg);
    cache.push_back({{fg, bg}, (short)next_pair});
    return next_pair++;
}

static void draw_half_block(WINDOW* win, int term_row, int col, wchar_t glyph,
    char top, char bot, int dc, int& next_pair) {
    const short fg = color_for_half(top, dc);
    const short bg = color_for_half(bot, dc);
    if (fg == -1 && bg == -1) return;
    const short pair = get_color_pair(fg, bg, next_pair);
    cchar_t wch;
    wchar_t ws[2] = {glyph, L'\0'};
    setcchar(&wch, ws, attr_for(top) | attr_for(bot), pair, nullptr);
    mvwadd_wch(win, term_row, col, &wch);
}

static bool load_world_csv(const char* csv_path, char world[SIZE_WORLD][SIZE_WORLD]) {
    std::ifstream in(csv_path);
    if (!in.is_open()) return false;

    std::string line;
    for (int i = 0; i < SIZE_WORLD; ++i) {
        if (!std::getline(in, line)) return false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        for (int j = 0; j < SIZE_WORLD; ++j) {
            world[i][j] = (j < (int)line.size()) ? line[j] : '0';
        }
    }
    return true;
}

static void query_screen_size(int* scr_h, int* scr_w) {
    getmaxyx(stdscr, *scr_h, *scr_w);
    if (*scr_h > 2 && *scr_w > 2) return;
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        *scr_h = ws.ws_row;
        *scr_w = ws.ws_col;
        return;
    }
    const char* cs = std::getenv("COLUMNS");
    const char* ls = std::getenv("LINES");
    if (cs && ls) {
        *scr_w = std::max(1, std::atoi(cs));
        *scr_h = std::max(1, std::atoi(ls));
    }
}

// call once at startup
void init_client_view() {
    setlocale(LC_ALL, "");
    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);
    start_color();
    use_default_colors();
    refresh();

    int scr_h = 0, scr_w = 0;
    query_screen_size(&scr_h, &scr_w);

    constexpr int hint_rows = 1;
    s_cell_h = 1;
    s_cell_w = 2;
    s_view_side = std::min({SIZE_WORLD, 2 * (scr_h - hint_rows), scr_w / s_cell_w});

    if (s_view_side < 1) {
        endwin();
        fprintf(stderr, "Terminal too small\n");
        return;
    }

    const int win_h = s_view_side / 2 + hint_rows;
    const int win_w = s_view_side * s_cell_w;
    const int origin_y = (scr_h - win_h) / 2;
    const int origin_x = (scr_w - win_w) / 2;

    box_win = newwin(win_h, win_w, origin_y, origin_x);
    keypad(box_win, TRUE);
    wtimeout(box_win, -1);  // block until the player presses a key
}

// call once at shutdown
void close_client_view() {
    if (box_win) {
        delwin(box_win);
        box_win = nullptr;
    }
    endwin();
}

// draws the world and returns the key pressed
int draw_client_view_and_get_key(const char* csv_path, std::pair<int,int> pacman_coord, int radius) {
    if (!box_win) return 'q';

    char world[SIZE_WORLD][SIZE_WORLD];
    if (!load_world_csv(csv_path, world)) {
        fprintf(stderr, "Could not load %s\n", csv_path);
        return ERR;
    }

    static int next_pair = 1;

    werase(box_win);

    for (int i = 0; i < s_view_side; i += 2) {
        int term_row = i / 2;
        for (int j = 0; j < s_view_side; ++j) {
            bool top_vis = abs(i   - pacman_coord.first) <= radius && abs(j - pacman_coord.second) <= radius;
            bool bot_vis = abs(i+1 - pacman_coord.first) <= radius && abs(j - pacman_coord.second) <= radius;

            // Case 1: both hidden: skip (werase already cleared to terminal background)
            if (!top_vis && !bot_vis) continue;

            // Case 2: only top visible: upper-half block, bot half = terminal background
            if (top_vis && !bot_vis) {
                char c = world[i][j]; if (c == '0') c = ' ';
                const int cols = is_pill_char(c) ? 1 : s_cell_w;
                for (int dc = 0; dc < cols; ++dc)
                    draw_half_block(box_win, term_row, j * s_cell_w + dc, L'▀', c, ' ', dc, next_pair);
                continue;
            }

            // Case 3: only bot visible: lower-half block, top half = terminal background
            if (!top_vis && bot_vis) {
                char c = (i+1 < s_view_side) ? world[i+1][j] : '0'; if (c == '0') c = ' ';
                const int cols = is_pill_char(c) ? 1 : s_cell_w;
                for (int dc = 0; dc < cols; ++dc)
                    draw_half_block(box_win, term_row, j * s_cell_w + dc, L'▄', ' ', c, dc, next_pair);
                continue;
            }

            // Case 4: both visible: upper-half block fg/bg pair logic
            char top = world[i][j];
            char bot = (i+1 < s_view_side) ? world[i+1][j] : '0';
            if (top == '0') top = ' ';
            if (bot == '0') bot = ' ';
            for (int dc = 0; dc < s_cell_w; ++dc)
                draw_half_block(box_win, term_row, j * s_cell_w + dc, L'▀', top, bot, dc, next_pair);
        }
    }

    const char* msg = "q=quit";
    int msg_col = (s_view_side * s_cell_w - (int)strlen(msg)) / 2;
    if (msg_col < 0) msg_col = 0;
    mvwprintw(box_win, s_view_side / 2, msg_col, "%s", msg);

    wrefresh(box_win);

    // wait for keypress
    return wgetch(box_win);
}
