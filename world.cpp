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

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_BLUE, COLOR_BLACK);
        init_pair(2, COLOR_YELLOW, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
        init_pair(4, COLOR_CYAN, COLOR_BLACK);
        init_pair(5, COLOR_GREEN, COLOR_BLACK);
        init_pair(6, COLOR_MAGENTA, COLOR_BLACK);
    }

    clear();
    for (int i = 0; i < SIZE_WORLD; ++i) {
        for (int j = 0; j < SIZE_WORLD; ++j) {
            char tile = world[i][j];
            switch (tile) {
                case 'X':
                    attron(COLOR_PAIR(1));
                    mvaddch(i, j, '#');
                    attroff(COLOR_PAIR(1));
                    break;
                case 'P':
                    attron(COLOR_PAIR(2));
                    mvaddch(i, j, 'P');
                    attroff(COLOR_PAIR(2));
                    break;
                case 'R':
                    attron(COLOR_PAIR(3));
                    mvaddch(i, j, 'R');
                    attroff(COLOR_PAIR(3));
                    break;
                case 'B':
                    attron(COLOR_PAIR(4));
                    mvaddch(i, j, 'B');
                    attroff(COLOR_PAIR(4));
                    break;
                case 'G':
                    attron(COLOR_PAIR(5));
                    mvaddch(i, j, 'G');
                    attroff(COLOR_PAIR(5));
                    break;
                case 'Y':
                    attron(COLOR_PAIR(6));
                    mvaddch(i, j, 'Y');
                    attroff(COLOR_PAIR(6));
                    break;
                case '0':
                    mvaddch(i, j, ' ');
                    break;
                default:
                    mvaddch(i, j, tile);
                    break;
            }
        }
    }

    mvprintw(SIZE_WORLD + 1, 0, "Press q to quit.");
    refresh();
    while (getch() != 'q') {}
    endwin();
}
