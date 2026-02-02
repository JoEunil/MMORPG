#pragma once
#include <WinSock2.h>
#include <Windows.h>
#pragma comment(lib, "Ws2_32.lib") 
//link winsocket library

struct ST_WSA_INITIALIZER
{
	ST_WSA_INITIALIZER(void)
	{
		WSAData wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
		// winsock version 2.2
	}
	~ST_WSA_INITIALIZER(void)
	{
		WSACleanup();
	}
};