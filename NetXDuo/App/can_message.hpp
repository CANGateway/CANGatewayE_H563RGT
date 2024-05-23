#pragma once

#include <stdint.h>
#include <string.h>

enum CANBusCh
{
    CAN_BUS1 = 1,
    CAN_BUS2 = 2
};

enum CANIdType
{
    CAN_STANDARD = 0,
    CAN_EXTENDED = 1
};

struct CANMessage
{
    uint32_t id;
    uint8_t data[64];
    uint8_t dlc;
    CANIdType format;
};

struct CANMessageWithCh
{
    CANBusCh bus_ch;
    CANMessage msg;
};

typedef enum
{
    DECODE_SUCCESS = 0,

    DECODE_BUFFER_NULL,
    DECODE_BUFFER_LENGTH_ZERO,
    DECODE_BUFFER_LENGTH_INVALID,
    DECODE_BUS_CH_INVALID,
    DECODE_FRAME_TYPE_INVALID,
    DECODE_ID_INVALID,

} DecodeResult;

static DecodeResult decode_can_message(uint8_t *buffer, uint32_t buffer_length, CANMessageWithCh &out_message, uint32_t *decoded_length)
{
    if (buffer == NULL)
        return DECODE_BUFFER_NULL;

    if (buffer_length == 0)
        return DECODE_BUFFER_LENGTH_ZERO;

    uint8_t bus_ch = buffer[0];

    if (!(bus_ch == CAN_BUS1 || bus_ch || CAN_BUS2))
        return DECODE_BUS_CH_INVALID;

    uint8_t data_len = buffer[1];

    // data_len + bus_ch + frame_type + id + data
    uint8_t expect_buffer_length = 1 + 1 + 1 + 4 + data_len;

    if (expect_buffer_length > buffer_length)
        return DECODE_BUFFER_LENGTH_INVALID;

    uint8_t frame_type = buffer[2];
    if (!(frame_type == CAN_STANDARD || frame_type == CAN_EXTENDED))
        return DECODE_FRAME_TYPE_INVALID;

    uint32_t id = buffer[3] << 24 | buffer[4] << 16 | buffer[5] << 8 | buffer[6];
    if (frame_type == CAN_STANDARD)
    {
        if (id > 0x7FF)
            return DECODE_ID_INVALID;
    }
    else
    {
        if (id > 0x1FFFFFFF)
            return DECODE_ID_INVALID;
    }

    out_message.bus_ch = static_cast<CANBusCh>(bus_ch);
    out_message.msg.id = id;
    out_message.msg.dlc = data_len;
    out_message.msg.format = static_cast<CANIdType>(frame_type);
    memcpy(out_message.msg.data, buffer + 7, data_len);
    *decoded_length = expect_buffer_length;

    return DECODE_SUCCESS;
}

typedef enum
{
    ENCODE_SUCCESS = 0,

    ENCODE_BUFFER_NULL,
    ENCODE_BUFFER_LENGTH_SHORT,
    ENCODE_BUS_CH_INVALID,
    ENCODE_FRAME_TYPE_INVALID,
    ENCODE_ID_INVALID

} encode_result;

static encode_result encode_can_message(CANMessageWithCh &message, uint8_t *out_buffer, uint32_t out_buffer_length, uint32_t *out_length)
{
    if (out_buffer == NULL)
        return ENCODE_BUFFER_NULL;

    uint8_t need_buffer_length = 1 + 1 + 1 + 4 + message.msg.dlc;
    if (out_buffer_length < need_buffer_length)
        return ENCODE_BUFFER_LENGTH_SHORT;

    if (!(message.bus_ch == CAN_BUS1 || message.bus_ch == CAN_BUS2))
        return ENCODE_BUS_CH_INVALID;

    if (!(message.msg.format == CAN_STANDARD || message.msg.format == CAN_EXTENDED))
        return ENCODE_FRAME_TYPE_INVALID;

    if (message.msg.format == CAN_STANDARD)
    {
        if (message.msg.id > 0x7FF)
            return ENCODE_ID_INVALID;
    }
    else
    {
        if (message.msg.id > 0x1FFFFFFF)
            return ENCODE_ID_INVALID;
    }

    out_buffer[0] = message.bus_ch;
    out_buffer[1] = message.msg.dlc;
    out_buffer[2] = message.msg.format;
    out_buffer[3] = (message.msg.id >> 24) & 0xFF;
    out_buffer[4] = (message.msg.id >> 16) & 0xFF;
    out_buffer[5] = (message.msg.id >> 8) & 0xFF;
    out_buffer[6] = message.msg.id & 0xFF;
    memcpy(out_buffer + 7, message.msg.data, message.msg.dlc);

    if (out_length != NULL)
        *out_length = need_buffer_length;

    return ENCODE_SUCCESS;
}
