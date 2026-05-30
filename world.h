#ifndef WORLD_H
#define WORLD_H

#include <algorithm>
#include <vector>

#define SIZE_WORLD 40

struct PillInfo {
    int row;
    int col;
    char id;
    int prize_type;
    const char* file_path;
};

void update_world_csv(char world[SIZE_WORLD][SIZE_WORLD]);

std::vector<std::pair<int, int>> init_world(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int, int> pacman_coord, int map_type, std::vector<PillInfo> &pills);

// Returns -1 ghost hit, 0 moved, or pill id ('1'..'6') when a pill was collected.
int move_pacman(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int, int> &pacman_coord, int key);

const PillInfo* find_pill_by_id(const std::vector<PillInfo> &pills, char pill_id);

int move_ghosts(char world[SIZE_WORLD][SIZE_WORLD], std::vector<std::pair<int, int>> &ghost_coord, std::pair<int, int> &pacman_coord, bool &go_left, bool &red_going_right, bool &blue_going_up);

#endif
