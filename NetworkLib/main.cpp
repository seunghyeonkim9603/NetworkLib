#include "stdafx.h"
#include "Listener.h"

#define SERVER_PORT (6000)

unsigned char gFixedKey = 0xa9;

void Encode(char* outEncoded, const unsigned int len, const unsigned char randKey)
{
	unsigned char scalar = 0;
	unsigned char encoded = 0;

	for (unsigned int i = 0; i < len; ++i)
	{
		scalar = outEncoded[i] ^ (randKey + scalar + i + 1);
		encoded = scalar ^ (gFixedKey + encoded + i + 1);

		outEncoded[i] = encoded;
	}
}

void Decode(char* outDecoded, const unsigned int len, const unsigned char randKey)
{
	unsigned char scalar = 0;
	unsigned char encodeKey = 0;

	for (unsigned int i = 0; i < len; ++i)
	{
		unsigned char temp = outDecoded[i] ^ (randKey + scalar + i + 1);

		outDecoded[i] = temp ^ (gFixedKey + encodeKey + i + 1);

		scalar = outDecoded[i] ^ (randKey + scalar + i + 1);
		encodeKey = scalar ^ (gFixedKey + encodeKey + i + 1);
	}
}

int main(void)
{

	char data[] = "aaaaaaaaaabbbbbbbbbbcccccccccc1234567890abcdefghijklmn";
	char* p = data;

	Encode(data, 55, 0x31);

	Decode(data, 55, 0x31);

	p = data;

	//DWORD num = GetTickCount();

	
	/*CCrashDump::Init();

	LanServer server;
	Listener listener(&server);

	LARGE_INTEGER frep;
	LARGE_INTEGER before;
	LARGE_INTEGER cur;

	QueryPerformanceFrequency(&frep);

	server.TryRun(INADDR_ANY, SERVER_PORT, 16, 4, 3000, true, &listener);

	QueryPerformanceCounter(&before);

	while (true)
	{
		QueryPerformanceCounter(&cur);
		if (frep.QuadPart <= cur.QuadPart - before.QuadPart)
		{
			std::cout << "current session count : " << server.GetCurrentSessionCount() << std::endl;
			before = cur;
		}
		Sleep(100);
	}
	server.Terminate();

	return 0;*/
}


