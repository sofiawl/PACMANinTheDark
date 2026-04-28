#include <cstdint>
#include <cstdio>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string>

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


uint8_t send_file(const char* arquivo, int sock, unsigned char src_mac[6], unsigned char dest_mac[6], const char* iface){
    printf("Debug: [send_file]\n");
    Frame f; 
    uint8_t buffer[1024];
    uint16_t seq = 0; 
    size_t bytes_lidos; 
    bool ack_received; 
    
    FILE *arq = fopen(arquivo, "rb"); 
    int type_int = identify_type(arquivo);  
    MessageType type = (MessageType) type_int; 

    while((bytes_lidos = fread(buffer, 1, 1024, arq)) > 0){
        printf("Debug: [send_file] first while\n");
        ack_received = false; 

        while(!ack_received){
            build_frame(&f, seq, type, buffer, bytes_lidos); 
            calc_CRC(&f); 
            send_frame(sock, &f, src_mac, dest_mac, iface); 
        }

        // stop and wait 
        Frame answer; 
        int status = recv_frame(sock, &answer);

        if(status > 0){
            // it does not enter this part (of course it doesn't you have to implement the client first)
            printf("Debug: [send_file] status > 0\n");
            if(answer.type == 0 && answer.sequence == seq){
                ack_received = true;
                seq++; 
                printf("%d\n", seq); 
            } else {
                printf("Timeout!"); 
            }
        }
    }

    return 0; 
    fclose(arq); 

}


int main(){
    int sock = create_raw_socket(network_interface_pcVeth0);

    Frame f;
    uint8_t seq = 0;
    char world[SIZE_WORLD][SIZE_WORLD];

    init_world(world);

    FILE* mundo = fopen("mundo.csv", "rb");
    if (!mundo) {
        fprintf(stderr, "Could not open mundo.csv for sending\n");
        return 1;
    }

    while (1) {
        uint8_t buffer[DATA_SIZE];
        size_t bytes_read = fread(buffer, 1, DATA_SIZE, mundo);
        if (bytes_read == 0) break;

        if (seq > 63) {
            fprintf(stderr, "File too large for current sequence field\n");
            fclose(mundo);
            return 1;
        }

        if (build_frame(&f, seq, MSG_TXT, buffer, (uint8_t)bytes_read) != 0) {
            fprintf(stderr, "Failed to build frame %u\n", seq);
            fclose(mundo);
            return 1;
        }

        if (send_frame(sock, &f, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0) < 0) {
            fprintf(stderr, "Failed to send frame %u\n", seq);
            fclose(mundo);
            return 1;
        }


        seq++;
    }

    fclose(mundo);

    if (build_frame(&f, seq, MSG_END, nullptr, 0) == 0) {
        send_frame(sock, &f, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0);
        printf("Debug: [build_frame]\n");
        send_file("1.txt", sock, SERVER_mac_pcVeth0, dest_mac_pcVeth1, network_interface_pcVeth0); 
    }

    printf("mundo.csv sent in %u data frames\n", seq);
    return 0;
}
