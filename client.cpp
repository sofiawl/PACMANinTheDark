#include <stdio.h>
#include <sys/socket.h>

#include "protocol.h"
#include "client_view.h"

// get this information in bash with: ip link show
const char* network_interface_pc1sofia = "enp4s0";

int main(){
    int sock = create_raw_socket(network_interface_pc1sofia);

    Frame frame;
    FILE* out = fopen("mundo_received.csv", "wb");
    if (!out) {
        fprintf(stderr, "Could not open mundo_received.csv for writing\n");
        return 1;
    }

    printf("Waiting for file frames...\n");

    while (1) {
        int rv = recv_frame(sock, &frame);

        if (rv == -1) {
            // timeout: keep waiting
            continue;
        }

        if (frame.type == MSG_TXT) {
            fwrite(frame.data, 1, frame.size, out);
            continue;
        }

        if (frame.type == MSG_END) break;
    }

    fflush(out);
    fclose(out);
    printf("Saved file as mundo_received.csv\n");
    fflush(stdout);

    draw_client_view_from_csv("mundo_received.csv");

    return 0;
}
