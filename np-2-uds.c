#include "common.h"


void process_sock_connection(connection* con) {
	wchar_t pipe_name[MAX_PATH];
	
	swprintf_s(pipe_name, MAX_PATH, L"\\\\.\\pipe\\openssh-ssh-agent");
	con->pipe = CreateFileW(pipe_name,
		GENERIC_WRITE | GENERIC_READ,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
		NULL);

	//start threads
	con->activity_count = 2;
	con->uds2np_th = CreateThread(NULL, 0, uds2np_thread, con, 0, NULL);
	con->np2uds_th = CreateThread(NULL, 0, np2uds_thread, con, 0, NULL);
}

int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
	WSADATA wsaData;
	SOCKET listen_sock = INVALID_SOCKET;
	struct sockaddr_in serv_addr;
	
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	ZeroMemory(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(47010);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	bind(listen_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	listen(listen_sock, SOMAXCONN);

	while (1) {
		connection *con = (connection*)malloc(sizeof(connection));
		ZeroMemory(con, sizeof(connection));
		con->sock = accept(listen_sock, NULL, NULL);
		process_sock_connection(con);
	}

	return 0;
}

