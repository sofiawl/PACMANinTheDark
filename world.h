#ifndef WORLD_H
#define WORLD_H

#include <algorithm>
#include <vector>

#define SIZE_WORLD 40

void update_world_csv(char world[SIZE_WORLD][SIZE_WORLD]);

std::vector<std::pair<int, int>> init_world(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int, int> pacman_coord);

int move_pacman(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int, int> &pacman_coord, int key);

int move_ghosts(char world[SIZE_WORLD][SIZE_WORLD], std::vector<std::pair<int, int>> &ghost_coord, std::pair<int, int> &pacman_coord, bool &go_left, bool &red_going_right, bool &blue_going_up);

#endif