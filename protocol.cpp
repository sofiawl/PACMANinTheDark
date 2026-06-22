#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>
#include <cerrno>
#include <format>
#include "protocol.h"
#include "log.h"

unsigned char SERVER[6] = {0x00, 0xe0, 0x4c, 0x03, 0x45, 0x58};
unsigned char CLIENT[6] = {0x04, 0x7c, 0x16, 0xa9, 0xb2, 0x5b};

// Why this value? Cause it is *01111110* in hex
#define MARKER 0x7E

// CRC-8 polynomial: x^8 + x^2 + x + 1 = 100000111 in binary
#define CRC8_POLY "100000111"

int create_raw_socket(const char* network_interface_name) {
    // Create file for socket, doesn't use any local machine protocol
    int sk = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (sk == -1) {
        fprintf(stderr, "Error creating the socket, are you really root?\n");
        log("PROTOCOLO", "ERRO", "Error creating the socket, are you really root?");
        exit(-1);
    }

    int ifindex = if_nametoindex(network_interface_name);

    struct sockaddr_ll addr = {0};
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = ifindex;
    // Inicialize socket
    if (bind(sk, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        fprintf(stderr, "Error making bind in socket\n");
        log("PROTOCOLO", "ERRO", "Error making bind in socket");
        exit(-1);
    }

    struct packet_mreq mr = {0};
    mr.mr_ifindex = ifindex;
    mr.mr_type = PACKET_MR_PROMISC;
    // Do not throw out what it think is trash: "Modo promíscuo"
    if (setsockopt(sk, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr)) == -1) {
        fprintf(stderr, "Error making setsockopt: "
            "Verify if the interface network is allright.\n");
            log("PROTOCOLO", "ERRO", "Verify if the interface network is allright.");
        exit(-1);
    }

    const int timeoutMillis = 300; // 300 milisegundos de timeout por exemplo
    struct timeval timeout = { .tv_sec = timeoutMillis / 1000, .tv_usec = (timeoutMillis % 1000) * 1000 };
    setsockopt(sk, SOL_SOCKET, SO_RCVTIMEO, (char*) &timeout, sizeof(timeout));

    return sk;
}

// converts each byte into a string of 8 '0'/'1' characters
static std::string bytes_to_bitstring(uint8_t* data, int len) {
    std::string result = "";
    for (int i = 0; i < len; i++)
        for (int bit = 7; bit >= 0; bit--)
            result += ((data[i] >> bit) & 1) ? '1' : '0'; // this means if the bit is 1, append '1', else append '0'
    return result;
}

// converts a string of '0'/'1' characters back to a uint8_t (max 8bits)
static uint8_t bitstring_to_byte(std::string bits) {
    uint8_t result = 0;
    for (int i = 0; i < (int)bits.length(); i++)
        result = (result << 1) | (bits[i] - '0');
    return result;
}

// binary division where subtraction is XOR. Kill me know
uint8_t calc_CRC(Frame* f) {
    // treat every byte of the frame except the crc field as the input bitstring
    uint8_t* bytes = (uint8_t*)f;
    std::string input_bitstring = bytes_to_bitstring(bytes, sizeof(Frame) - 1);

    // strip leading zeros from polynomial (same as Python'slstrip("0")) in wikipedia
    std::string poly = CRC8_POLY;
    int start = poly.find('1');
    if (start != (int)std::string::npos)
        poly = poly.substr(start);

    int len_input = input_bitstring.length();
    int poly_len  = poly.length();

    // append (poly_len - 1) zeros as padding — same as Python's initial_filler='0'
    std::vector<char> arr(input_bitstring.begin(),
    input_bitstring.end());
    for (int i = 0; i < poly_len - 1; i++)
        arr.push_back('0');

    // while there is a '1' in the INPUT portion (not the padding)
    while (true) {
        // FIXED: search only within [:len_input], not the whole array. THIS WAS THE PROBLEM ALBINI TOLD US ABOUT
        int cur_shift = -1;
        for (int i = 0; i < len_input; i++) {
            if (arr[i] == '1') { cur_shift = i; break; }
        }
        if (cur_shift == -1) break; // no '1' found in input portion — done

        // XOR the polynomial at cur_shift position, bit by bit
        for (int i = 0; i < poly_len; i++)
            arr[cur_shift + i] = (poly[i] != arr[cur_shift + i]) ? '1': '0';
    }

    // the remainder is the padding portion (everything after len_input)
    std::string remainder(arr.begin() + len_input, arr.end());
    return bitstring_to_byte(remainder);
}
/*
 * BITMASK, how it works
 *
 * example:
 * if seq = 70 -> in binary 01000110
 * seq & 0x3F = 01000110 & 00111111 = 00000110 = 6
 *
 * It prevents sequence, type and size of the frame from exceeding 6 bits, 5 bits and 5 bits respectively
 * prevents them from overflowing
 *
 * But it was baaaad, because it crashed without warning. So i changed it
 */
 // it is using 3 bytes and can only use 2. The bitmask is not working
int build_frame(Frame *f, uint8_t seq, MessageType msgtype, uint8_t *data, uint8_t data_size){
    if (seq > 63) {
        fprintf(stderr, "build_frame: sequence %d out of range(0-63)\n", seq);
        log("PROTOCOLO", "ERRO", "build_frame: sequence out of range(0-63)");
        return -1;
    }
    if ((int)msgtype > 31) {
        fprintf(stderr, "build_frame: type %d out of range (0-31)\n",msgtype);
        log("PROTOCOLO", "ERRO", "build_frame: sequence out of range(0-31)");
        return -1;
    }
    if (data_size > DATA_SIZE) {
        fprintf(stderr, "build_frame: data_size %d exceeds max %d\n",data_size, DATA_SIZE);
        log("PROTOCOLO", "ERRO", "build_frame: data_size exceeds max");
        return -1;
    }

    f->marker   = MARKER;
    f->sequence = seq;
    f->type     = (uint8_t)msgtype;
    f->size     = data_size;

    memset(f->data, 0, DATA_SIZE);
    if (data && data_size > 0)
        memcpy(f->data, data, data_size);
    f->CRC = 0;
    f->CRC = calc_CRC(f); //

    return 0;
}

int send_frame(int sock, Frame *f, unsigned char src_mac[6], unsigned char dest_mac[6], const char* iface){

    //printf("Estou mandando seq: %d, type: %d\n", f->sequence, f->type); 

    /* This is necessary even for two PCs connected to the same network cable
     * because RAWsocket operates at layer 2, enlace, it sends and receives full ethernet frames.
     * The network card requires a valid Ethernet header to know where to send the frame and what to do with it     */
    unsigned char eth_frame[14 + sizeof(Frame)];
    memcpy(eth_frame, dest_mac, 6);
    memcpy(eth_frame + 6, src_mac, 6);

    eth_frame[12] = 0x08;
    eth_frame[13] = 0x88;
    memcpy(eth_frame + 14, f, sizeof(Frame));

    struct sockaddr_ll addr = {0};
    addr.sll_ifindex = if_nametoindex(iface);
    addr.sll_halen = ETH_ALEN;
    memcpy(addr.sll_addr, dest_mac, 6);

    return sendto(sock, eth_frame, sizeof(eth_frame), 0, (struct sockaddr*)&addr, sizeof(addr));
}

int recv_frame(int sock, Frame* f, unsigned char src_mac[6], unsigned char dest_mac[6], const char* iface, uint8_t exp_seq){
    unsigned char eth_frame[14 + sizeof(Frame)];
    int bytes = recv(sock, eth_frame, sizeof(eth_frame), 0);

    if (bytes < (int)(14 + sizeof(Frame))){
        return -1;
    }

    if (eth_frame[12] != 0x08 || eth_frame[13] != 0x88){
        log("PROTOCOLO", "ERRO", "frame errada");
        return -1;
    }

    memcpy(f, eth_frame + 14, sizeof(Frame));

    if (f->marker != MARKER){
        log("PROTOCOLO", "ERRO", "marker errada");
        return -1;
    }
    // printf("Marker - Type: %d - %d\n",f->marker, f->type);

    //if(f->type == MSG_TXT || f->type == MSG_JPG || f->type == MSG_MP4)
       // printf("Debug [recv_frame] seq: %d, exp: %d\n", f->sequence, exp_seq);


    //printf("Sequencia recebida, seq exp, msg_type -> %d, %d, %d\n", f->sequence, exp_seq, f->type);
    
    uint8_t expected = calc_CRC(f);

    if ((f->type != MSG_ACK) && (f->type != MSG_NACK) && (src_mac != NULL)) {
        // eu acho que ele tá recebendo uma sequencia e dai outra coisa acontece como receber uma tecla e ele não zera a sequencia 
        // oq ele pode receber no meio de uma transmissão alem de direcao? 
        //printf("seq, exp_seq: %d, %d\n", f->sequence, exp_seq);
        if (((expected != f->CRC) || (exp_seq != f->sequence)) && (f->type != MSG_DOWN) && (f->type != MSG_UP) && (f->type != MSG_LEFT) && (f->type != MSG_RIGHT) && (f->type != MSG_END)) {
            send_nack(sock, exp_seq, src_mac, dest_mac, iface);
        }
        else {
            send_ack(sock, f->sequence, src_mac, dest_mac, iface);
        }
    }
        
    // timeout
    if (bytes < 0) {
        // não chegou nada
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            log("PROTOCOLO", "ERRO", "EAGAIN ou EWOULDBL");
            return -1;
        }

        // deu erro
        log("PROTOCOLO", "ERRO", "Erro no recv");
        perror("Erro no recv!\n");
        return -2;
    }

    return 0;
}

// Eu fiz outra função pq fiquei com medo do q ia acontecer na função recursiva
int send(int sock, Frame *f, unsigned char src_mac[6], unsigned char dest_mac[6], const char* iface, uint8_t exp_seq){
    Frame f_recv;
    int retrans = 5;

    // ele vai receber um ack e mandar um ack por cima
    while (retrans > 0) {

        if (send_frame(sock, f, src_mac, dest_mac, iface) > 0) {
            //printf("conseguiu enviar\n");
            // espera o ACK
            int recv = recv_frame(sock, &f_recv, NULL, NULL, NULL, exp_seq);
            //printf("recv_frame tá devolvendo: %d\n", recv); 
            if (recv == 0 && f_recv.type == MSG_ACK) {
               return 0;
            }
            
        }
        //printf("recebi type: %d\n", f_recv.type);
        retrans--;
    }
    
    return -1;
}



int send_ack(int sock, uint16_t seq, uint8_t *src_mac, uint8_t *dest_mac, const char* iface){
    //printf("Debug: [send_ack]\n");
    Frame f;

    //log("PROTOCOLO", "INFO", "Enviando ack");

    build_frame(&f, seq, MSG_ACK, nullptr, 0);
    return send_frame(sock, &f, src_mac, dest_mac, iface);
}

int send_nack(int sock, uint16_t seq, uint8_t *src_mac, uint8_t *dest_mac, const char* iface){
    Frame f;

    //log("PROTOCOLO", "INFO", "Enviando nack");

    build_frame(&f, seq, MSG_NACK, nullptr, 0);
    return send_frame(sock, &f, src_mac, dest_mac, iface);
}

int send_init(int sock, uint8_t *map) {
    Frame f_send;
    if (build_frame(&f_send, 0, MSG_INIT, map, 1) != 0){
        log("PROTOCOLO", "INFO", "Erro ao montar frame");
        return -1;
    }

    log("PROTOCOLO", "ERRO", "Enviando init");
    return send_frame(sock, &f_send, CLIENT, SERVER, INTERFACE_CLIENT) < 0 ? -1 : 0;
}



/*
 *
 * 2. i dont get these values, how does this actually works?
 *     f->sequence = seq & 0x3F; // clamp to 6 bits
 *     f->type = msgtype & 0x1F; // clamp to 5 bits
 *     f->size = data_size & 0x1F; // clamp to 5 bits
 *
 * 3. Is this really necessary? Since it is just two computers liked by a network cable
 * unsigned char eth_frame[14 + sizeof(Frame)];
 memcpy(eth_frame, dest_mac, 6);
 memset(eth_frame + 6, 0x00, 6);

 eth_frame[12] = 0x08;
 eth_frame[13] = 0x88;


 server recebe que quer mundo
 e manda ack
 ai recebe ack só né
 */
