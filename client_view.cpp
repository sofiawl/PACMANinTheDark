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

    const int map_w = SIZE_WORLD;
    const int map_h = SIZE_WORLD;

    initscr();
    noecho();
    cbreak();
    curs_set(0);
    keypad(stdscr, TRUE);

    int scr_h = 0, scr_w = 0;
    getmaxyx(stdscr, scr_h, scr_w);

    if (scr_w < map_w || scr_h < map_h) {
        endwin();
        std::fprintf(
            stderr,
            "Terminal too small: need at least %d cols x %d rows (have %d x %d)\n",
            map_w,
            map_h,
            scr_w,
            scr_h);
        return;
    }

    const int hint_h = (scr_h >= map_h + 1) ? 1 : 0;
    const int win_w = map_w;
    const int win_h = map_h + hint_h;

    const int origin_y = (scr_h - win_h) / 2;
    const int origin_x = (scr_w - win_w) / 2;

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
            mvwaddch(box, i, j, static_cast<unsigned char>(ch));
        }
    }

    if (hint_h) {
        const char* msg = "q=quit";
        const int msglen = (int)std::strlen(msg);
        int col = (win_w - msglen) / 2;
        if (col < 0) col = 0;
        mvwprintw(box, map_h, col, "%s", msg);
    }

    wrefresh(box);
    refresh();

    int k = 0;
    while ((k = wgetch(box)) != 'q' && k != 'Q') {}

    delwin(box);
    endwin();
}
