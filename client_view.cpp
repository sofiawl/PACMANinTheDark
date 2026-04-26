#include "client_view.h"
#include "world.h"

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <ncursesw/ncurses.h>
#include <sys/ioctl.h>
#include <unistd.h>

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

void draw_client_view_from_csv(const char* csv_path) {
    char world[SIZE_WORLD][SIZE_WORLD];
    if (!load_world_csv(csv_path, world)) {
        std::fprintf(stderr, "Could not load %s\n", csv_path);
        return;
    }
    constexpr int hint_rows = 1;

    setlocale(LC_ALL, "");  // ← enables UTF-8 in ncurses
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

    // 2 world rows per terminal row via half-blocks
    const int term_rows_needed = SIZE_WORLD / 2;  // 20
    const int term_cols_needed = SIZE_WORLD;       // 40

    if (scr_h - hint_rows < term_rows_needed || scr_w < term_cols_needed) {
        endwin();
        std::fprintf(stderr, "Terminal too small: need %dx%d, have %dx%d\n",
            term_cols_needed, term_rows_needed + hint_rows, scr_w, scr_h);
        return;
    }

    // each world cell is 1 terminal row tall (via half-block) and cell_w cols wide
    const int cell_w = scr_w / SIZE_WORLD;
    const int win_h  = term_rows_needed + hint_rows;  // 21
    const int win_w  = SIZE_WORLD * cell_w;
    const int origin_y = (scr_h - win_h) / 2;
    const int origin_x = (scr_w - win_w) / 2;

    // build a color pair for each unique (top, bottom) combo on demand
    // we use a simple map: pair index stored in a 2D array keyed by char
    // chars we expect: 'X','G','B','R','P','Y',' ' → max ~7 chars → 49 pairs
    static const char CHARS[] = " XGBRPY";
    constexpr int NC = sizeof(CHARS) - 1;
    // pair[top_idx][bot_idx] → color pair number (1-based)
    short pair_id[NC][NC];
    memset(pair_id, 0, sizeof(pair_id));
    int next_pair = 1;

    auto char_color = [](char c) -> short {
        switch (c) {
            case 'X': return COLOR_WHITE;
            case 'G': return COLOR_GREEN;
            case 'B': return COLOR_BLUE;
            case 'R': return COLOR_RED;
            case 'P': return COLOR_MAGENTA;
            case 'Y': return COLOR_YELLOW;
            case ' ': return COLOR_BLACK;  // ← explicit black for empty space
            default:  return COLOR_BLACK;
        }
    };
    auto char_idx = [&](char c) -> int {
        for (int k = 0; k < NC; ++k) if (CHARS[k] == c) return k;
        return 0;
    };

    WINDOW* box = newwin(win_h, win_w, origin_y, origin_x);
    if (!box) { endwin(); std::fprintf(stderr, "newwin failed\n"); return; }
    keypad(box, TRUE);
    werase(box);

    for (int i = 0; i < SIZE_WORLD; i += 2) {
        int term_row = i / 2;
        for (int j = 0; j < SIZE_WORLD; ++j) {
            char top = world[i][j];
            char bot = (i + 1 < SIZE_WORLD) ? world[i + 1][j] : '0';
            if (top == '0') top = ' ';
            if (bot == '0') bot = ' ';

            int ti = char_idx(top);
            int bi = char_idx(bot);
            if (!pair_id[ti][bi]) {
                // ▀ = upper half block: foreground = top color, background = bot color
                init_pair(next_pair, char_color(top), char_color(bot));
                pair_id[ti][bi] = next_pair++;
            }

            cchar_t wch;
            wchar_t wstr[2] = { L'▀', L'\0' };
            setcchar(&wch, wstr, A_NORMAL, pair_id[ti][bi], nullptr);
            for (int dc = 0; dc < cell_w; ++dc) {
                mvwadd_wch(box, term_row, j * cell_w + dc, &wch);
            }
        }
    }

    const char* msg = "q=quit";
    int col = (win_w - (int)std::strlen(msg)) / 2;
    if (col < 0) col = 0;
    mvwprintw(box, term_rows_needed, col, "%s", msg);

    wrefresh(box);
    int key = 0;
    while ((key = wgetch(box)) != 'q' && key != 'Q') {}
    delwin(box);
    endwin();
}
