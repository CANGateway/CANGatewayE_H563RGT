#pragma once

#include "stmbed/can.hpp"

namespace cannelloni {

/* flags for CAN ID*/
#define CAN_EFF_FLAG 0x80000000U
#define CAN_RTR_FLAG 0x40000000U
#define CAN_ERR_FLAG 0x20000000U

/* frame formats for CAN ID */
#define CAN_SFF_MASK 0x000007FFU
#define CAN_EFF_MASK 0x1FFFFFFFU
#define CAN_ERR_MASK 0x1FFFFFFFU

namespace tcp_protocol {

const char HANDSHAKE[] = "CANNELLONIv1";

size_t build_packet(uint8_t *buffer, const std::vector<stmbed::CANMessage> &frames) {
    uint8_t *payload;
    uint32_t can_id;
    int i;
    size_t packet_size = 0;

    uint16_t frame_count = frames.size();

    payload = buffer;

    for (i = 0; i < frame_count; i++) {
        can_id = frames[i].id;
        if (frames[i].format == stmbed::CANFormat::CANExtended) { // extended frame format
            can_id |= CAN_EFF_FLAG;
        }
        // if (frames[i].flags & CAN_MSG_FLAG_RTR) { // remote transmission request
        //     can_id |= CAN_RTR_FLAG;
        // }

        can_id = htonl(can_id); // converts u_long to TCP/IP network byte order

        memcpy(payload, &can_id, sizeof(can_id));
        payload += sizeof(can_id);

        *payload = frames[i].size;
        payload += sizeof(frames[i].size);

        // if ((frames[i].flags & CAN_MSG_FLAG_RTR) == 0) {
        memcpy(payload, frames[i].data.data(), frames[i].size);
        payload += frames[i].size;
        // }
    }
    packet_size = payload - buffer;
    return packet_size;
}

// true: success, false: fail
bool parse_packet(std::vector<stmbed::CANMessage> &frames, const uint8_t *buffer, const size_t buffer_size) {
    const uint8_t *payload;
    stmbed::CANFormat format;
    stmbed::CANMessage frame;
    // stmbed::CANType type;
    uint32_t can_id;
    uint8_t size;
    int i;
    size_t packet_size = 0;

    frames.clear();

    payload = buffer;

    // for (i = 0; i < frame_count; i++) {
    while (payload < buffer + buffer_size) {
        memcpy(&can_id, payload, sizeof(can_id));
        payload += sizeof(can_id);
        can_id = ntohl(can_id);

        memcpy(&size, payload, sizeof(size));
        payload += sizeof(size);

        format = stmbed::CANFormat::CANStandard;
        if (can_id & CAN_EFF_FLAG) {
            can_id &= CAN_EFF_MASK;
            format = stmbed::CANFormat::CANExtended;
        }

        frame.id = can_id;
        frame.size = size;
        frame.format = format;
        memcpy(frame.data.data(), payload, size);

        // type = stmbed::CANType::CANData;
        // if (can_id & CAN_RTR_FLAG) {
        //     can_id &= CAN_ERR_MASK;
        //     type = stmbed::CANType::CANRemote;
        // }

        frames.push_back(frame);
        payload += size;
    }
    return true;
}

} // namespace tcp_protocol

namespace udp_protocol {
static const uint8_t CANNELLONI_FRAME_VERSION = 2;
static const size_t CANNELLONI_DATA_PACKET_BASE_SIZE = 5;
static const size_t CANNELLONI_FRAME_BASE_SIZE = 5;
static const size_t CANNELLONI_UDP_RX_PACKET_BUF_LEN = 1600;
static const size_t CANNELLONI_UDP_TX_PACKET_BUF_LEN = 1600;

struct __attribute__((__packed__)) CannelloniDataPacket {
    uint8_t version;
    uint8_t op_code;
    uint8_t seq_no;
    uint16_t count;
};

enum op_codes { DATA = 0, ACK = 1, NACK = 2 };

// static uint8_t seq_no = 0;

// size_t cannelloni_build_packet_header(uint8_t *buffer, const std::vector<stmbed::CANMessage> &frames) {
//     // POSIX alternative ssize_t or Standard C size_t
//     // CannelloniDataPacket *snd_hdr;
//     uint8_t *payload;
//     uint32_t can_id;
//     int i;
//     size_t packet_size = 0;

//     uint16_t frame_count = frames.size();

//     // snd_hdr = (CannelloniDataPacket *)buffer;
//     // snd_hdr->count = 0;
//     // snd_hdr->op_code = DATA;
//     // snd_hdr->seq_no = seq_no++;
//     // snd_hdr->version = CANNELLONI_FRAME_VERSION;
//     // snd_hdr->count = htons(frame_count); // normal use with htons(): converts u_short to TCP/IP network byte order
//     // snd_hdr->count = frame_count;

//     payload = buffer; // + CANNELLONI_DATA_PACKET_BASE_SIZE;

//     for (i = 0; i < frame_count; i++) {
//         can_id = frames[i].id;
//         if (frames[i].format == stmbed::CANFormat::CANExtended) { // extended frame format
//             can_id |= CAN_EFF_FLAG;
//         }
//         // if (frames[i].flags & CAN_MSG_FLAG_RTR) { // remote transmission request
//         //     can_id |= CAN_RTR_FLAG;
//         // }

//         can_id = htonl(can_id); // converts u_long to TCP/IP network byte order

//         memcpy(payload, &can_id, sizeof(can_id));
//         payload += sizeof(can_id);

//         *payload = frames[i].size;
//         payload += sizeof(frames[i].size);

//         // if ((frames[i].flags & CAN_MSG_FLAG_RTR) == 0) {
//         memcpy(payload, frames[i].data.data(), frames[i].size);
//         payload += frames[i].size;
//         // }
//     }
//     packet_size = payload - buffer;
//     return packet_size;
// }

// // true: success, false: fail
// bool cannelloni_parse_packet(std::vector<stmbed::CANMessage> &frames, const uint8_t *buffer, const size_t
// buffer_size) {
//     // const CannelloniDataPacket *rcv_hdr;
//     const uint8_t *payload;
//     stmbed::CANFormat format;
//     stmbed::CANMessage frame;
//     // stmbed::CANType type;
//     uint32_t can_id;
//     uint8_t size;
//     int i;
//     size_t packet_size = 0;

//     // rcv_hdr = (CannelloniDataPacket *)buffer;
//     // if (rcv_hdr->version != CANNELLONI_FRAME_VERSION) {
//     //     return false;
//     // }

//     frames.clear();

//     payload = buffer; // + CANNELLONI_DATA_PACKET_BASE_SIZE;

//     // uint16_t frame_count = ntohs(rcv_hdr->count);

//     // for (i = 0; i < frame_count; i++) {
//     while (payload < buffer + buffer_size) {
//         memcpy(&can_id, payload, sizeof(can_id));
//         payload += sizeof(can_id);
//         can_id = ntohl(can_id);

//         memcpy(&size, payload, sizeof(size));
//         payload += sizeof(size);

//         format = stmbed::CANFormat::CANStandard;
//         if (can_id & CAN_EFF_FLAG) {
//             can_id &= CAN_EFF_MASK;
//             format = stmbed::CANFormat::CANExtended;
//         }

//         frame.id = can_id;
//         frame.size = size;
//         frame.format = format;
//         memcpy(frame.data.data(), payload, size);

//         // type = stmbed::CANType::CANData;
//         // if (can_id & CAN_RTR_FLAG) {
//         //     can_id &= CAN_ERR_MASK;
//         //     type = stmbed::CANType::CANRemote;
//         // }

//         frames.push_back(frame);
//         payload += size;
//     }
//     return true;
// }

} // namespace udp_protocol

} // namespace cannelloni