#include <string.h>
#include <stdio.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>

# include "protocol.h"

// get this information in bash with: ip link show
const char* network_interface_pc2sofia = "enx00e04c034558";

// get this information with: ip link show *your ip link name*
// put THE CLIENT MAC address here
unsigned char src_mac_pc2sofia[6] = {0x00, 0xe0, 0x4c, 0x03, 0x45, 0x58};

int main(){
    int sock = create_raw_socket(network_interface_pc2sofia);

    // Build a raw ethernet frame
    unsigned char frame[64] = {0};

    // Destination MAC (first 6 bytes)
    memcpy(frame, src_mac_pc2sofia, 6);

    // Source MAC — get from interface (bytes 6-11)
    // For now just put 0s, promiscuous mode on receiver will catch it anyway
    memset(frame + 6, 0x00, 6);

    // EtherType — use a custom one so receiver knows it's ours (bytes 12-13)
    frame[12] = 0x08;
    frame[13] = 0x88;

    // Payload — our message (bytes 14+)
    const char* msg = "Hello from PC2!";
    memcpy(frame + 14, msg, strlen(msg));

    // Send
    struct sockaddr_ll addr = {0};
    addr.sll_ifindex  = if_nametoindex(network_interface_pc2sofia);
    addr.sll_halen    = ETH_ALEN;
    memcpy(addr.sll_addr, src_mac_pc2sofia, 6);

    int sent = sendto(sock, frame, 64, 0, (struct sockaddr*)&addr, sizeof(addr));
    if (sent == -1) {
        perror("sendto failed");
    } else {
        printf("Sent %d bytes!\n", sent);
    }
}
