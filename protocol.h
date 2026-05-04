// protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <stdint.h>

#define DATA_SIZE 31
#define TIMEOUT 1000

typedef enum {
    MSG_ACK = 0,
    MSG_NACK = 1,
    MSG_VIS = 2,
    MSG_INIT = 3,
    MSG_DATA = 4,
    MSG_TXT = 5,
    MSG_JPG = 6,
    MSG_MP4 = 7,
    MSG_RIGHT = 10,
    MSG_LEFT = 11,
    MSG_UP = 12,
    MSG_DOWN = 13,
    MSG_ERROR = 15,
    MSG_END = 16,
    MSG_WORLD = 14
} MessageType;

typedef struct {
    uint8_t marker;
    uint8_t size;
    uint8_t sequence;
    uint8_t type;
    uint8_t data[DATA_SIZE];
    uint8_t CRC;
} Frame;

int create_raw_socket(const char* network_interface_name);

uint8_t calc_CRC(Frame *f);

int build_frame(Frame *f, uint8_t seq, MessageType msgtype, uint8_t *data, uint8_t data_size);

int send_frame(int sock, Frame *f, unsigned char src_mac[6], unsigned char dest_mac[6], const char* iface);

int recv_frame(int sock, Frame* f);

int send_ack(int sock, uint16_t seq, uint8_t *src_mac, uint8_t *dest_mac, const char* iface);

int send_nack(int sock, uint16_t seq, uint8_t *src_mac, uint8_t *dest_mac, const char* iface);

#endif
