#include <stdio.h>
#include <sys/socket.h>
#include <ncursesw/ncurses.h>

#include "protocol.h"
#include "client_view.h"


int send_init(int sock) {
    // sends a message to tell the server that it wants to start
    Frame f_send, f_recv;
    if(build_frame(&f_send, 0, MSG_INIT, nullptr, 1) == 0){
        send_frame(sock, &f_send, CLIENT, SERVER, INTERFACE1);
    }
    
    int timeout;
    while(1) {
        if (recv_frame(sock, &f_recv) >= 0) {

            if (f_recv.type == MSG_ACK) break;

            if (f_recv.type == MSG_NACK)
                send_frame(sock, &f_send, CLIENT, SERVER, INTERFACE1); 
        }
        if (timeout++ > TIMEOUT) return -1; 
    }
    return 0;
}


// implement this on the main 
void recv_file(int sock, Frame *f, const char* name){
    printf("Debug: [recv_file] \n");

    FILE *arq = fopen(name, "wb");
    if(arq == NULL){
        printf("Erro ao abrir arquivo\n");
        return; 
    }
    bool transmiting = true;  

    while(transmiting){
        if(recv_frame(sock, f)){

            if(f->type == MSG_END){
                send_ack(sock, f->sequence,CLIENT, SERVER, INTERFACE1);
                /* recv_frame(sock, f); */
                printf("Debug [recv_file] END - seq: %d\n", f->sequence); 
                transmiting = false; 
            } else { // TRATAR NACK DO SERVIDOR - retransmissao guardar ultima msg + TRATAR O NR SEQ
                fwrite(f->data, 1, f->size, arq); 
                //it is sending to many acks and the acks are not being received 
                send_ack(sock, f->sequence, CLIENT, SERVER, INTERFACE1); 
                // recv_frame(sock, f); 
                printf("Debug [recv_file] seq: %d\n", f->sequence); 
                printf("Debug: [recv_file]ack sent\n");
            }
        } /*else {
            // here is sending a bunch of nacks 
            // it should only send one nack if necessary, this loop doensn't make sense 
            send_nack(sock, f->sequence, CLIENT, SERVER, INTERFACE1); 
            //printf("Debug: [recv_file] nack sent\n");
            recv_frame(sock, f); 
        }*/

    }

    fflush(arq);
    fclose(arq); 
}

bool receive_world(int sock) {
    FILE* out = fopen("mundo.csv", "wb");
    if (!out) {
        fprintf(stderr, "Could not open mundo.csv for writing\n");
        return false;
    }
    Frame frame;
    while (1) {
        int rv = recv_frame(sock, &frame);
        if (rv < 0) continue;  // timeout, keep waiting for server push
        if (frame.type == MSG_WORLD) {
            send_ack(sock, frame.sequence, CLIENT, SERVER, INTERFACE1); 
            fwrite(frame.data, 1, frame.size, out);
            continue;
        }
        if (frame.type == MSG_END) break;
    }
    fflush(out);
    fclose(out);
    return true;
}

int main() {
    int sock = create_raw_socket(INTERFACE1);

    if (send_init(sock) != 0) {
        printf("Debug [main client]: Tempo expirado para receber o mapa inicial.\n");
        return -1;
    }

    printf("Waiting for initial world...\n");
    if (!receive_world(sock)) return 1;

    init_client_view();

    while (1) {
        // draw world and check for a key immediately (non-blocking, wtimeout=0)
        int key = draw_client_view_and_get_key("mundo.csv");
 
        if (key != ERR) {
            // maybe all of this key things should be in a function
            // map arrow keys to single bytes the server understands
            //uint8_t key_byte; 
            MessageType type; 
            switch (key) {
                case KEY_UP:    type = MSG_UP; break;
                case KEY_DOWN:  type = MSG_DOWN; break;
                case KEY_LEFT:  type = MSG_LEFT; break;
                case KEY_RIGHT: type = MSG_RIGHT; break;
                default:        type = MSG_DATA; break;
            }
            printf("key recv_frame (%d)\n",key);

            Frame f;
            if (build_frame(&f, 0, type, nullptr, 1) == 0)
                send_frame(sock, &f, CLIENT, SERVER, INTERFACE1);

            if (key == 'q' || key == 'Q') break;

            // this should only happen after pressing a key and receving a file but it is happening every time 
            //The problem is here, I don't know where to put this part 
            Frame frame; 
            if((recv_frame(sock, &frame))){
                if(frame.type == MSG_TXT || frame.type == MSG_JPG || frame.type == MSG_MP4){
                    recv_file(sock, &frame, "1");
                }
            }
        }

        
        
        // block here until server pushes the next world update (every 300ms)
        if (!receive_world(sock)) break;
    }

    close_client_view();
    printf("Game over\n");
    return 0;
}
/*
    int movement = capture_movement(); 
    Frame out_frame; 

    if (build_frame(&out_frame, seq_client++, (MessageType)movement, nullptr, 0) == 0) {
        send_frame(sock, &out_frame, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0);
    }   
        }

        if (frame.type == MSG_TXT || frame.type == MSG_JPG || frame.type == MSG_MP4){
            recv_file(sock, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0, "name", &frame); 
            continue; 
        }
        
        if (frame.type == MSG_END) break;



    */

    

/*
Ordem dos eventos:
1. receber mundo 
2. scanf movimento
3. recv_file 

Fazer um while para o mundo e um outro while para mensagens

Should I really create a type world? 
*/


