#include "client_view.h"
#include "world.h"

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

    // Fixed viewport in terminal character cells 
    constexpr int VIEW_W = 40;
    constexpr int VIEW_H = 40;
    constexpr int CELL = 1;

    const int MAP_W = SIZE_WORLD * CELL;
    const int MAP_H = SIZE_WORLD * CELL;

    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);

    int scr_h = 0, scr_w = 0;
    getmaxyx(stdscr, scr_h, scr_w);

    if (scr_w < VIEW_W || scr_h < VIEW_H) {
        endwin();
        std::fprintf(
            stderr,
            "Terminal too small: need at least %d cols x %d rows (have %d x %d)\n",
            VIEW_W,
            VIEW_H,
            scr_w,
            scr_h);
        return;
    }

    const int origin_y = (scr_h - VIEW_H) / 2;
    const int origin_x = (scr_w - VIEW_W) / 2;

    erase();
    WINDOW* box = newwin(VIEW_H, VIEW_W, origin_y, origin_x);
    if (!box) {
        endwin();
        std::fprintf(stderr, "newwin failed\n");
        return;
    }

    const int mx = (VIEW_W - MAP_W) / 2;
    const int my = (VIEW_H - MAP_H) / 2;

    werase(box);
    for (int i = 0; i < SIZE_WORLD; ++i) {
        for (int j = 0; j < SIZE_WORLD; ++j) {
            char ch = world[i][j];
            if (ch == '0') ch = ' ';
            const int base_r = my + i * CELL;
            const int base_c = mx + j * CELL;
            for (int dr = 0; dr < CELL; ++dr) {
                for (int dc = 0; dc < CELL; ++dc) {
                    mvwaddch(box, base_r + dr, base_c + dc, static_cast<unsigned char>(ch));
                }
            }
        }
    }

    const char* msg = "Press q to quit";
    const int msglen = (int)std::strlen(msg);
    int col = (VIEW_W - msglen) / 2;
    if (col < 0) col = 0;
    mvwprintw(box, VIEW_H - 1, col, "%s", msg);

    wrefresh(box);
    refresh();

    int k = 0;
    while ((k = wgetch(box)) != 'q' && k != 'Q') {}

    delwin(box);
    endwin();
}
