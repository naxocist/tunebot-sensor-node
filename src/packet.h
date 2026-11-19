// packet.h - one ESP-NOW packet per reading (Build Guide, "Message from node to gateway").
// Keep this file identical in tunebot/src and gateway/src.
#pragma once
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t  node_id;       /* 1 for the first guitar node */
    uint32_t seq;           /* +1 per packet: gaps = lost packets */
    uint8_t  string_index;  /* 0 = low E ... 5 = high E */
    uint8_t  state;         /* 0 no signal, 1 flat, 2 sharp, 3 in tune */
    float    hz;
    float    cents;
    uint8_t  moving;        /* 1 if the MPU6050 sees the guitar moving */
    uint8_t  ai_string;     /* 0-5, 255 = not a string / no AI yet */
    uint8_t  ai_conf;       /* 0-100 % */
} tunebot_packet_t;

_Static_assert(sizeof(tunebot_packet_t) == 18, "packet must stay 18 bytes");

enum { ST_NO_SIGNAL = 0, ST_FLAT = 1, ST_SHARP = 2, ST_IN_TUNE = 3 };
