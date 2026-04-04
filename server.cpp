#include <string.h>
#include <stdio.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>

# include "protocol.h"

// get this information in bash with: ip link show
const char* network_interface_pc1sofia = "enp4s0";

// get this information with: ip link show *your ip link name*
// put THE CLIENT MAC address here
unsigned char dest_mac_pc1sofia[6] = {0x04, 0x7c, 0x16, 0xa9, 0xb2, 0x5b};

int main(){
    int sock = create_raw_socket(network_interface_pc1sofia);

    // Build a raw ethernet frame
    unsigned char frame[64] = {0};

    // Destination MAC (first 6 bytes)
    memcpy(frame, dest_mac_pc1sofia, 6);

    // Source MAC — get from interface (bytes 6-11)
    // For now just put 0s, promiscuous mode on receiver will catch it anyway
    memset(frame + 6, 0x00, 6);

    // EtherType — use a custom one so receiver knows it's ours (bytes 12-13)
    frame[12] = 0x08;
    frame[13] = 0x88;

    // Payload — our message (bytes 14+)
    const char* msg = "Hello from PC1!";
    memcpy(frame + 14, msg, strlen(msg));

    // Send
    struct sockaddr_ll addr = {0};
    addr.sll_ifindex  = if_nametoindex(network_interface_pc1sofia);
    addr.sll_halen    = ETH_ALEN;
    memcpy(addr.sll_addr, dest_mac_pc1sofia, 6);

    int sent = sendto(sock, frame, 64, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (sent == -1) {
        perror("sendto failed");
    } else {
        printf("Sent %d bytes!\n", sent);
    }
}
