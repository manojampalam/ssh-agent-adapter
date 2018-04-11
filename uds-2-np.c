#include "common.h"


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

