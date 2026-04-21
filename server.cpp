#include <cstdint>
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
unsigned char dest_mac_pc2sofia[6] = {0x00, 0xe0, 0x4c, 0x03, 0x45, 0x58};


int main(){
    int sock = create_raw_socket(network_interface_pc2sofia);

    Frame f;
    uint8_t seq, *data, data_size;
    MessageType msgtype;
    char world[SIZE_WORLD][SIZE_WORLD];

    init_world(world);

    data_size = SIZE_WORLD;
    msgtype = MSG_INIT;
    seq = 0;
    for (int i = 0; i< SIZE_WORLD; i++){
        data = (uint8_t*)world[i];
        build_frame(&f, seq, msgtype, data, data_size); // add checks for errors later
        send_frame(sock, &f, src_mac_pc2sofia, dest_mac_pc2sofia, network_interface_pc2sofia); // here too
        seq++;
    }
}
