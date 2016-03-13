#ifndef __MICRO_HTTP_SERVER_H__
#define __MICRO_HTTP_SERVER_H__

#define MAX_HEADER_SIZE  2048
#define MAX_BODY_SIZE    4096
#ifndef MTS_PORT
#define MTS_PORT         8001
#endif
#ifndef MAX_HTTP_CLIENT
#define MAX_HTTP_CLIENT  5
#endif
#ifndef HTTP_SERVER
#define HTTP_SERVER      "Micro CHTTP Server"
#endif

typedef int SOCKET;

typedef struct _HTTPServer {
	SOCKET sock;
	SOCKET _max_sock;
	fd_set _sock_pool;
} HTTPServer;

typedef struct _HTTPHeaderField {
	char *key;
	char *value;
} HTTPHeaderField;

#ifndef MAX_HEADER_FIELDS
#define MAX_HEADER_FIELDS  20
#endif

typedef struct _HTTPReqHeader {
	char *Method;
	char *URI;
	char *Version;
	HTTPHeaderField Fields[MAX_HEADER_FIELDS];
	unsigned int Amount;
} HTTPReqHeader;

typedef struct _HTTPReqMessage {
	HTTPReqHeader Header;
	char *Body;
	uint8_t *_buf;
	uint16_t _index;
} HTTPReqMessage;

typedef struct _HTTPResHeader {
	char *Version;
	char *StatusCode;
	char *Description;
	HTTPHeaderField Fields[MAX_HEADER_FIELDS];
	unsigned int Amount;
} HTTPResHeader;

typedef struct _HTTPResMessage {
	HTTPResHeader Header;
	char *Body;
	uint8_t *_buf;
	uint16_t _index;
} HTTPResMessage;

typedef void (*HTTPREQ_CALLBACK)(HTTPReqMessage *, HTTPResMessage *);

#endif