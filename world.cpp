#include <cstdio>
#include <cstdlib>
#include <random>
#include <algorithm>
#include <fstream>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include "world.h"

static void terminal_size(int* rows, int* cols) {
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row > 0 && ws.ws_col > 0) {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return;
    }
    const char* ce = std::getenv("COLUMNS");
    const char* li = std::getenv("LINES");
    *cols = (ce && std::atoi(ce) > 0) ? std::atoi(ce) : 80;
    *rows = (li && std::atoi(li) > 0) ? std::atoi(li) : 24;
}

static void print_padx(int pad_x) {
    for (int x = 0; x < pad_x; ++x) putchar(' ');
}

static void wait_for_q() {
    fflush(stdout);
    struct termios oldt {};
    if (tcgetattr(STDIN_FILENO, &oldt) != 0) {
        (void)getchar();
        return;
    }
    struct termios newt = oldt;
    newt.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &newt) != 0) {
        (void)getchar();
        (void)tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
        return;
    }
    int c = 0;
    while ((c = getchar()) != 'q' && c != 'Q') {}
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

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

void draw_world_terminal_from_csv(const char* csv_path) {
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

    const int grid_w = SIZE_WORLD;
    const int grid_h = SIZE_WORLD + 1;

    int term_rows = 0, term_cols = 0;
    terminal_size(&term_rows, &term_cols);

    // Clear screen + home cursor: one character per cell, no ncurses width quirks.
    std::fputs("\033[2J\033[H", stdout);

    if (term_cols < grid_w || term_rows < grid_h) {
        std::fprintf(
            stderr,
            "Terminal too small: need at least %d columns x %d rows (got %d x %d).\n",
            grid_w,
            grid_h,
            term_cols,
            term_rows);
        std::fprintf(stderr, "Press q to exit. ");
        wait_for_q();
        return;
    }

    int pad_x = (term_cols - grid_w) / 2;
    int pad_y = (term_rows - grid_h) / 2;
    if (pad_x < 0) pad_x = 0;
    if (pad_y < 0) pad_y = 0;

    for (int y = 0; y < pad_y; ++y) putchar('\n');

    for (int i = 0; i < SIZE_WORLD; ++i) {
        print_padx(pad_x);
        for (int j = 0; j < SIZE_WORLD; ++j) {
            char ch = world[i][j];
            if (ch == '0') ch = ' ';
            putchar(ch);
        }
        putchar('\n');
    }

    char hint[grid_w + 1];
    std::snprintf(hint, sizeof(hint), "%-*s", grid_w, "q=quit");
    print_padx(pad_x);
    std::fputs(hint, stdout);
    putchar('\n');

    wait_for_q();
}
