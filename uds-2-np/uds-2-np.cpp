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

}connection;

#define BUFSIZE 5*1024

static void con_activity_done(connection* con) {
	if (InterlockedDecrement(&(con->activity_count)) == 0) {
		CloseHandle(con->pipe);
		closesocket(con->sock);
		free(con);
	}
}

DWORD WINAPI uds2np_thread(LPVOID param) {
	connection* con = (connection*)param;
	char buf[BUFSIZE];
	DWORD bytes_read, bytes_written;
	//read from pipe and write to sock
	while (ReadFile(con->pipe, buf, BUFSIZE, &bytes_read, NULL)) {
		send(con->sock, buf, bytes_read, 0);
	}

	shutdown(con->sock, SD_SEND);
	con_activity_done(con);
	return 0;
}


DWORD WINAPI np2uds_thread(LPVOID param) {
	connection* con = (connection*)param;
	char buf[BUFSIZE];
	DWORD bytes_read, bytes_written;
	//read from sock and write to pipe
	while ((bytes_read = recv(con->sock, buf, BUFSIZE, 0)) > 0)
		WriteFile(con->pipe, buf, bytes_read, &bytes_written, NULL);

	con_activity_done(con);
	return 0;
}




void process_pipe_connection(connection* con) {
	WSADATA wsaData;
	struct sockaddr_in serv_addr;

	WSAStartup(MAKEWORD(2, 2), &wsaData);
	con->sock = socket(AF_INET, SOCK_STREAM, 0);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(47010);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	connect(con->sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	//start threads
	con->activity_count = 2;
	CloseHandle(CreateThread(NULL, 0, uds2np_thread, con, 0, NULL));
	CloseHandle(CreateThread(NULL, 0, np2uds_thread, con, 0, NULL));
}

int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
	wchar_t pipe_name[MAX_PATH];
	OVERLAPPED ol;

	swprintf_s(pipe_name, L"\\\\.\\pipe\\usd-2-np-%d", GetCurrentProcessId());
	ZeroMemory(&ol, sizeof(ol));
	ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	
	while (1) {
		connection *con = (connection*)malloc(sizeof(connection));
		con->pipe = CreateNamedPipeW(
			pipe_name,		  // pipe name 
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,       // read/write access 
			PIPE_TYPE_BYTE |       // message type pipe 
			PIPE_READMODE_BYTE |   // message-read mode 
			PIPE_WAIT,                // blocking mode 
			PIPE_UNLIMITED_INSTANCES, // max. instances  
			BUFSIZE,                  // output buffer size 
			BUFSIZE,                  // input buffer size 
			0,                        // client time-out 
			NULL);

		ConnectNamedPipe(con->pipe, &ol);
		WaitForSingleObject(ol.hEvent, INFINITE);
		process_pipe_connection(con);
	}



    return 0;
}

