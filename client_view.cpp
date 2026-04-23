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

    const int map_budget_rows = scr_h - hint_rows;
    if (scr_w < 1 || map_budget_rows < 1) {
        endwin();
        std::fprintf(
            stderr,
            "Terminal too small: need at least 1 column and 2 rows (have %d x %d)\n",
            scr_w,
            scr_h);
        return;
    }

    // Largest square (side×side world cells), each drawn as cell×cell terminal chars,
    // center-cropped from the 40×40 world. Fits any terminal that can show at least 1×1 + hint.
    const int max_side = std::min(SIZE_WORLD, std::min(scr_w, map_budget_rows));
    int best_side = 1;
    int best_cell = 1;
    for (int side = max_side; side >= 1; --side) {
        const int cell = std::min(scr_w / side, map_budget_rows / side);
        if (cell < 1) continue;
        const int area = side * cell;
        if (area > best_side * best_cell) {
            best_side = side;
            best_cell = cell;
        }
    }

    const int off_r = (SIZE_WORLD - best_side) / 2;
    const int off_c = (SIZE_WORLD - best_side) / 2;

    const int square_side = best_side * best_cell;
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
    for (int i = 0; i < best_side; ++i) {
        for (int j = 0; j < best_side; ++j) {
            char ch = world[off_r + i][off_c + j];
            if (ch == '0') ch = ' ';
            const int base_r = i * best_cell;
            const int base_c = j * best_cell;
            for (int dr = 0; dr < best_cell; ++dr) {
                for (int dc = 0; dc < best_cell; ++dc) {
                    mvwaddch(box, base_r + dr, base_c + dc, static_cast<unsigned char>(ch));
                }
            }
        }
    }

    const char* msg = (best_side < SIZE_WORLD) ? "q=quit (cropped)" : "q=quit";
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
