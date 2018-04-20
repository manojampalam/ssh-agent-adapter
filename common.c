#include "common.h"

int debug_mode = 0;

static void con_activity_done(connection* con) {
	if (InterlockedDecrement(&(con->activity_count)) == 0) {
		CloseHandle(con->pipe);
		closesocket(con->sock);
		CloseHandle(con->np2uds_th);
		CloseHandle(con->uds2np_th);
		free(con);
		if (debug_mode != 0) {
			printf("connection closed\n");
		}
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
			if (debug_mode != 0) {
				printf("couldn't start read from pipe\n");
			}
			goto done;
		}
		
		SleepEx(INFINITE, TRUE);
		bytes_read = 0;
		if (con->abort_io || !GetOverlappedResult(con->pipe, &ol, &bytes_read, FALSE)) {
			if (debug_mode != 0) {
				printf("couldn't get bytes from pipe, or read aborted\n");
			}
			goto done;
		}
		if (debug_mode != 0) {
			printf("read %d bytes from pipe\n", bytes_read);
		}

		//now write to sock
		bytes_written = 0;
		while (bytes_written < bytes_read) {
			int written = send(con->sock, buf + bytes_written, bytes_read - bytes_written, 0);
			if (written == SOCKET_ERROR) {
				if (debug_mode != 0) {
					printf("error when writing to socket\n");
				}
				goto done;
			}
			bytes_written += written;
			if (debug_mode != 0) {
				printf("wrote %d (%d) of %d bytes to socket\n", written, bytes_written, bytes_read);
			}
		}
	}

done:
	if (debug_mode != 0) {
		printf("shutting down socket\n");
	}
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
		if (bytes_read <= 0) {
			if (debug_mode != 0) {
				printf("nothing to read from socket\n");
			}
			goto done;
		}
		if (debug_mode != 0) {
			printf("read %d bytes from socket\n", bytes_read);
		}

		// now write to pipe
		bytes_written = 0;
		ZeroMemory(&ol, sizeof(OVERLAPPED));
		if (!WriteFileEx(con->pipe, buf, bytes_read, &ol, apc_callback)) {
			if (debug_mode != 0) {
				printf("can't start write to pipe\n");
			}
			goto done;
		}

		SleepEx(INFINITE, TRUE);
		if (!GetOverlappedResult(con->pipe, &ol, &bytes_written, FALSE)) {
			if (debug_mode != 0) {
				printf("couldn't write to pipe\n");
			}
			goto done;
		}
		if (debug_mode != 0) {
			printf("wrote %d of %d bytes to pipe\n", bytes_written, bytes_read);
		}
	}

done:
	if (debug_mode != 0) {
		printf("shutting down pipe\n");
	}
	QueueUserAPC(apc_abortio, con->uds2np_th, (ULONG_PTR)con);
	con_activity_done(con);
	return 0;
}