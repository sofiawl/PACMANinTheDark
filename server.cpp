#include <cstdint>
#include <cstdio>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "protocol.h"
#include "world.h"

// get this information in bash with: ip link show
const char* network_interface_pc2sofia = "enx00e04c034558";

// get this information with: ip link show *your ip link name*
// put THE CLIENT MAC address here
unsigned char src_mac_pc2sofia[6] = {0x00, 0xe0, 0x4c, 0x03, 0x45, 0x58};
unsigned char dest_mac_pc1sofia[6] = {0x04, 0x7c, 0x16, 0xa9, 0xb2, 0x5b};


int main(){
    int sock = create_raw_socket(network_interface_pc2sofia);

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

        if (send_frame(sock, &f, src_mac_pc2sofia, dest_mac_pc1sofia, network_interface_pc2sofia) < 0) {
            fprintf(stderr, "Failed to send frame %u\n", seq);
            fclose(mundo);
            return 1;
        }

        seq++;
    }

    fclose(mundo);

    if (build_frame(&f, seq, MSG_END, nullptr, 0) == 0) {
        send_frame(sock, &f, src_mac_pc2sofia, dest_mac_pc1sofia, network_interface_pc2sofia);
    }

    printf("mundo.csv sent in %u data frames\n", seq);
    return 0;
}
