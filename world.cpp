#include <random>
#include <algorithm>
#include <fstream>
#include <vector>
#include <tuple>

#include "world.h"
#include "protocol.h"

enum GhostIndex { RED = 0, BLUE = 1, GREEN = 2, YELLOW = 3 };

static bool is_pill(char c) {
    return c >= '1' && c <= '6';
}

// Ghosts cannot enter pill cells (positions must stay fixed until Pacman collects them).
static bool is_ghost_blocked(char world[SIZE_WORLD][SIZE_WORLD], int r, int c) {
    char cell = world[r][c];
    return cell == 'X' || is_pill(cell);
}

void update_world_csv(char world[SIZE_WORLD][SIZE_WORLD]) {

    std::ofstream mundo_file("mundo.csv", std::ios::trunc);
    if (!mundo_file.is_open()) return;
    for (int i = 0; i < SIZE_WORLD; ++i) {
        mundo_file.write(world[i], SIZE_WORLD);
        mundo_file.put('\n');
    }
}

static void make_map(char world[SIZE_WORLD][SIZE_WORLD], int type) {
    for (int i = 0; i < SIZE_WORLD; i++) {
        world[i][0] = 'X';
        world[i][SIZE_WORLD - 1] = 'X';
        world[0][i] = 'X';
        world[SIZE_WORLD - 1][i] = 'X';
    }

    for (int i = 1; i < SIZE_WORLD - 1; i++) {
        for (int j = 1; j < SIZE_WORLD - 1; j++) {
            world[i][j] = '0';
        }
    }

    if (type == UFPR_MAP) {
        const int MASK_ROWS = 8;
        const int MASK_COLS = 34;
        const char* ufpr_mask[MASK_ROWS] = {
            "X000X000XXXXXX000XXXXXX000XXXX0000",
            "X000X000X00000000X0000X0000000X000",
            "X000X000X00000000X0000X000X0000X00",
            "X000X000X00000000X0000X000X0000X00",
            "X000X000XXXX00000X0XXXX000XXXXX000",
            "X000X000X00000000X00000000X000X000",
            "X000X000X00000000X00000000X0000X00",
            "0XXX0000X00000000X00000000X0000X00"
        };
        int start_row = (SIZE_WORLD - MASK_ROWS) / 2;
        int start_col = (SIZE_WORLD - MASK_COLS) / 2;

        for (int i = 0; i < MASK_ROWS; i++) {
            for (int j = 0; j < MASK_COLS; j++) {
                world[start_row + i][start_col + j] = ufpr_mask[i][j];
            }
        }
    }
}

static const char* PILL_FILES[] = {
    "pills/1.txt",
    "pills/2.txt",
    "pills/3.jpg",
    "pills/4.jpg",
    "pills/5.mp4",
    "pills/6.mp4",
};

const PillInfo* find_pill_by_id(const std::vector<PillInfo> &pills, char pill_id) {
    for (const auto &p : pills) {
        if (p.id == pill_id) return &p;
    }
    return nullptr;
}

std::vector<std::pair<int, int>> init_world(char world[SIZE_WORLD][SIZE_WORLD],
    std::pair<int, int> pacman_coord, int map_type,
    std::vector<PillInfo> &pills) {

    make_map(world, map_type);

    std::random_device rd;
    std::mt19937 gen(rd());

    world[pacman_coord.first][pacman_coord.second] = 'P';

    std::vector<std::size_t> allowed;
    allowed.reserve(SIZE_WORLD * SIZE_WORLD);

    for (std::size_t i = 0; i < SIZE_WORLD; ++i) {
        for (std::size_t j = 0; j < SIZE_WORLD; ++j) {
            bool is_center = (i == (SIZE_WORLD - 1) / 2 && j == (SIZE_WORLD - 1) / 2);
            bool is_border = (i == 0 || i == SIZE_WORLD - 1 || j == 0 || j == SIZE_WORLD - 1);
            if (!is_center && !is_border && world[i][j] != 'X')
                allowed.push_back(i * SIZE_WORLD + j);
        }
    }

    std::shuffle(allowed.begin(), allowed.end(), gen);

    auto pos0 = allowed[0];
    auto pos1 = allowed[1];
    auto pos2 = allowed[2];
    auto pos3 = allowed[3];
    auto pill1 = allowed[4];
    auto pill2 = allowed[5];
    auto pill3 = allowed[6];
    auto pill4 = allowed[7];
    auto pill5 = allowed[8];
    auto pill6 = allowed[9];

    std::vector<std::pair<int, int>> ghost_coord;
    ghost_coord.push_back({(int)(pos0 / SIZE_WORLD), (int)(pos0 % SIZE_WORLD)});
    ghost_coord.push_back({(int)(pos1 / SIZE_WORLD), (int)(pos1 % SIZE_WORLD)});
    ghost_coord.push_back({(int)(pos2 / SIZE_WORLD), (int)(pos2 % SIZE_WORLD)});
    ghost_coord.push_back({(int)(pos3 / SIZE_WORLD), (int)(pos3 % SIZE_WORLD)});

    world[pos0 / SIZE_WORLD][pos0 % SIZE_WORLD] = 'R';
    world[pos1 / SIZE_WORLD][pos1 % SIZE_WORLD] = 'B';
    world[pos2 / SIZE_WORLD][pos2 % SIZE_WORLD] = 'G';
    world[pos3 / SIZE_WORLD][pos3 % SIZE_WORLD] = 'Y';

    struct PillPlan { std::size_t pos; char id; int prize; const char* file; } plans[] = {
        {pill1, '1', MSG_TXT, PILL_FILES[0]}, {pill2, '2', MSG_TXT, PILL_FILES[1]},
        {pill3, '3', MSG_JPG, PILL_FILES[2]}, {pill4, '4', MSG_JPG, PILL_FILES[3]},
        {pill5, '5', MSG_MP4, PILL_FILES[4]}, {pill6, '6', MSG_MP4, PILL_FILES[5]},
    };
    pills.clear();
    for (const auto &plan : plans) {
        int r = (int)(plan.pos / SIZE_WORLD);
        int c = (int)(plan.pos % SIZE_WORLD);
        world[r][c] = plan.id;
        pills.push_back({r, c, plan.id, plan.prize, plan.file});
    }

    update_world_csv(world);
    return ghost_coord;
}

int move_pacman(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int, int> &pacman_coord, int key) {
    int old_x = pacman_coord.first;
    int old_y = pacman_coord.second;
    int new_x = old_x, new_y = old_y;

    switch (key) {
        case MSG_UP:    new_x = old_x - 1; break;
        case MSG_DOWN:  new_x = old_x + 1; break;
        case MSG_LEFT:  new_y = old_y - 1; break;
        case MSG_RIGHT: new_y = old_y + 1; break;
        default: return 0;
    }

    char dest = world[new_x][new_y];
    if (dest == 'X') return 0;
    if (dest == 'Y' || dest == 'B' || dest == 'G' || dest == 'R')
        return -1;

    bool ate_pill = is_pill(dest);
    world[old_x][old_y] = '0';
    world[new_x][new_y] = 'P';
    pacman_coord = {new_x, new_y};
    return ate_pill ? (int)dest : 0;
}

int move_red_ghost(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int,int> &ghost_coord, std::pair<int,int> &pacman_coord, bool &going_right) {
    int old_x = ghost_coord.first, old_y = ghost_coord.second;
    int new_x = old_x, new_y = old_y;

    if (going_right) {
        if (!is_ghost_blocked(world, old_x, old_y + 1))      new_y = old_y + 1;
        else { going_right = false;
               if (!is_ghost_blocked(world, old_x, old_y - 1)) new_y = old_y - 1; }
    } else {
        if (!is_ghost_blocked(world, old_x, old_y - 1))      new_y = old_y - 1;
        else { going_right = true;
               if (!is_ghost_blocked(world, old_x, old_y + 1)) new_y = old_y + 1; }
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

int move_blue_ghost(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int,int> &ghost_coord, std::pair<int,int> &pacman_coord, bool &going_up) {
    int old_x = ghost_coord.first, old_y = ghost_coord.second;
    int new_x = old_x, new_y = old_y;

    if (going_up) {
        if (!is_ghost_blocked(world, old_x - 1, old_y))      new_x = old_x - 1;
        else { going_up = false;
               if (!is_ghost_blocked(world, old_x + 1, old_y)) new_x = old_x + 1; }
    } else {
        if (!is_ghost_blocked(world, old_x + 1, old_y))      new_x = old_x + 1;
        else { going_up = true;
               if (!is_ghost_blocked(world, old_x - 1, old_y)) new_x = old_x - 1; }
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

int move_green_ghost(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int,int> &ghost_coord, bool &go_left, std::pair<int,int> &pacman_coord) {
    int old_x = ghost_coord.first, old_y = ghost_coord.second;
    int new_x = old_x, new_y = old_y;

    if (!is_ghost_blocked(world, old_x + 1, old_y)) {
        new_x = old_x + 1;
    } else {
        int preferred = go_left ? old_y + 1 : old_y - 1;
        int fallback  = go_left ? old_y - 1 : old_y + 1;
        if (!is_ghost_blocked(world, old_x, preferred)) {
            new_y = preferred;
        } else {
            go_left = !go_left;
            if (!is_ghost_blocked(world, old_x, fallback)) new_y = fallback;
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

int move_yellow_ghost(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int,int> &ghost_coord, std::pair<int,int> &pacman_coord) {
    int old_x = ghost_coord.first, old_y = ghost_coord.second;
    int new_x = old_x, new_y = old_y;

    std::mt19937 gen(std::random_device{}());
    std::vector<std::pair<int,int>> moves = {{1,0},{-1,0},{0,1},{0,-1}};
    std::shuffle(moves.begin(), moves.end(), gen);

    for (auto &m : moves) {
        if (!is_ghost_blocked(world, old_x + m.first, old_y + m.second)) {
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
