#include <arpa/inet.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdlib.h>
#include <stdio.h>

int create_raw_socket(const char* network_interface_name) {
    // Create file for socket, doesn't use any local machine protocol
    int socket = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (socket == -1) {
        fprintf(stderr, "Erro ao criar socket: Verifique se você é root!\n");
        exit(-1);
    }

    int ifindex = if_nametoindex(network_interface_name);

    struct sockaddr_ll addr = {0};
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = ifindex;
    // Inicialize socket
    if (bind(socket, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        fprintf(stderr, "Erro ao fazer bind no socket\n");
        exit(-1);
    }

    struct packet_mreq mr = {0};
    mr.mr_ifindex = ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    // Do not throw out what it think is trash: "Modo promíscuo"
    if (setsockopt(socket, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1) {
        fprintf(stderr, "Erro ao fazer setsockopt: "
            "Verifique se a interface de rede foi especificada corretamente.\n");
        exit(-1);
    }

    const int timeoutMillis = 300; // 300 milisegundos de timeout por exemplo
    struct timeval timeout = { .tv_sec = timeoutMillis / 1000, .tv_usec = (timeoutMilis % 1000) * 1000 };
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(timeout));

    return socket;
}
