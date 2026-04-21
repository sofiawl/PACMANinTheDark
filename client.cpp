#include <stdio.h>
#include <sys/socket.h>
#include <string.h>

#include "protocol.h"
#include "world.h"

// get this information in bash with: ip link show
const char* network_interface_pc1sofia = "enp4s0";

int main(){
    int sock = create_raw_socket(network_interface_pc1sofia);

    Frame frame;
    char world[SIZE_WORLD][SIZE_WORLD];

    printf("Waiting for message...\n");

    while (1) {
        int rv = recv_frame(sock, &frame);

        if (rv == -1) {
            // timeout: keep waiting just for this test
            continue;
        }

        uint8_t row = frame.sequence;
        if (row >= SIZE_WORLD) continue;

        size_t copy_len = (frame.size <= SIZE_WORLD) ? frame.size : SIZE_WORLD;
        memcpy(world[row], frame.data, copy_len);

        if (row == SIZE_WORLD - 1) {
            // print world and exit
            printf("Received full world (end seq %u):\n", row);
            for (int i = 0; i < SIZE_WORLD; ++i) {
                for (int j = 0; j < SIZE_WORLD; ++j)
                    putchar(world[i][j]);
                putchar('\n');
            }
            break;
        }
    }
}
