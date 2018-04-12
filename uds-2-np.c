#include "common.h"

int uds_agent_port = 47010;
char* cookie = NULL;
int cookie_len = 0;

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
	DWORD client_pid;

	//spawn a child and give back control to cmd
	if (argc == 1) {
		wchar_t cmdline[MAX_PATH];
		STARTUPINFOW si;
		PROCESS_INFORMATION pi;

		cmdline[0] = L'\0';
		wcscat(cmdline, argv[0]);
		wcscat(cmdline, L" -z");
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(STARTUPINFOW);
		ZeroMemory(&pi, sizeof(pi));
		CreateProcessW(NULL, cmdline, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi);
		swprintf_s(pipe_name, MAX_PATH, L"SSH_AUTH_SOCK=\\\\.\\pipe\\usd-2-np-%d", pi.dwProcessId);
		_wputenv(pipe_name);
		printf("set %ls", pipe_name);
		return 0;
	}
	else if (argc > 2 || wcslen(argv[1]) > 2) {
		printf("wrong usage\n");
		exit(1);
	}
	else {
		if (wcscmp(argv[1], L"-z") == 0) {
			//if child mode fall through
		}
		else if (wcscmp(argv[1], L"-d") == 0) {
			//if debug mode fall through
			debug_mode = 1;
		}
		else if (wcscmp(argv[1], L"-k") == 0) {
			//kill agent
			printf("kill agent adapter not implemented yet\n");
			printf("identify agent pid from SSH_AUTH_SOCK (trailing number) and kill it yourself :)\n");
			exit(1);
		}
		else {
			//unsupported param
			printf("wrong usage\n");
			exit(1);
		}
	}


	//TODO - parse contents of file pointed by SSH_AUTH_SOCK env variable
	// and read port and cookie info

	swprintf_s(pipe_name, MAX_PATH, L"\\\\.\\pipe\\usd-2-np-%d", GetCurrentProcessId());
	ZeroMemory(&ol, sizeof(ol));
	ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	log("incomig connections to pipe: %ls", pipe_name);
	log("will be routed to 127.0.0.1:%d", uds_agent_port);
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
		GetNamedPipeClientProcessId(con->pipe, &client_pid);
		log("connection accepted from pid:%d", client_pid);
		process_pipe_connection(con);
	}

    return 0;
}

