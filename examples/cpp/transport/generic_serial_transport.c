#include "zcm/zcm.h"
#include "zcm/transport.h"
#include "generic_serial_transport.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MTU 128
#define BUFFER_SIZE 5*MTU+5*ZCM_CHANNEL_MAXLEN
#define ESCAPE_CHAR (0xcc)

// Framing (size = 8 + chan_len + data_len)
//   0xCC
//   0x00
//   chan_len
//   data_len  (4 bytes)
//   *chan
//   *data
//   sum(*chan, *data)
#define FRAME_BYTES 8

// Note: there is little to no error checking in this, misuse will cause problems
typedef struct circBuffer_t circBuffer_t;
struct circBuffer_t
{
    uint8_t data[BUFFER_SIZE];
    int front;
    int back;
};

void cb_init(circBuffer_t* cb)
{
    cb->front = 0;
    cb->back  = 0;
}

int cb_size(circBuffer_t* cb)
{
    if (cb->back >= cb->front)
        return cb->back - cb->front;
    else
        return BUFFER_SIZE - (cb->front - cb->back);
}

int cb_room(circBuffer_t* cb)
{
    return BUFFER_SIZE - 1 - cb_size(cb);
}

void cb_push(circBuffer_t* cb, uint8_t d)
{
    cb->data[cb->back++] = d;
    if (cb->back >= BUFFER_SIZE) cb->back = 0;
}

uint8_t cb_top(circBuffer_t* cb, uint32_t offset)
{
    int val = cb->front + offset;
    if (val >= BUFFER_SIZE) val -= BUFFER_SIZE;
    return cb->data[val];
}

void cb_pop(circBuffer_t* cb, uint32_t num)
{
    cb->front += num;
    if (cb->front >= BUFFER_SIZE) cb->front -= BUFFER_SIZE;
}

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
uint32_t cb_flush_out(circBuffer_t* cb, uint32_t (*write)(const uint8_t* data, uint32_t num))
{
	uint32_t written = 0;
	uint32_t n;

    uint32_t f2b = MIN(BUFFER_SIZE - cb->front, cb_size(cb));
    uint32_t b2e = cb_size(cb) - f2b;

    n = write(cb->data + cb->front, f2b);
    written += n;
    cb_pop(cb, n);

    if (written != f2b) return written;

    n = write(cb->data, b2e);
    written += n;
    cb_pop(cb, n);
    return written;
}

uint32_t cb_flush_in(circBuffer_t* cb, uint32_t bytes,
		uint32_t (*read)(uint8_t* data, uint32_t num))
{
	uint32_t bytesRead = 0;
	uint32_t n;

    // First, don't bother trying to read more bytes than there are room for
    bytes = MIN(bytes, cb_room(cb));

    // Next, find out how much room is left between back and end of buffer or back and front
    // of buffer. Because we already know there's room for whatever we're about to place,
    // if back < front, we can just read in every byte starting at "back".
    if (cb->back < cb->front) {
    	bytesRead += read(cb->data + cb->back, bytes);
        cb->back += bytesRead;
        return bytesRead;
    }

    // Otherwise, we need to be a bit more careful about overflowing the back of the buffer.
    uint32_t b2b = MIN(BUFFER_SIZE - cb->back, bytes);
    uint32_t b2e = bytes - b2b;

    n = read(cb->data + cb->back, b2b);
    bytesRead += n;
    cb->back += n;
    if (n != b2b) return bytesRead;

    if (cb->back >= BUFFER_SIZE) cb->back = 0;
    n = read(cb->data, b2e);
    bytesRead += n;
    cb->back += n;
    return bytesRead;
}

#undef MIN

typedef struct zcm_trans_generic_serial_t zcm_trans_generic_serial_t;
struct zcm_trans_generic_serial_t
{
    zcm_trans_t trans; // This must be first to preserve pointer casting

    circBuffer_t sendBuffer;
    circBuffer_t recvBuffer;
    char         recvChanName[ZCM_CHANNEL_MAXLEN+1];
    char         recvMsgData[MTU];

    uint32_t (*get)(uint8_t* data, uint32_t nData);
    uint32_t (*put)(const uint8_t* data, uint32_t nData);
};

size_t get_mtu(zcm_trans_generic_serial_t *zt)
{
    return MTU;
}

int sendmsg(zcm_trans_generic_serial_t *zt, zcm_msg_t msg)
{
    size_t chan_len = strlen(msg.channel);
    size_t nPushed = 0;

    if (chan_len > ZCM_CHANNEL_MAXLEN)
        return ZCM_EINVALID;

    if (msg.len > MTU)
        return ZCM_EINVALID;

    if (FRAME_BYTES + chan_len + msg.len > cb_room(&zt->sendBuffer))
        return ZCM_EAGAIN;

    cb_push(&zt->sendBuffer, ESCAPE_CHAR); nPushed++;
    cb_push(&zt->sendBuffer, 0x00); nPushed++;
    cb_push(&zt->sendBuffer, chan_len); nPushed++;

    uint32_t len = (uint32_t)msg.len;
    cb_push(&zt->sendBuffer, (len>>24)&0xff); nPushed++;
    cb_push(&zt->sendBuffer, (len>>16)&0xff); nPushed++;
    cb_push(&zt->sendBuffer, (len>>8)&0xff ); nPushed++;
    cb_push(&zt->sendBuffer, (len>>0)&0xff ); nPushed++;

    uint8_t sum = 0;
    int i;
    for (i = 0; i < chan_len; ++i) {
        char c = msg.channel[i];

        cb_push(&zt->sendBuffer, c); nPushed++;

        if (c == ESCAPE_CHAR) {
        	// the escape character doesn't count, so we have chan_len - i characters
        	// remaining.
            if (cb_room(&zt->sendBuffer) > chan_len - i + msg.len + 1) {
                cb_push(&zt->sendBuffer, c); nPushed++;
            } else {
                cb_pop(&zt->sendBuffer, nPushed);
                return ZCM_EAGAIN;
            }
        }

        sum += c;
    }

    for (i = 0; i < msg.len; ++i) {
        char c = msg.buf[i];

        cb_push(&zt->sendBuffer, c); nPushed++;

        if (c == ESCAPE_CHAR) {
        	// the escape character doesn't count, so we have msg.len - i characters
        	// remaining.
            if (cb_room(&zt->sendBuffer) > msg.len - i + 1) {
                cb_push(&zt->sendBuffer, c); nPushed++;
            } else {
                cb_pop(&zt->sendBuffer, nPushed);
                return ZCM_EAGAIN;
            }
        }

        sum += c;
    }

    cb_push(&zt->sendBuffer, sum); nPushed++;

    return ZCM_EOK;
}

int recvmsg_enable(zcm_trans_generic_serial_t *zt, const char *channel, bool enable)
{
    // NOTE: not implemented because it is unlikely that a microprocessor is
    //       going to be hearing messages on a USB comms that it doesn't want
    //       to hear
    return ZCM_EOK;
}

int recvmsg(zcm_trans_generic_serial_t *zt, zcm_msg_t *msg, int timeout)
{
    int incomingSize = cb_size(&zt->recvBuffer);
    if (incomingSize < FRAME_BYTES)
        return ZCM_EAGAIN;

    uint32_t consumed = 0;

    // Sync
    if (cb_top(&zt->recvBuffer, consumed++) != ESCAPE_CHAR)
        goto fail;
    if (cb_top(&zt->recvBuffer, consumed++) != 0x00)
        goto fail;

    // Msg sizes
    uint8_t chan_len = cb_top(&zt->recvBuffer, consumed++);
    msg->len  = cb_top(&zt->recvBuffer, consumed++) << 24;
    msg->len |= cb_top(&zt->recvBuffer, consumed++) << 16;
    msg->len |= cb_top(&zt->recvBuffer, consumed++) << 8;
    msg->len |= cb_top(&zt->recvBuffer, consumed++);

    if (chan_len > ZCM_CHANNEL_MAXLEN)
        goto fail;
    if (msg->len > MTU)
        goto fail;

    if (incomingSize < FRAME_BYTES + chan_len + msg->len)
        return ZCM_EAGAIN;

    memset(&zt->recvChanName, '0', ZCM_CHANNEL_MAXLEN);

    uint8_t sum = 0;
    int i;
    for (i = 0; i < chan_len; ++i) {

        char c = cb_top(&zt->recvBuffer, consumed++);

        if (c == ESCAPE_CHAR) {
        	// the consumed escape character effectively doesn't count,
        	// so we have yet to consume chan_len - i bytes in the channel
            if (consumed + chan_len - i + msg->len + 1 > incomingSize)
                return ZCM_EAGAIN;

            c = cb_top(&zt->recvBuffer, consumed++);

            if (c != ESCAPE_CHAR) {
                consumed-=2;
                goto fail;
            }
        }

        zt->recvChanName[i] = c;
        sum += c;
    }

    zt->recvChanName[chan_len] = '\0';

    for (i = 0; i < msg->len; ++i) {
        if (consumed > incomingSize)
            return ZCM_EAGAIN;

        char c = cb_top(&zt->recvBuffer, consumed++);

        // We got a byte that doesn't count toward our msg len. Need to make
        // sure we still have enough bytes in the incoming buffer.
        if (c == ESCAPE_CHAR) {
            if (consumed + msg->len - i + 1 > incomingSize)
                return ZCM_EAGAIN;

            c = cb_top(&zt->recvBuffer, consumed++);

            if (c != ESCAPE_CHAR) {
                consumed-=2;
                goto fail;
            }
        }

        zt->recvMsgData[i] = c;
        sum += c;
    }

    uint8_t expected = cb_top(&zt->recvBuffer, consumed++);

    if (expected == sum) {
        msg->channel = zt->recvChanName;
        msg->buf     = zt->recvMsgData;
        cb_pop(&zt->recvBuffer, consumed);
        return ZCM_EOK;
    }

  fail:
    cb_pop(&zt->recvBuffer, consumed);
    return recvmsg(zt, msg, timeout);
}

int update(zcm_trans_generic_serial_t *zt)
{
    cb_flush_in(&zt->recvBuffer, cb_room(&zt->recvBuffer), zt->get);
    cb_flush_out(&zt->sendBuffer, zt->put);

    return ZCM_EOK;
}

/********************** STATICS **********************/
static zcm_trans_generic_serial_t *cast(zcm_trans_t *zt);

static size_t _get_mtu(zcm_trans_t *zt)
{ return get_mtu(cast(zt)); }

static int _sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
{ return sendmsg(cast(zt), msg); }

static int _recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable)
{ return recvmsg_enable(cast(zt), channel, enable); }

static int _recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)
{ return recvmsg(cast(zt), msg, timeout); }

static int _update(zcm_trans_t *zt)
{ return update(cast(zt)); }

static void _destroy(zcm_trans_t *zt)
{ free(cast(zt)); }

static zcm_trans_methods_t methods = {
    &_get_mtu,
    &_sendmsg,
    &_recvmsg_enable,
    &_recvmsg,
    &_update,
    &_destroy,
};

static zcm_trans_generic_serial_t *cast(zcm_trans_t *zt)
{
    assert(zt->vtbl == &methods);
    return (zcm_trans_generic_serial_t*)zt;
}

// Because we are doing the generic_serial_init() in here, this can only be called once
zcm_trans_t *zcm_trans_generic_serial_create(
        uint32_t (*get)(uint8_t* data, uint32_t nData),
        uint32_t (*put)(const uint8_t* data, uint32_t nData))
{
    zcm_trans_generic_serial_t *zt = malloc(sizeof(zcm_trans_generic_serial_t));
    zt->trans.trans_type = ZCM_NONBLOCKING;
    zt->trans.vtbl = &methods;
    cb_init(&zt->sendBuffer);
    cb_init(&zt->recvBuffer);

    zt->get = get;
    zt->put = put;

    return (zcm_trans_t*) zt;
}
