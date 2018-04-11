#include "common.h"

static void con_activity_done(connection* con) {
	if (InterlockedDecrement(&(con->activity_count)) == 0) {
		CloseHandle(con->pipe);
		closesocket(con->sock);
		CloseHandle(con->np2uds_th);
		CloseHandle(con->uds2np_th);
		free(con);
	}
}

VOID CALLBACK apc_callback(DWORD error, DWORD bytes, LPOVERLAPPED ol) {
	return;
}

VOID CALLBACK apc_abortio(ULONG_PTR param) {
	connection* con = (connection*)param;
	con->abort_io = 1;
}


DWORD WINAPI uds2np_thread(LPVOID param) {
	connection* con = (connection*)param;
	char buf[BUFSIZE];
	int bytes_read, bytes_written;
	OVERLAPPED ol;

	//read from pipe and write to sock
	while (1) {
		ZeroMemory(&ol, sizeof(OVERLAPPED));
		// read from pipe
		if (!ReadFileEx(con->pipe, buf, BUFSIZE, &ol, apc_callback)) {
			goto done;
		}
		
		SleepEx(INFINITE, TRUE);
		bytes_read = 0;
		if (con->abort_io || !GetOverlappedResult(con->pipe, &ol, &bytes_read, FALSE))
			goto done;

		//now write to sock
		bytes_written = 0;
		while (bytes_written < bytes_read) {
			int written = send(con->sock, buf + bytes_written, bytes_read - bytes_written, 0);
			if (written == SOCKET_ERROR)
				goto done;
			bytes_written += written;
		}
	}

done:
	shutdown(con->sock, SD_SEND);
	con_activity_done(con);
	return 0;
}


DWORD WINAPI np2uds_thread(LPVOID param) {
	connection* con = (connection*)param;
	char buf[BUFSIZE];
	int bytes_read, bytes_written;
	OVERLAPPED ol;

	//read from sock and write to pipe
	while (1) {
		// read from sock
		bytes_read = recv(con->sock, buf, BUFSIZE, 0);
		if (bytes_read <= 0)
			goto done;

		// now write to pipe
		bytes_written = 0;
		ZeroMemory(&ol, sizeof(OVERLAPPED));
		if (!WriteFileEx(con->pipe, buf, bytes_read, &ol, apc_callback))
			goto done;
			
		SleepEx(INFINITE, TRUE);
		if (!GetOverlappedResult(con->pipe, &ol, &bytes_written, FALSE))
			goto done;
	}

done:
	QueueUserAPC(apc_abortio, con->uds2np_th, (ULONG_PTR)con);
	con_activity_done(con);
	return 0;
}