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

// get this information in bash with: ip link show
const char* network_interface_pc2sofia = "enx00e04c034558";
const char* network_interface_pcVeth0 = "veth0";

// get this information with: ip link show *your ip link name*
// put THE CLIENT MAC address here
unsigned char src_mac_pc2sofia[6] = {0x00, 0xe0, 0x4c, 0x03, 0x45, 0x58};
unsigned char dest_mac_pc1sofia[6] = {0x04, 0x7c, 0x16, 0xa9, 0xb2, 0x5b};

unsigned char SERVER_mac_pcVeth0[6] = {0x9e, 0x82, 0xe1, 0x6d, 0x4d, 0x6c};
unsigned char dest_mac_pcVeth1[6] = {0x52, 0x07, 0x95, 0x37, 0x1a, 0xc7};


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

        uint8_t buffer[1024];
        size_t bytes_read = fread(buffer, 1, 1024, arq);
        if (bytes_read == 0) break;
        if (build_frame(&f, seq, type, buffer, (uint8_t)bytes_read) != 0) break;
        if (send_frame(sock, &f, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0) < 0) break;
        seq++;

        // send END frame to signal world transmission is complete
        if (build_frame(&f, seq, MSG_END, nullptr, 0) == 0){
            send_frame(sock, &f, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0);
            printf("Debug: [send_file] file sent\n");
        }

        while(!ack_received){
            Frame answer; 
            int status = recv_frame(sock, &answer);

            //it is receiving way too many acks while the client only sends one ack 
            if(status){
                if(answer.type == 0 ){ //&& answer.sequence == seq
                    printf("Debug: [send_file] ack received\n");
                    ack_received = true;
                    seq++; 
                    printf("%d\n", seq); 
                } else if(answer.type == 1){
                    printf("Timeout!\n"); 
                }
            }
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

    Frame f;
    uint8_t seq = 0;
    while (1) {
        uint8_t buffer[DATA_SIZE];
        size_t bytes_read = fread(buffer, 1, DATA_SIZE, mundo);
        if (bytes_read == 0) break;
        if (build_frame(&f, seq, MSG_TXT, buffer, (uint8_t)bytes_read) != 0) break;
        if (send_frame(sock, &f, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0) < 0) break;
        seq++;
    }
    fclose(mundo);

    // send END frame to signal world transmission is complete
    if (build_frame(&f, seq, MSG_END, nullptr, 0) == 0)
        send_frame(sock, &f, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0);
}


int main(){
    int sock = create_raw_socket(network_interface_pcVeth0);

    // 10ms recv timeout — server loop never blocks waiting for client input
    struct timeval tv = {0, 10000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char world[SIZE_WORLD][SIZE_WORLD];

    std::pair<int, int> pacman_coord = {{(SIZE_WORLD-1)/2}, {(SIZE_WORLD-1)/2}};
    std::vector<std::pair<int, int>> ghost_coords = init_world(world, pacman_coord);
    bool green_go_left  = false;
    bool red_going_right = true;
    bool blue_going_up   = true;

    send_world(sock, world);

    Frame f;
    auto last_tick = std::chrono::steady_clock::now();
    const auto ghost_interval = std::chrono::milliseconds(300);

    while (1) {
        // non-blocking key check: returns after 10ms if client sent nothing
        if (recv_frame(sock, &f) >= 0) {
            if (f.type == MSG_END) break;
            if (f.type == MSG_TXT && f.size > 0) { // why is f.type MSG_TXT shouldn't be RIGHT, LEFT, UP or DOWN?  
                int key = f.data[0];
                if (key == 'q' || key == 'Q') break;
                if (move_pacman(world, pacman_coord, key) == -1) break;
                send_world(sock, world);  // respond immediately so Pacman feels responsive
                
                if(key == 'w' || key == 's'|| key == 'a' || key == 'd'){
                    printf("Debug: [main_server while] key pressed\n");
                    send_file("teste_grande.jpg", sock);
                }   
            }
        }

        // ghost tick: fires every 300ms regardless of client input
        auto now = std::chrono::steady_clock::now();
        if (now - last_tick >= ghost_interval) {
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