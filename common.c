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
	int bytes_read, bytes_written;
	OVERLAPPED ol;

	ZeroMemory(&ol, sizeof(OVERLAPPED));
	ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	//read from pipe and write to sock
	while (1) {
		// read from pipe
		if (!ReadFile(con->pipe, buf, BUFSIZE, NULL, &ol)) {
			if (GetLastError() != ERROR_IO_PENDING)
				goto done;
			WaitForSingleObject(ol.hEvent, INFINITE);
		}
		
		if (!GetOverlappedResult(con->pipe, &ol, &bytes_read, FALSE))
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
	CancelIo(con->pipe);
	shutdown(con->sock, SD_SEND);
	con_activity_done(con);
	return 0;
}


DWORD WINAPI np2uds_thread(LPVOID param) {
	connection* con = (connection*)param;
	char buf[BUFSIZE];
	int bytes_read, bytes_written;
	OVERLAPPED ol;

	ZeroMemory(&ol, sizeof(OVERLAPPED));
	ol.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	//read from sock and write to pipe
	while (1) {
		// read from sock
		bytes_read = recv(con->sock, buf, BUFSIZE, 0);
		if (bytes_read <= 0)
			goto done;

		// now write to pipe
		bytes_written = 0;
		while (bytes_written < bytes_read) {
			DWORD written;

			if (!WriteFile(con->pipe, buf + bytes_written, bytes_read - bytes_written, NULL, &ol)) {
				if (GetLastError() != ERROR_IO_PENDING)
					goto done;
				WaitForSingleObject(ol.hEvent, INFINITE);
			}

			if (!GetOverlappedResult(con->pipe, &ol, &written, FALSE))
				goto done;

			bytes_written += written;
		}

	}

done:
	CancelIo(con->pipe);
	con_activity_done(con);
	return 0;
}