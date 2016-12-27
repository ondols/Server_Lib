#pragma once

// packet enum list.
enum ePACKET_BASE
{
	ePACKET_SIZE = 2,
	ePACKET_ID = 2,
	ePACKET_HEADER_SIZE = ePACKET_SIZE + ePACKET_ID,
	ePACKET_RECEIVE_BUF_SIZE = 40960,
};

typedef bool(*pPACKET_HANDLE_FUN)(class CBBaseSession* session, char* data);

// packet base header.
struct sPacketHeader
{
	unsigned short size;
	unsigned short packetid;

	sPacketHeader()
	{
		size = 0;
		packetid = 0;
	}
};

enum eECHO_PACKET_ID
{
	eEcho_Packet_8 = 1000,
	eEcho_Packet_128,
	eEcho_Packet_1024,
};

struct EchoPacket8 : public sPacketHeader
{
	char text[8];

	EchoPacket8()
	{
		packetid = eEcho_Packet_8;
	}
};

struct EchoPacket128 : public sPacketHeader
{
	char text[128];

	EchoPacket128()
	{
		packetid = eEcho_Packet_128;
	}
};

struct EchoPacket1024 : public sPacketHeader
{
	char text[1024];

	EchoPacket1024()
	{
		packetid = eEcho_Packet_1024;
	}
};
