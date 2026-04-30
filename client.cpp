#include <stdio.h>
#include <sys/socket.h>
#include <ncursesw/ncurses.h>

#include "protocol.h"
#include "client_view.h"

// get this information in bash with: ip link show
const char* network_interface_pc1sofia = "enp4s0";
const char* network_interface_pcVeth1 = "veth1";

// get this information in bash with: ip link show
const char* network_interface_pc2sofia = "enx00e04c034558";
const char* network_interface_pcVeth0 = "veth0";

// get this information with: ip link show *your ip link name*
// put THE CLIENT MAC address here
unsigned char src_mac_pc2sofia[6] = {0x00, 0xe0, 0x4c, 0x03, 0x45, 0x58};
unsigned char dest_mac_pc1sofia[6] = {0x04, 0x7c, 0x16, 0xa9, 0xb2, 0x5b};

unsigned char SERVER_mac_pcVeth0[6] = {0x9e, 0x82, 0xe1, 0x6d, 0x4d, 0x6c};
unsigned char dest_mac_pcVeth1[6] = {0x52, 0x07, 0x95, 0x37, 0x1a, 0xc7};


// implement this on the main 
void recv_file(int sock, Frame *f, const char* name){
    printf("Debug: [recv_file] \n");

    FILE *arq = fopen(name, "wb");
    bool transmiting = true;  

    while(transmiting){
        if(recv_frame(sock, f)){

            if(f->type == 16){
                send_ack(sock, f->sequence, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0);
                //printf("Debug: [recv_file] ack sent\n");
                transmiting = false; 
            } else {
                fwrite(f->data, 1, f->size, arq); 
                send_ack(sock, f->sequence, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0); 
                recv_frame(sock, f); 
            }
        } else {
            send_nack(sock, f->sequence, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0); 
            //printf("Debug: [recv_file] nack sent\n");
            recv_frame(sock, f); 
        }

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
        if (frame.type == MSG_TXT) {
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
    int sock = create_raw_socket(network_interface_pcVeth1);


    printf("Waiting for initial world...\n");
    if (!receive_world(sock)) return 1;

    init_client_view();

    while (1) {
        // draw world and check for a key immediately (non-blocking, wtimeout=0)
        int key = draw_client_view_and_get_key("mundo.csv");

        if (key != ERR) {
            // map arrow keys to single bytes the server understands
            uint8_t key_byte;
            switch (key) {
                case KEY_UP:    key_byte = 'w'; break;
                case KEY_DOWN:  key_byte = 's'; break;
                case KEY_LEFT:  key_byte = 'a'; break;
                case KEY_RIGHT: key_byte = 'd'; break;
                default:        key_byte = (uint8_t)key; break;
            }

            Frame f;
            if (build_frame(&f, 0, MSG_TXT, &key_byte, 1) == 0)
                send_frame(sock, &f, dest_mac_pcVeth1, SERVER_mac_pcVeth0, network_interface_pcVeth1);

            if (key == 'q' || key == 'Q') break;
        }

        // this should only happen after pressing a key and receving a file but it is happening every time 
        //The problem is here, I don't know where to put this part 
        /*Frame frame; 
        if((recv_frame(sock, &frame))){
            recv_file(sock, &frame, "1");
        }*/
        
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

The ghosts are not moving anymore which means that it senses that the file is receives and gets stuck in a loop somewhere
*/


