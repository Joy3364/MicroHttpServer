#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fcntl.h>
#include "server.h"

void HTTPServerInit(HTTPServer *srv, uint16_t port) {
	struct sockaddr_in srv_addr;
	int flags;

	/* Have a server socket. */
	srv->sock = socket(AF_INET, SOCK_STREAM, 0);
	if(srv->sock <= 0) exit(1);
	/* Set server address. */
	memset(&srv_addr, 0, sizeof(srv_addr));
	srv_addr.sin_family = AF_INET;
	srv_addr.sin_port = htons(port);
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	/* Bind the server socket with the server address. */
	bind(srv->sock, (struct sockaddr*) &srv_addr, sizeof(srv_addr));
	/* Set the server socket non-blocking. */
	flags = fcntl(srv->sock, F_GETFL, 0) | O_NONBLOCK;
	fcntl(srv->sock, F_SETFL, flags);
}

void _HTTPServerAccept(HTTPServer *srv) {
	struct sockaddr_in cli_addr;
	int sockaddr_len = sizeof(cli_addr);
	SOCKET clisock;
	int flags;

	/* Have the client socket and append it to the master socket queue. */
	clisock = accept(srv->sock, (struct sockaddr*) &cli_addr, &sockaddr_len);
	if(clisock != -1) {
		FD_SET(clisock, &(srv->_sock_pool));
		/* Set the client socket non-blocking. */
		flags = fcntl(clisock, F_GETFL, 0) | O_NONBLOCK;
		fcntl(clisock, F_SETFL, flags);
		/* Set the max socket file descriptor. */
		if(clisock > srv->_max_sock) srv->_max_sock = clisock;
		printf("Accept 1 client.\n");
	}
}

int _CheckLine(char *buf) {
	int i = 0;

	if(buf[i] == '\n') {
		if(buf[i - 1] == '\r')
			i = 2;
		else
			i = 1;
	}

	return i;
}

int _CheckFieldSep(char *buf) {
	int i = 0;

	if((buf[i - 1] == ':') && (buf[i] == ' ')) {
		i = 2;
	}

	return i;
}

int _ParseHeader(SOCKET clisock, HTTPReqMessage *req) {
	int n;
	int l, end;
	int i = 0;
	char *p;

	printf("\tParse Header\n");
	p = req->_buf;
	/* GET, PUT ... and a white space are 3 charaters. */
	n = recv(clisock, p, 3, 0);
	if(n == 3) {
		/* Parse method. */
		req->Header.Method = p;
		for(i = 3; n>0; i++) {
			n = recv(clisock, p + i, 1, 0);
			if(p[i] == ' ') {
				p[i] = '\0';
				break;
			}
		}

		/* Parse URI. */
		if(n > 0) i += 1;
		req->Header.URI = p + i;
		for(; n>0; i++) {
			n = recv(clisock, p + i, 1, 0);
			if(p[i] == ' ') {
				p[i] = '\0';
				break;
			}
		}

		/* Parse HTTP version. */
		if(n > 0) i += 1;
		req->Header.Version = p + i;
		/* HTTP/1.1 has 8 charaters. */
		n = recv(clisock, p + i, 8, 0);
		for(i+=8; (n>0) && (i<MAX_HEADER_SIZE); i++) {
			n = recv(clisock, p + i, 1, 0);
			if(l = _CheckLine(p + i)) {
				if(l == 2) p[i - 1] = '\0';
				p[i] = '\0';
				break;
			}
		}

		/* Parse other fields. */
		if(n > 0) i += 1;
		req->Header.Fields[req->Header.Amount].key = p + i;
		end = 0;
		for(; (n>0) && (i<MAX_HEADER_SIZE) && (req->Header.Amount<MAX_HEADER_FIELDS); i++) {
			n = recv(clisock, p + i, 1, 0);
			/* Check field key name end. */
			if(l = _CheckFieldSep(p + i)) {
				p[i - 1] = '\0';
				req->Header.Fields[req->Header.Amount].value = p + i + 1;
			}

			/* Check header end. */
			if(l = _CheckLine(p + i)) {
				if(end == 0) {
					if(l == 2) p[i - 1] = '\0';
					p[i] = '\0';

					/* CRLF have 2 characters, so check 2 times new line. */
					end = 2;

					/* Go to parse next header field. */
					req->Header.Amount += 1;
					req->Header.Fields[req->Header.Amount].key = p + i + 1;
				}
				else {
					/* Requset message header finished. */
					break;
				}
			}
			else {
				if(end > 0) end -= 1;
			}
		}
	}

	req->_index = (n > 0) ? i + 1: i;
	return i;
}

int _GetBody(SOCKET clisock, HTTPReqMessage *req) {
	int n = 1;
	int i = 0;
	char *p;

	printf("\tParse body\n");
	p = req->_buf + req->_index;
	for(i=0; (n>0) && (i<MAX_BODY_SIZE); i++) {
		n = recv(clisock, p + i, 1, MSG_PEEK);
	}

	return i;
}

void _HTTPServerRequest(SOCKET clisock, HTTPREQ_CALLBACK callback) {
	uint8_t req_buf[MAX_HEADER_SIZE + MAX_BODY_SIZE];
	uint8_t res_buf[MAX_HEADER_SIZE + MAX_BODY_SIZE];
	int n;
	HTTPReqMessage request;
	HTTPResMessage response;

	request._buf = req_buf;
	request.Header.Amount = 0;
	response._buf = res_buf;
	response.Header.Amount = 0;
	n = _ParseHeader(clisock, &request);
	if(n > 0) {
		n = _GetBody(clisock, &request);
		callback(&request, &response);
		n = write(clisock, res_buf, response._index);
	}
	shutdown(clisock, SHUT_RDWR);
	close(clisock);
}

void HTTPServerListen(HTTPServer *srv, HTTPREQ_CALLBACK callback) {
	SOCKET clisock;
	fd_set readable;
	uint16_t i;

	/* Start server socket listening. */
	printf("Listening\n");
	listen(srv->sock, MAX_HTTP_CLIENT);

	/* Append server socket to the master socket queue. */
	FD_SET(srv->sock, &(srv->_sock_pool));
	/* The server socket's FD is max in the master socket queue for now. */
	srv->_max_sock = srv->sock;

	while(1) {
		/* Copy master socket queue to readable socket queue. */
		readable = srv->_sock_pool;
		/* Wait the flag of any socket in readable socket queue. */
		select(srv->_max_sock+1, &readable, NULL, NULL, 0);
		/* Go through the sockets in readable socket queue.  */
		for(i=0; i<=srv->_max_sock; i++) {
			if(FD_ISSET(i, &readable)) {
				if(i == srv->sock) {
					/* Accept when server socket has been connected. */
					_HTTPServerAccept(srv);
				}
				else {
					/* Deal the request from the client socket. */
					_HTTPServerRequest(i, callback);
					if(i >= srv->_max_sock) srv->_max_sock -= 1;
					FD_CLR(i, &(srv->_sock_pool));
				}
			}
		}
	}
}

#define HTTPServerClose(srv) (close((srv)->sock))

void _HelloPage(HTTPReqMessage *req, HTTPResMessage *res) {
	int n, i = 0, j;
	char *p;
	char header1[] = "HTTP/1.1 200 OK\r\nConnection: close\r\n";
	char header2[] = "Content-Type: text/html; charset=UTF-8\r\n\r\n";
	char body1[] = "<html><body>許功蓋 Hello <br>";
	char body2[] = "</body></html>";

	/* Build header. */
	p = res->_buf;	
	n = strlen(header1);
	memcpy(p, header1, n);
	p += n;
	i += n;
	
	n = strlen(header2);
	memcpy(p, header2, n);
	p += n;
	i += n;
	
	/* Build body. */
	n = strlen(body1);
	memcpy(p, body1, n);
	p += n;
	i += n;

	/* Echo request header into body. */
	n = strlen(req->Header.Method);
	memcpy(p, req->Header.Method, n);
	p += n;
	i += n;

	n = strlen("<br>");
	memcpy(p, "<br>", n);
	p += n;
	i += n;

	n = strlen(req->Header.URI);
	memcpy(p, req->Header.URI, n);
	p += n;
	i += n;

	n = strlen("<br>");
	memcpy(p, "<br>", n);
	p += n;
	i += n;

	n = strlen(req->Header.Version);
	memcpy(p, req->Header.Version, n);
	p += n;
	i += n;

	for(j=0; j<req->Header.Amount; j++) {
		n = strlen("<br>");
		memcpy(p, "<br>", n);
		p += n;
		i += n;

		n = strlen(req->Header.Fields[j].key);
		memcpy(p, req->Header.Fields[j].key, n);
		p += n;
		i += n;

		p[0] = ':'; p[1] = ' ';
		p += 2;
		i += 2;

		n = strlen(req->Header.Fields[j].value);
		memcpy(p, req->Header.Fields[j].value, n);
		p += n;
		i += n;
	}

	n = strlen(body2);
	memcpy(p, body2, n);
	i += n;

	res->_index = i;
}

int main(void) {
	HTTPServer srv;
	HTTPServerInit(&srv, MTS_PORT);
	HTTPServerListen(&srv, _HelloPage);
	HTTPServerClose(&srv);
	return 0;
}