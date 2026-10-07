#ifndef PINGO2_BRIDGE_H
#define PINGO2_BRIDGE_H

#include "pingo2_control.h"
#include "sprites.h"
#include "buffers.h"
#include "vdu_stream_processor.h"

struct PingoBitmapOwner {
    std::shared_ptr<Bitmap> bitmap;
    std::shared_ptr<BufferStream> backing;
};
static void pingoDropBitmap(void *opaque) {
    auto owner = static_cast<PingoBitmapOwner *>(opaque);
    owner->~PingoBitmapOwner();
    p2bridge_free(owner);
}
static int pingoBorrowBitmap(P2Commands *owner, uint16_t id) {
    if (pingoIsControl(id)) return P2CMD_INVALID;
    auto bitmap = getBitmap(id);
    auto found = buffers.find(id);
    if (!bitmap || found == buffers.end() || found->second.size() != 1)
        return P2CMD_MISSING;
    auto backing = found->second.front();
    unsigned format = bitmap->format == PixelFormat::RGBA2222 ? 1 :
        bitmap->format == PixelFormat::RGBA8888 ? 4 : 0;
    if (!format || !backing || bitmap->width <= 0 || bitmap->height <= 0 ||
        !bitmap->data || bitmap->data != backing->getBuffer() ||
        size_t(bitmap->width) * bitmap->height > SIZE_MAX / format ||
        backing->size() != size_t(bitmap->width) * bitmap->height * format)
        return P2CMD_INVALID;
    /* FabGL's two accepted formats are tightly packed, with stride=width.
       Retaining the wrapper alone does NOT retain borrowed BufferStream data. */
    void *storage = p2bridge_calloc(1, sizeof(PingoBitmapOwner));
    if (!storage) return P2CMD_ALLOC;
    auto retained = new(storage) PingoBitmapOwner{bitmap, backing};
    int result = p2cmd_borrow_bitmap(owner, id, bitmap->width, bitmap->height,
        format, bitmap->data, backing->size(), retained, pingoDropBitmap);
    if (result != P2CMD_OK) pingoDropBitmap(retained);
    return result;
}
struct PingoNotice { bool ready = false; uint8_t packet[10] = {}; };
static void pingoCollectNotice(void *context, uint16_t, const uint8_t packet[10]) {
    auto notice = static_cast<PingoNotice *>(context);
    memcpy(notice->packet, packet, sizeof notice->packet);
    notice->ready = true;
}
#ifdef PINGO2_BRIDGE_DIAGNOSTICS
static int pingoLastStatus = P2CMD_OK;
static uint16_t pingoLastId = 0;
static unsigned pingoLastCommand = 0;
#endif
static void pingoCommandStatus(uint16_t id, unsigned command, int status) {
#ifdef PINGO2_BRIDGE_DIAGNOSTICS
    pingoLastStatus = status; pingoLastId = id; pingoLastCommand = command;
#endif
    if (status != P2CMD_OK)
        debug_log("Pingo2: id=%u command=%u rejected=%d\n\r", id, command, status);
}

void VDUStreamProcessor::bufferUsePingo2(uint16_t id) {
    int command = readByte_t();
    if (command < 0) return;
    uint8_t prefix[4] = {};
    size_t have = 0, wanted = 0;
    bool upload = (command >= 1 && command <= 4) || command == 40;
    if (upload) {
        for (; have < sizeof prefix; ++have) {
            int value = readByte_t();
            if (value < 0) { pingoCommandStatus(id, command, P2CMD_TRUNCATED); return; }
            prefix[have] = value;
        }
    }
    int shape = p2cmd_payload_length(command, prefix, have, &wanted);
    if (shape == P2CMD_UNSUPPORTED) {
        /* Unassigned opcodes have no knowable payload. Do not invent one. */
        pingoCommandStatus(id, command, shape); return;
    }
    uint8_t small[32] = {23, 0, 0xA0, uint8_t(id), uint8_t(id >> 8), 0x49,
                         uint8_t(command)};
    auto bytes = wanted + 7;
    auto wire = bytes <= sizeof small ? small :
        static_cast<uint8_t *>(p2bridge_calloc(bytes, 1));
    if (wire != small && wire) memcpy(wire, small, 7);
    if (wire && have) memcpy(wire + 7, prefix, have);
    /* Drain ALL known payload bytes even after allocation/semantic failure.
       Stop at the first timeout; never mutate a partially decoded command. */
    bool complete = true;
    for (size_t i = have; i < wanted; ++i) {
        int value = readByte_t();
        if (value < 0) { complete = false; break; }
        if (wire) wire[7 + i] = value;
    }
    if (!complete || !wire) {
        if (wire != small) p2bridge_free(wire);
        pingoCommandStatus(id, command, complete ? P2CMD_ALLOC : P2CMD_TRUNCATED);
        return;
    }
    int result = P2CMD_MISSING;
    size_t consumed = 0;
    auto control = pingoControlFind(id);
    PingoNotice notice;
    if (command == 0 && id != UINT16_MAX) {
        /* Build completely before replacing the old typed/data owner. */
        auto memory = p2bridge_calloc(1, sizeof(PingoControl));
        auto replacement = memory ? new(memory) PingoControl{} : nullptr;
        if (replacement) replacement->owner = p2cmd_create();
        result = replacement && replacement->owner ?
            p2cmd_execute(replacement->owner, wire, bytes, &consumed) : P2CMD_ALLOC;
        if (result == P2CMD_OK) {
            bufferClear(id);
            replacement->id = id;
            auto slot = pingoControlSlot(id);
            replacement->next = *slot; *slot = replacement;
        } else pingoControlRelease(replacement);
    } else if (command == 0) result = P2CMD_INVALID;
    else if (command == 49 || command == 50) result = P2CMD_UNSUPPORTED;
    else if (command == 39) {
        if (control) { bufferClear(id); result = P2CMD_OK; }
    } else if (control) {
        auto owner = control->owner;
        uint16_t bitmapId = 0;
        bool bitmapCommand = command == 5 || command == 38;
        if (bitmapCommand) {
            const auto p = wire + (command == 5 ? 11 : 7);
            bitmapId = uint16_t(p[0]) | uint16_t(p[1]) << 8;
            result = pingoBorrowBitmap(owner, bitmapId);
        } else result = P2CMD_OK;
        if (result == P2CMD_OK) {
            if (command == 38) waitPlotCompletion();
            p2cmd_completion(owner, pingoCollectNotice, &notice);
            result = p2cmd_execute(owner, wire, bytes, &consumed);
            p2cmd_completion(owner, nullptr, nullptr);
        }
        if (bitmapCommand) p2cmd_release_bitmap(owner, bitmapId);
    }
    if (wire != small) p2bridge_free(wire);
    pingoCommandStatus(id, command, result);
    /* send_packet may run user VDP callbacks which clear this control.
       Nothing may dereference the control/owner after this point. */
    if (result == P2CMD_OK && notice.ready)
        send_packet(PACKET_KEYCODE, sizeof notice.packet, notice.packet);
}
#endif
