#include <random>
#include <algorithm>
#include <fstream>
#include <vector>

#include "world.h"
#include "protocol.h"

enum GhostIndex { RED = 0, BLUE = 1, GREEN = 2, YELLOW = 3 };

void update_world_csv(char world[SIZE_WORLD][SIZE_WORLD]) {

    std::ofstream mundo_file("mundo.csv", std::ios::trunc);
    if (!mundo_file.is_open()) return;
    for (int i = 0; i < SIZE_WORLD; ++i) {
        mundo_file.write(world[i], SIZE_WORLD);
        mundo_file.put('\n');
    }
}

std::vector<std::pair<int, int>> init_world(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int, int> pacman_coord) {
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

    world[pacman_coord.first][pacman_coord.second] = 'P';

    // build list of allowed flattened positions (a vector of a matrix), exclude center
    std::vector<std::size_t> allowed;
    allowed.reserve(SIZE_WORLD*SIZE_WORLD - 1); // the allowed positions, does not include the wall

    for (std::size_t i = 0; i < SIZE_WORLD; ++i) {
        for (std::size_t j = 0; j < SIZE_WORLD; ++j) {
            bool is_center = (i == (SIZE_WORLD-1)/2 && j == (SIZE_WORLD-1)/2); // reserve center, PACMAN position [19][19]
            bool is_border = (i == 0 || i == SIZE_WORLD-1 || j == 0 || j == SIZE_WORLD-1); // reserve borders, the wall
            if (!is_center && !is_border)
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
    std::vector<std::pair<int, int>> ghost_coord;
    ghost_coord.push_back({pos0 / SIZE_WORLD, pos0 % SIZE_WORLD});
    ghost_coord.push_back({pos1 / SIZE_WORLD, pos1 % SIZE_WORLD});
    ghost_coord.push_back({pos2 / SIZE_WORLD, pos2 % SIZE_WORLD});
    ghost_coord.push_back({pos3 / SIZE_WORLD, pos3 % SIZE_WORLD});

    world[pos0 / SIZE_WORLD][pos0 % SIZE_WORLD] = 'R';
    world[pos1 / SIZE_WORLD][pos1 % SIZE_WORLD] = 'B';
    world[pos2 / SIZE_WORLD][pos2 % SIZE_WORLD] = 'G';
    world[pos3 / SIZE_WORLD][pos3 % SIZE_WORLD] = 'Y';

    // save world to mundo.csv
    update_world_csv(world);

    return ghost_coord;
}

int move_pacman(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int, int> &pacman_coord, int key) {
    int old_x, old_y;
    old_x = pacman_coord.first;
    old_y = pacman_coord.second;
    int new_x = old_x, new_y = old_y;

    switch (key) {
        case MSG_UP: new_x = old_x - 1; break;
        case MSG_DOWN: new_x = old_x + 1; break;
        case MSG_LEFT: new_y = old_y - 1; break;
        case MSG_RIGHT: new_y = old_y + 1; break;
        default: return 0;
    }

    if (world[new_x][new_y] == 'X') return 0;
    if (world[new_x][new_y] == 'Y' || world[new_x][new_y] == 'B' || world[new_x][new_y] == 'G' || world[new_x][new_y] == 'R') return -1;

    world[old_x][old_y] = '0';
    world[new_x][new_y] = 'P';

    pacman_coord = {new_x, new_y};
    return 0;
}

/*
 * In each pair consider: first row and second column:

1. Red: go foward (right -> second++), if hits the wall go left
2. Blue: go foward (up -> first--), if hits the wall go right
3. Green: go foward (down -> first++), if hits the wall alternate between left and right
4. Yellow: aleatory X>

If they hit each other, they should reverse direction: right->left and up->down and vice versa
 */
// only walls block movement, ghosts pass through each other
static bool is_wall(char world[SIZE_WORLD][SIZE_WORLD], int r, int c) {
    return world[r][c] == 'X';
}

// ping-pong right↔left across the row, going_right flips only on wall hit
int move_red_ghost(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int,int> &ghost_coord, std::pair<int,int> &pacman_coord, bool &going_right) {
    int old_x = ghost_coord.first, old_y = ghost_coord.second;
    int new_x = old_x, new_y = old_y;

    if (going_right) {
        if (!is_wall(world, old_x, old_y + 1))      new_y = old_y + 1;
        else { going_right = false;
               if (!is_wall(world, old_x, old_y - 1)) new_y = old_y - 1; }
    } else {
        if (!is_wall(world, old_x, old_y - 1))      new_y = old_y - 1;
        else { going_right = true;
               if (!is_wall(world, old_x, old_y + 1)) new_y = old_y + 1; }
    }

    world[old_x][old_y] = '0';
    if (world[new_x][new_y] == 'P') {
        world[new_x][new_y] = 'R';
        ghost_coord = {new_x, new_y};
        return -1;
    }
    world[new_x][new_y] = 'R';
    ghost_coord = {new_x, new_y};
    return 0;
}

// ping-pong up↔down across the column,  going_up flips only on wall hit
int move_blue_ghost(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int,int> &ghost_coord, std::pair<int,int> &pacman_coord, bool &going_up) {
    int old_x = ghost_coord.first, old_y = ghost_coord.second;
    int new_x = old_x, new_y = old_y;

    if (going_up) {
        if (!is_wall(world, old_x - 1, old_y))      new_x = old_x - 1;
        else { going_up = false;
               if (!is_wall(world, old_x + 1, old_y)) new_x = old_x + 1; }
    } else {
        if (!is_wall(world, old_x + 1, old_y))      new_x = old_x + 1;
        else { going_up = true;
               if (!is_wall(world, old_x - 1, old_y)) new_x = old_x - 1; }
    }

    world[old_x][old_y] = '0';
    if (world[new_x][new_y] == 'P') {
        world[new_x][new_y] = 'B';
        ghost_coord = {new_x, new_y};
        return -1;
    }
    world[new_x][new_y] = 'B';
    ghost_coord = {new_x, new_y};
    return 0;
}

// go down; at wall slide sideways, go_left only flips when the sideways direction is also blocked
int move_green_ghost(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int,int> &ghost_coord, bool &go_left, std::pair<int,int> &pacman_coord) {
    int old_x = ghost_coord.first, old_y = ghost_coord.second;
    int new_x = old_x, new_y = old_y;

    if (!is_wall(world, old_x + 1, old_y)) {
        new_x = old_x + 1;
    } else {
        int preferred = go_left ? old_y + 1 : old_y - 1;
        int fallback  = go_left ? old_y - 1 : old_y + 1;
        if (!is_wall(world, old_x, preferred)) {
            new_y = preferred;               // keep going same sideways direction
        } else {
            go_left = !go_left;              // only flip when preferred is blocked
            if (!is_wall(world, old_x, fallback)) new_y = fallback;
        }
    }

    world[old_x][old_y] = '0';
    if (world[new_x][new_y] == 'P') {
        world[new_x][new_y] = 'G';
        ghost_coord = {new_x, new_y};
        return -1;
    }
    world[new_x][new_y] = 'G';
    ghost_coord = {new_x, new_y};
    return 0;
}

// try all 4 directions in random order; take first non-wall
int move_yellow_ghost(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int,int> &ghost_coord, std::pair<int,int> &pacman_coord) {
    int old_x = ghost_coord.first, old_y = ghost_coord.second;
    int new_x = old_x, new_y = old_y;

    std::mt19937 gen(std::random_device{}());
    std::vector<std::pair<int,int>> moves = {{1,0},{-1,0},{0,1},{0,-1}};
    std::shuffle(moves.begin(), moves.end(), gen);

    for (auto &m : moves) {
        if (!is_wall(world, old_x + m.first, old_y + m.second)) {
            new_x = old_x + m.first;
            new_y = old_y + m.second;
            break;
        }
    }

    world[old_x][old_y] = '0';
    if (world[new_x][new_y] == 'P') {
        world[new_x][new_y] = 'Y';
        ghost_coord = {new_x, new_y};
        return -1;
    }
    world[new_x][new_y] = 'Y';
    ghost_coord = {new_x, new_y};
    return 0;
}

int move_ghosts(char world[SIZE_WORLD][SIZE_WORLD], std::vector<std::pair<int,int>> &ghost_coord, std::pair<int,int> &pacman_coord, bool &go_left, bool &red_going_right, bool &blue_going_up) {

    if (move_red_ghost(world,    ghost_coord[RED],    pacman_coord, red_going_right) == -1 ||
        move_green_ghost(world,  ghost_coord[GREEN],  go_left, pacman_coord) == -1 ||
        move_blue_ghost(world,   ghost_coord[BLUE],   pacman_coord, blue_going_up) == -1 ||
        move_yellow_ghost(world, ghost_coord[YELLOW], pacman_coord) == -1)
        return -1;

    return 0;
}
