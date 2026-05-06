#include <stdio.h>
#include <sys/socket.h>
#include <ncursesw/ncurses.h>

#include "protocol.h"
#include "client_view.h"




// implement this on the main 
bool recv_file(int sock, Frame *f, const char* name){
    printf("Debug [recv_file] entrou\n");
    FILE* out = fopen(name, "wb");
    if (!out) {
        fprintf(stderr, "Could not open file for writing\n");
        return false;
    }

    Frame frame;
    while (1) {
        int rv = recv_frame(sock, &frame, CLIENT, SERVER, INTERFACE1);
        if (rv < 0) continue;  // timeout, keep waiting for server push
        printf("Debug [recv_file] type: %d\n", frame.type);
        if (frame.type == MSG_JPG) {
            fwrite(frame.data, 1, frame.size, out);
            continue;
        }
        if (frame.type == MSG_END) {
            printf("Debug [recv_file] msg_end\n");
            break;
        }
    }
    fflush(out);
    fclose(out);
    printf("Debug [recv_file] saida\n");
    return true;
}

bool receive_world(int sock) {
    FILE* out = fopen("mundo.csv", "wb");
    if (!out) {
        fprintf(stderr, "Could not open mundo.csv for writing\n");
        return false;
    }
    Frame frame;
    while (1) {
        int rv = recv_frame(sock, &frame, CLIENT, SERVER, INTERFACE1);
        if (rv < 0) continue;  // timeout, keep waiting for server push
        if (frame.type == MSG_WORLD) {
            fwrite(frame.data, 1, frame.size, out);
            continue;
        }
        if (frame.type == MSG_END) break;
    }
    fflush(out);
    fclose(out);
    return true;
}

void receive_key(int sock, int key){
    //while (1) {
        // draw world and check for a key immediately (non-blocking, wtimeout=0)
        
        // map arrow keys to single bytes the server understands
        MessageType type; 
        switch (key) {
            case KEY_UP:    type = MSG_UP; //break;
            case KEY_DOWN:  type = MSG_DOWN; //break;
            case KEY_LEFT:  type = MSG_LEFT; //break;
            case KEY_RIGHT: type = MSG_RIGHT; //break;
            //default:        type = MSG_DATA; //break;
        }
        printf("key recv_frame (%d)\n",key);

        Frame f;
        if (build_frame(&f, 0, type, nullptr, 1) == 0)
            send(sock, &f, CLIENT, SERVER, INTERFACE1);

        if (key == 'q' || key == 'Q') ;//break;

        Frame frame; 
        recv_file(sock, &frame, "1");

        
        // block here until server pushes the next world update (every 300ms)
        //if (!receive_world(sock));// break;
    //}

}

int main() {
    int sock = create_raw_socket(INTERFACE1);

    while(1){
        if (send_init(sock) != 0) {
            printf("Debug [main client]: Tempo expirado para receber o mapa inicial.\n");
            return -1;
        }

        // both the client and the server can start first
        if(recv_frame(sock, &f, SERVER, CLIENT, INTERFACE2) >= 0){

            
            if(f.type == MSG_INIT) {
                send_init(sock); 
            }
        }

        if (!receive_world(sock)) return 1;

        init_client_view();

        int key = draw_client_view_and_get_key("mundo.csv");
        if(key != ERR){
            receive_key(sock, key);

            //when I build the other functions hopefully this will work better 
            // if this is not here it will start building a world on top of the old one
            close_client_view();
        }  
    }
    

    close_client_view();
    printf("Game over\n");
    return 0;
}






/*
Ordem dos eventos:
1. receber mundo 
2. scanf movimento
3. recv_file 

First message goes to trash 

client starts first:
client starts and send init message
waits to receive a ack or init?
if receives init then sends init 



*/


