#ifndef CLIENT_VIEW_H
#define CLIENT_VIEW_H

#include <utility>

void init_client_view();
void close_client_view();
int  draw_client_view_and_get_key(const char* csv_path, std::pair<int,int> pacman_coord, int radius);

#endif