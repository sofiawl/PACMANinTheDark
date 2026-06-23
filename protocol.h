// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <stdint.h>

#define DATA_SIZE 31
#define TIMEOUT   1000

#define RECV_TIMEOUT        -1
#define RECV_ERROR          -2
#define NETWORK_GIVEUP_TRIES 300

#define UFPR_MAP    1
#define DEFAULT_MAP 2

// quantas vezes ele quer retransmitir antes de dar timeout?
#define MAX_RETRANS 1000


// Loopback
/*
#define INTERFACE_SERVER "veth1"
#define INTERFACE_CLIENT "veth0"
extern unsigned char SERVER[6]; // = {0x9e, 0x82, 0xe1, 0x6d, 0x4d, 0x6c}
extern unsigned char CLIENT[6]; // = {0x52, 0x07, 0x95, 0x37, 0x1a, 0xc7}
*/

//Sofia
#define INTERFACE_SERVER "enp4s0"
#define INTERFACE_CLIENT "enx00e04c034558"
extern unsigned char SERVER[6];
extern unsigned char CLIENT[6];

typedef enum {
    MSG_ACK = 0,
    MSG_NACK = 1,
    MSG_VIS = 2,
    MSG_INIT = 3,
    MSG_DATA = 4,
    MSG_TXT = 5,
    MSG_JPG = 6,
    MSG_MP4 = 7,
    MSG_OVER = 8,
    MSG_GHOSTS = 9,
    MSG_RIGHT = 10,
    MSG_LEFT = 11,
    MSG_UP = 12,
    MSG_DOWN = 13,
    MSG_WORLD = 14,
    MSG_ERROR = 15,
    MSG_END = 16,
    MSG_RESYNC = 17,
} MessageType;

// Bit-Fields insted of Bit-Masking
// packed assures that no padding is in between bytes 
typedef struct __attribute__((packed)) {
    uint8_t marker;        

    uint16_t size     : 5; 
    uint16_t sequence : 6; 
    uint16_t type     : 5; 

    uint8_t data[DATA_SIZE]; 
    uint8_t CRC;             
} Frame;

int create_raw_socket(const char* network_interface_name);

uint8_t calc_CRC(Frame *f);

int build_frame(Frame *f, uint8_t seq, MessageType msgtype, uint8_t *data, uint8_t data_size);

int send_frame(int sock, Frame *f, unsigned char src_mac[6], unsigned char dest_mac[6], const char* iface);

int recv_frame(int sock, Frame* f, unsigned char src_mac[6], unsigned char dest_mac[6], const char* iface, uint8_t exp_seq);

int send_ack(int sock, uint16_t seq, uint8_t *src_mac, uint8_t *dest_mac, const char* iface);

int send_nack(int sock, uint16_t seq, uint8_t *src_mac, uint8_t *dest_mac, const char* iface);

int send(int sock, Frame *f, unsigned char src_mac[6], unsigned char dest_mac[6], const char* iface, uint8_t exp_seq);

int send_init(int sock, uint8_t *data);

bool network_recv_failed(int rv, int *fail_count, const char *origem);

#endif
