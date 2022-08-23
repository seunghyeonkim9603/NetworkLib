#pragma once

struct PacketHeader
{
	uint16_t Length;
};

static_assert(sizeof(PacketHeader) == 2, "Invalid [PacketHeader] size\n");