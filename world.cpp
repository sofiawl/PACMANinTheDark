#include <cstdio>
#include <random>
#include <algorithm>
#include <fstream>
#include <ncurses.h>
#include "world.h"

void init_world(char world[SIZE_WORLD][SIZE_WORLD]){
    for (int i=0; i<SIZE_WORLD; i++) {
      world[i][0] = 'X';
      world[i][SIZE_WORLD-1] = 'X';
      world[0][i] = 'X';
      world[SIZE_WORLD-1][i] = 'X';
    }

    for (int i=1; i<SIZE_WORLD-1; i++) {
      for (int j=1; j<SIZE_WORLD-1; j++) {
        world[i][j] = '0';
      }
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    world[(SIZE_WORLD-1)/2][(SIZE_WORLD-1)/2] = 'P';

    // build list of allowed flattened positions (a vector of a matrix), exclude center
    std::vector<std::size_t> allowed;
    allowed.reserve(SIZE_WORLD*SIZE_WORLD - 1); // the allowed positions, does not include the wall
    for (std::size_t i = 0; i < SIZE_WORLD; ++i) {
      for (std::size_t j = 0; j < SIZE_WORLD; ++j) {
        if (!(i == (SIZE_WORLD-1)/2 && j == (SIZE_WORLD-1)/2)) // reserve center, PACMAN position [19][19]
          allowed.push_back(i * SIZE_WORLD + j);
      }
    }

    std::shuffle(allowed.begin(), allowed.end(), gen);

    auto pos0 = allowed[0];
    auto pos1 = allowed[1];
    auto pos2 = allowed[2];
    auto pos3 = allowed[3];

    // pos / SIZE_WORLD gives the row, pos % SIZE_WORLD gives the column
    /* Example:
     * SIZE_WORLD == 40 and pos == 123, then
     * row = 123 / 40 = 3
     * col = 123 % 40 = 3 so world[3][3] is set
    */
    world[pos0 / SIZE_WORLD][pos0 % SIZE_WORLD] = 'R';
    world[pos1 / SIZE_WORLD][pos1 % SIZE_WORLD] = 'B';
    world[pos2 / SIZE_WORLD][pos2 % SIZE_WORLD] = 'G';
    world[pos3 / SIZE_WORLD][pos3 % SIZE_WORLD] = 'Y';

    // save world to mundo.csv
    std::ofstream mundo_file("mundo.csv", std::ios::trunc);
    if (!mundo_file.is_open()) return;

    for (int i = 0; i < SIZE_WORLD; ++i) {
      mundo_file.write(world[i], SIZE_WORLD);
      mundo_file.put('\n');
    }
}

void draw_world_ncurses_from_csv(const char* csv_path) {
    std::ifstream in(csv_path, std::ios::binary);
    if (!in.is_open()) return;

    char world[SIZE_WORLD][SIZE_WORLD];
    for (int i = 0; i < SIZE_WORLD; ++i) {
        int j = 0;
        char c = '\0';
        while (j < SIZE_WORLD && in.get(c)) {
            if (c == '\n' || c == '\r') continue;
            world[i][j] = c;
            ++j;
        }
        while (j < SIZE_WORLD) {
            world[i][j] = '0';
            ++j;
        }
    }

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);

    // Plain text only: 40 columns wide, 41 rows tall (40 map + 1 hint), centered as one block.
    const int grid_w = SIZE_WORLD;
    const int grid_h = SIZE_WORLD + 1;

    while (true) {
        int rows = 0, cols = 0;
        getmaxyx(stdscr, rows, cols);

        clear();

        if (cols >= grid_w && rows >= grid_h) {
            int base_y = (rows - grid_h) / 2;
            int base_x = (cols - grid_w) / 2;

            for (int i = 0; i < SIZE_WORLD; ++i) {
                for (int j = 0; j < SIZE_WORLD; ++j) {
                    char c = world[i][j];
                    if (c == '0') c = ' ';
                    mvaddch(base_y + i, base_x + j, static_cast<unsigned char>(c));
                }
            }

            char hint[grid_w + 1];
            snprintf(hint, sizeof(hint), "%-*s", grid_w, "q=quit");
            mvprintw(base_y + SIZE_WORLD, base_x, "%s", hint);
        } else {
            mvprintw(
                rows / 2,
                0,
                "Terminal too small: need at least %d cols x %d rows.",
                grid_w,
                grid_h);
        }

        refresh();

        timeout(150);
        int key = getch();
        timeout(-1);
        if (key == 'q' || key == 'Q') break;
    }

    endwin();
}
