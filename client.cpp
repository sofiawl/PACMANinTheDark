#include <stdio.h>
#include <sys/socket.h>
#include <ncurses.h>

#include "protocol.h"
#include "world.h"

// get this information in bash with: ip link show
const char* network_interface_pc1sofia = "enp4s0";

static void draw_world_ncurses(FILE* in) {
    char world[SIZE_WORLD][SIZE_WORLD];

    for (int i = 0; i < SIZE_WORLD; ++i) {
        int j = 0;
        int c = 0;
        while (j < SIZE_WORLD && (c = fgetc(in)) != EOF) {
            if (c == '\n' || c == '\r') continue;
            world[i][j] = (char)c;
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
        init_pair(1, COLOR_BLUE, COLOR_BLACK);     // walls
        init_pair(2, COLOR_YELLOW, COLOR_YELLOW);   // pacman
        init_pair(3, COLOR_RED, COLOR_RED);      // ghost R
        init_pair(4, COLOR_CYAN, COLOR_BLUE);     // ghost B
        init_pair(5, COLOR_GREEN, COLOR_GREEN);    // ghost G
        init_pair(6, COLOR_MAGENTA, COLOR_YELLOW);  // ghost Y
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

int main(){
    int sock = create_raw_socket(network_interface_pc1sofia);

    Frame frame;
    FILE* out = fopen("mundo_received.csv", "wb");
    if (!out) {
        fprintf(stderr, "Could not open mundo_received.csv for writing\n");
        return 1;
    }

    printf("Waiting for file frames...\n");

    while (1) {
        int rv = recv_frame(sock, &frame);

        if (rv == -1) {
            // timeout: keep waiting
            continue;
        }

        if (frame.type == MSG_TXT) {
            fwrite(frame.data, 1, frame.size, out);
            continue;
        }

        if (frame.type == MSG_END) break;
    }

    fclose(out);
    printf("Saved file as mundo_received.csv\n");

    FILE* in = fopen("mundo_received.csv", "rb");
    if (!in) {
        fprintf(stderr, "Could not open mundo_received.csv for visualization\n");
        return 1;
    }
    draw_world_ncurses(in);
    fclose(in);

    return 0;
}
