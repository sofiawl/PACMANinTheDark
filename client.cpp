#include <stdio.h>
#include <sys/socket.h>

#include "protocol.h"

// get this information in bash with: ip link show
const char* network_interface_pc1sofia = "enp4s0";

int main(){
    int sock = create_raw_socket(network_interface_pc1sofia);

    unsigned char frame[64] = {0};

    printf("Waiting for message...\n");

    while (1) {
        int bytes = recv(sock, frame, sizeof(frame), 0);

        if (bytes == -1) {
            // timeout: keep waiting just for this test
            continue;
        }

        if (frame[12] == 0x08 && frame[13] == 0x88) {
            // Payload starts at byte 14
            printf("Received: %s\n", (char*)(frame + 14));
            break;
        }
    }
}
