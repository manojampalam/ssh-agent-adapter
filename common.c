#include "common.h"

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