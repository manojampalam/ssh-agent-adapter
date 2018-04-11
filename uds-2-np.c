#include "common.h"

int uds_agent_port = 47010;

void process_pipe_connection(connection* con) {
	WSADATA wsaData;
	struct sockaddr_in serv_addr;

	WSAStartup(MAKEWORD(2, 2), &wsaData);
	con->sock = socket(AF_INET, SOCK_STREAM, 0);
	ZeroMemory(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(uds_agent_port);
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

	connect(con->sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

	//TODO - write cookie

	//start threads
	con->activity_count = 2;
	con->uds2np_th = CreateThread(NULL, 0, uds2np_thread, con, 0, NULL);
	con->np2uds_th = CreateThread(NULL, 0, np2uds_thread, con, 0, NULL);
}

int wmain(int argc, wchar_t *argv[], wchar_t *envp[])
{
	wchar_t pipe_name[MAX_PATH];
	OVERLAPPED ol;

	//spawn a child go give back control to cmd
	if (argc == 1) {
		char cmdline[MAX_PATH];
		STARTUPINFOA si;
		PROCESS_INFORMATION pi;

		//read SSH_AUTH_SOCK
		wchar_t* auth_sock = "filepath";
		auth_sock = getenv("SSH_AUTH_SOCK");
		cmdline[0] = '\0';
		strcat(cmdline, "E:\\temp\\sample.exe");
		strcat(cmdline, " ");
		strcat(cmdline, auth_sock);
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(STARTUPINFO);
		ZeroMemory(&pi, sizeof(pi));
		CreateProcessA(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
		swprintf_s(pipe_name, MAX_PATH, L"SSH_AUTH_SOCK=\\\\.\\pipe\\usd-2-np-%d", pi.dwProcessId);
		_wputenv(pipe_name);
		printf("%ls", pipe_name);
	}

	//TODO - parse contents of file pointed by argv[1]
	// and read port and cookie info

	swprintf_s(pipe_name, MAX_PATH, L"\\\\.\\pipe\\usd-2-np-%d", GetCurrentProcessId());
	ZeroMemory(&ol, sizeof(ol));
	ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	printf("pipe: %ls", pipe_name);
	while (1) {
		connection *con = (connection*)malloc(sizeof(connection));
		ZeroMemory(con, sizeof(connection));
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

