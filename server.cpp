#include <cstdint>
#include <cstdio>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <chrono>

#include "protocol.h"
#include "world.h"


// It can also identified by the pills 
int identify_type(std::string nomeArquivo) {
    if (nomeArquivo.find(".txt") != std::string::npos) return 5;
    if (nomeArquivo.find(".jpg") != std::string::npos) return 6;
    if (nomeArquivo.find(".mp4") != std::string::npos) return 7;
    return -1; 
}


uint8_t send_file(const char* arquivo, int sock){
    printf("Debug: [send_file]\n");
    
    FILE *arq = fopen(arquivo, "rb"); 
    if(!arq){
        fprintf(stderr, "Could not open file for sending"); 
        return -1; 
    }

    Frame f; 
    uint16_t seq = 0; 
    bool ack_received;

    int type_int = identify_type(arquivo);  
    MessageType type = (MessageType) type_int; 


    while (1) {
        ack_received = false;

        uint8_t buffer[DATA_SIZE];
        size_t bytes_read = fread(buffer, 1,DATA_SIZE, arq);
        if (bytes_read == 0) break;
        if (build_frame(&f, seq, type, buffer, (uint8_t)bytes_read) != 0) break;
        if (send_frame(sock, &f, SERVER, CLIENT, INTERFACE2) < 0) break;

        // send END frame to signal world transmission is complete
        if (build_frame(&f, seq, MSG_END, nullptr, 0) == 0){
            send_frame(sock, &f, SERVER, CLIENT, INTERFACE2);
            printf("Debug: [send_file] file sent\n");
        }

        while(!ack_received){
            Frame answer; 
            int status = recv_frame(sock, &answer);

            //it is receiving way too many acks while the client only sends one ack 
            if(status){
                if(answer.type == MSG_ACK ){ //&& answer.sequence == seq
                    printf("Debug: [send_file] ack received\n");
                    ack_received = true;
                    seq++; 
                    printf("Debug [send_file] seq: %d\n", seq); 
                } else if(answer.type == MSG_NACK){
                    printf("Timeout!\n"); // tratar retransmissao
                }
            } 
            /*
            else {
                printf("Debug [send_file]: Malformed frame.\n"); // mandar NACK PARA O CLIENTE
            }
            */
        }
    }

    return 0; 
    fclose(arq); 

}


void send_world(int sock, char world[SIZE_WORLD][SIZE_WORLD]) {
    update_world_csv(world);

    FILE* mundo = fopen("mundo.csv", "rb");
    if (!mundo) {
        fprintf(stderr, "Could not open mundo.csv for sending\n");
        return;
    }

    Frame f_send, f_recv;
    uint8_t seq = 0;
    while (1) {
        uint8_t buffer[DATA_SIZE];
        size_t bytes_read = fread(buffer, 1, DATA_SIZE, mundo);
        if (bytes_read == 0) break;
        if (build_frame(&f_send, seq, MSG_WORLD, buffer, (uint8_t)bytes_read) != 0) break;
        if (send_frame(sock, &f_send, SERVER, CLIENT, INTERFACE2) < 0) break;
        
        // it does not enter any of them 
        do {
            if (recv_frame(sock, &f_recv) >= 0 && f_recv.type == MSG_NACK) {
                printf("Debug [send_world] nack recv\n");
                send_frame(sock, &f_send, SERVER, CLIENT, INTERFACE2);
            } 
        } while (f_recv.type == MSG_NACK);

        printf("Debug [send_world] ack recv\n");
        seq++;

        printf("Debug [send_world] recv type: %d\n", f_recv.type);
        
    }
    fclose(mundo);

    // send END frame to signal world transmission is complete
    // should I change the end too?
    if (build_frame(&f_send, seq, MSG_END, nullptr, 0) == 0)
        send_frame(sock, &f_send, SERVER, CLIENT, INTERFACE2);
}


int main(){
    bool start = false; 

    int sock = create_raw_socket(INTERFACE2);
  
    // 10ms recv timeout — server loop never blocks waiting for client input
    struct timeval tv = {0, 10000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));


    char world[SIZE_WORLD][SIZE_WORLD];

    std::pair<int, int> pacman_coord = {{(SIZE_WORLD-1)/2}, {(SIZE_WORLD-1)/2}};
    std::vector<std::pair<int, int>> ghost_coords = init_world(world, pacman_coord);
    bool green_go_left  = false;
    bool red_going_right = true;
    bool blue_going_up   = true;

    Frame f;
    auto last_tick = std::chrono::steady_clock::now();
    const auto ghost_interval = std::chrono::milliseconds(300);

    while (1) {
        // non-blocking key check: returns after 10ms if client sent nothing
        if (recv_frame(sock, &f) >= 0) {

            // trata END
            if (f.type == MSG_END) break;

            // trata o INIT
            if (f.type == MSG_INIT) {
                send_ack(sock, 0, SERVER, CLIENT, INTERFACE2);
                start = true;
                send_world(sock, world);
            }

            // trata movimento
            if ((f.type == MSG_UP || f.type == MSG_DOWN || f.type == MSG_LEFT || f.type == MSG_RIGHT) && f.size > 0 && start) { // why is f.type MSG_TXT shouldn't be RIGHT, LEFT, UP or DOWN?  
                //if (key == 'q' || key == 'Q') break;
                if (move_pacman(world, pacman_coord, f.type) == -1) break;
                send_world(sock, world);  // respond immediately so Pacman feels responsive

                    printf("Debug: [main_server while] key pressed\n");
                    send_file("teste-jpg.jpg", sock);
            }

        }

        //printf("Debug: [main_server] f.type (%d)\n",f.type);
        // ghost tick: fires every 300ms regardless of client input
        auto now = std::chrono::steady_clock::now();
        if ((now - last_tick >= ghost_interval) && start){
            last_tick = now;
            if (move_ghosts(world, ghost_coords, pacman_coord, green_go_left, red_going_right, blue_going_up) == -1) break;
            send_world(sock, world);
        }
    }

    printf("Game over\n");
    return 0;

}

/*
o mundo só vai ser mandado depois que tudo acontecer dentro do while(1)
como que eu faço para:
1. manda o mundo 
2. esperar o cliente andar para algum lugar
3. checar se comeu pastilha, bateu na parede ou só atualizar a coordenada 
4. mandar a mensagem da pilula 
*/