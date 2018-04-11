#define _WIN32_WINNT 0x600
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#define DEFAULT_BUFLEN 512


typedef struct _connection {
	HANDLE pipe;
	SOCKET sock;
	LONG volatile activity_count;
	HANDLE uds2np_th;
	HANDLE np2uds_th;
	int abort_io;
}connection;

#define BUFSIZE 5*1024


DWORD WINAPI uds2np_thread(LPVOID param);
DWORD WINAPI np2uds_thread(LPVOID param);

