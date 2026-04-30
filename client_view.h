#ifndef CLIENT_VIEW_H
#define CLIENT_VIEW_H

void init_client_view();
void close_client_view();
int  draw_client_view_and_get_key(const char* csv_path);

#endif