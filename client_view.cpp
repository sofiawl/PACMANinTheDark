#include "client_view.h"
#include "world.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <ncurses.h>

static bool load_world_csv(const char* csv_path, char world[SIZE_WORLD][SIZE_WORLD]) {
    std::ifstream in(csv_path, std::ios::binary);
    if (!in.is_open()) return false;

    for (int i = 0; i < SIZE_WORLD; ++i) {
        int j = 0;
        char c = '\0';
        while (j < SIZE_WORLD && in.get(c)) {
            if (c == '\n' || c == '\r') continue;
            world[i][j] = c;
            ++j;
        }
        while (j < SIZE_WORLD) world[i][j++] = '0';
    }
    return true;
}

void draw_client_view_from_csv(const char* csv_path) {
    char world[SIZE_WORLD][SIZE_WORLD];
    if (!load_world_csv(csv_path, world)) {
        std::fprintf(stderr, "Could not open %s\n", csv_path);
        return;
    }

    constexpr int hint_rows = 1;

    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);

    int scr_h = 0, scr_w = 0;
    getmaxyx(stdscr, scr_h, scr_w);

    // Each world cell is cell×cell terminal characters so the map is always a square
    // of side (SIZE_WORLD * cell) columns and the same number of rows.
    int cell = std::min(scr_w / SIZE_WORLD, (scr_h - hint_rows) / SIZE_WORLD);
    if (cell < 1) {
        endwin();
        std::fprintf(
            stderr,
            "Terminal too small: need at least %d cols x %d rows (have %d x %d)\n",
            SIZE_WORLD,
            SIZE_WORLD + hint_rows,
            scr_w,
            scr_h);
        return;
    }

    const int square_side = SIZE_WORLD * cell;
    const int win_w = square_side;
    const int win_h = square_side + hint_rows;

    const int origin_x = (scr_w - win_w) / 2;
    const int origin_y = (scr_h - win_h) / 2;

    erase();
    WINDOW* box = newwin(win_h, win_w, origin_y, origin_x);
    if (!box) {
        endwin();
        std::fprintf(stderr, "newwin failed\n");
        return;
    }

    werase(box);
    for (int i = 0; i < SIZE_WORLD; ++i) {
        for (int j = 0; j < SIZE_WORLD; ++j) {
            char ch = world[i][j];
            if (ch == '0') ch = ' ';
            const int base_r = i * cell;
            const int base_c = j * cell;
            for (int dr = 0; dr < cell; ++dr) {
                for (int dc = 0; dc < cell; ++dc) {
                    mvwaddch(box, base_r + dr, base_c + dc, static_cast<unsigned char>(ch));
                }
            }
        }
    }

    const char* msg = "q=quit";
    const int msglen = (int)std::strlen(msg);
    int col = (win_w - msglen) / 2;
    if (col < 0) col = 0;
    mvwprintw(box, square_side, col, "%s", msg);

    wrefresh(box);
    refresh();

    int key = 0;
    while ((key = wgetch(box)) != 'q' && key != 'Q') {}

    delwin(box);
    endwin();
}
