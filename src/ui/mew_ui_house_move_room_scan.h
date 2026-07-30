#pragma once

#include "mew_ui_house_move_probe.h"

uint32_t AcMewMoveRoomMask(
    const uint8_t* bytes,
    size_t byte_count);

void AcMewFindMoveRoomReferences(
    const uint8_t* bytes,
    size_t byte_count,
    uint8_t region,
    AcMewMoveRoomReference* references,
    uint32_t* reference_count);
