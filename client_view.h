#ifndef CLIENT_VIEW_H
#define CLIENT_VIEW_H

#include <utility>

#include "world.h"

void init_client_view();
void close_client_view();
void restore_client_view_after_overlay();
void draw_client_view_world(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int,int> pacman_coord, int radius);
void draw_client_view(const char* csv_path, std::pair<int,int> pacman_coord, int radius);
int  poll_client_key();
int  draw_client_view_and_get_key(const char* csv_path, std::pair<int,int> pacman_coord, int radius);

#endif