#ifndef WORLD_H
#define WORLD_H

#define SIZE_WORLD 40

void init_world(char world[SIZE_WORLD][SIZE_WORLD]);
void draw_world_ncurses_from_csv(const char* csv_path);

#endif
