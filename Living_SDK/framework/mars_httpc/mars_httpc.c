/*
 * @Description  : 
 * @Author       : zhoubw
 * @Date         : 2022-08-29 09:16:35
 * @LastEditors  : zhoubw
 * @LastEditTime : 2022-11-17 16:29:29
 * @FilePath     : /tg7100c/Living_SDK/framework/mars_httpc/mars_httpc.c
 */
/*! \file httpclient.c
 *  \brief HTTP client API implementation.
 *
 * This is the HTTP client.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "assert.h"

#include "aos/aos.h"

#ifndef ioctlsocket
#define ioctlsocket(a,b,c)	ioctl(a,b,c)
#endif

#include "lwip/netdb.h"
// #include "netdb.h"
#include "mars_httpc.h"

#define CONFIG_LWIP_STACK

#define PRINTF_E(_mod_name_, _fmt_, ...)                                        \
	printf("[%s]%s"_fmt_"\n\r", _mod_name_, " Error: ", ##__VA_ARGS__)

#define PRINTF_W(_mod_name_, _fmt_, ...)                                        \
	printf("[%s]%s"_fmt_"\n\r", _mod_name_, " Warn: ", ##__VA_ARGS__)


#define httpc_e(...)			\
	PRINTF_E( "httpc",##__VA_ARGS__)
#define httpc_w(...)				\
	PRINTF_W("[httpc] ", ##__VA_ARGS__)

// #define CONFIG_HTTPC_DEBUG

#ifdef CONFIG_HTTPC_DEBUG
#define httpc_d(_fmt_,...)				\
	printf("(httpc) "_fmt_"\r\n",##__VA_ARGS__)
#else
#define httpc_d(...)
#endif /* ! CONFIG_HTTPC_DEBUG */

/* #define DUMP_RECV_DATA */

#define PARSE_EXTRA_BYTES 10
#define DEFAULT_RETRY_COUNT 3

#define DEFAULT_HTTP_PORT 80
#define DEFAULT_HTTPS_PORT 443

/*
 * We are not sure of the exact header size. Instead of reading one octet
 * at a time we will have prefetch buffer. This buffer will be used to read
 * the either header and then this header is parsed.
 * This prefetch buffer is also used as a buffer to store the request
 * header, before sending an HTTP request.
 */
#define MAX_REQ_RESP_HDR_SIZE (1024)	/* 1024 */
/*
 * Reuse prefetch buffer to extract the proper hostname from the user given
 * scheme://hostname:port combination.
 */
#define MAX_HOSTNAME_LEN MAX_REQ_RESP_HDR_SIZE
/*
 * Chunk header is the data structure before each chunk. Chunked data is a
 * type of transfer encoding.
 */
#define MAX_CHUNK_HEADER_SIZE 32
#define PROTOCOL_STR "HTTP"
#define PROTOCOL_VER_1_0 "1.0"
#define PROTOCOL_VER_1_1 "1.1"

#define DEFAULT_USER_AGENT "WMSDK"

#define NEXT_STR(str) (char *)(str + strlen(str) + 1)

typedef enum {
	SESSION_ALLOC,
	SESSION_INIT_DONE,
	SESSION_PREPARE,
	SESSION_REQ_DONE,
	SESSION_RESP_DONE,
	SESSION_DATA_IN_PROGRESS,
	SESSION_COMPLETE,
	SESSION_ERROR,		/* FIXME: Should we flush the socket stream
				   during close, in this state? */
	SESSION_RAW,
} session_state_t;

typedef enum {
	STATE_STANDARD,
	STATE_START_CHUNK,
	STATE_BETWEEN_CHUNK,
	STATE_END_CHUNK,
	STATE_ERROR_CHUNK,
} chunk_state_t;

/*
 * Prefetch buffer info.
 */
typedef struct {
	char *buf;
	int size_read;		/* The total data in the buffer including
				   the header */
	int offset;		/* The offset (excluding) upto which data
				   is processed */
	/* This caches the offset of name-value pairs in the http header */
	int hdr_fields_start_offset;
} prefetch_t;

typedef struct {
	chunk_state_t state;
	int size;
} chunk_t;

#define HOSTNM_MAX	256
typedef struct {
	session_state_t state;
	int sockfd;		/* Socket descriptor. Not exposed to user */
	char hostname[HOSTNM_MAX + 1];	/* Hostname of connecting server */
	prefetch_t pbuf;	/* Prefetch buffer */
	http_resp_t resp;	/* The response structure. */
	chunk_t chunk;		/* For chunked transfers  */
	int content_returned;	/* content bytes returned till now */
#ifdef CONFIG_ENABLE_TLS
	tls_handle_t tls_handle;	/* tls handle for tls connections */
#endif				/* CONFIG_ENABLE_TLS */
} session_t;


uint32_t SEC_PER_YR[2] = { 31536000, 31622400 };
uint32_t SEC_PER_MT[2][12] =  {
{ 2678400, 2419200, 2678400, 2592000, 2678400, 2592000, 2678400, 2678400, 2592000, 2678400, 2592000, 2678400 },
{ 2678400, 2505600, 2678400, 2592000, 2678400, 2592000, 2678400, 2678400, 2592000, 2678400, 2592000, 2678400 },
};
uint32_t SEC_PER_DY = 86400;
uint32_t SEC_PER_HR = 3600;

static int month_from_string_short(const char *month)
{
	if (strncmp(month, "Jan", 3) == 0)
		return 0;
	if (strncmp(month, "Feb", 3) == 0)
		return 1;
	if (strncmp(month, "Mar", 3) == 0)
		return 2;
	if (strncmp(month, "Apr", 3) == 0)
		return 3;
	if (strncmp(month, "May", 3) == 0)
		return 4;
	if (strncmp(month, "Jun", 3) == 0)
		return 5;
	if (strncmp(month, "Jul", 3) == 0)
		return 6;
	if (strncmp(month, "Aug", 3) == 0)
		return 7;
	if (strncmp(month, "Sep", 3) == 0)
		return 8;
	if (strncmp(month, "Oct", 3) == 0)
		return 9;
	if (strncmp(month, "Nov", 3) == 0)
		return 10;
	if (strncmp(month, "Dec", 3) == 0)
		return 11;
	/* not a valid date */
	return 12;
}

time_t http_date_to_time(const char *date)
{
	struct tm tm_time;
	time_t ret = 0;
	char buf[16]={0};
	const char *start_date = NULL;
	int i = 0;

	/* make sure we can use it */
	if (!date)
		return 0;
	memset(&tm_time, 0, sizeof(struct tm));
	memset(buf, 0, 16);
	/* try to figure out which format it's in */
	/* rfc 1123 */
	if (date[3] == ',') {
		if (strlen(date) != 29)
			return 0;
		/* make sure that everything is legal */
		if (date[4] != ' ')
			return 0;
		/* 06 */
		if ((isdigit(date[5]) == 0) || (isdigit(date[6]) == 0))
			return 0;
		/* Nov */
		tm_time.tm_mon = month_from_string_short(&date[8]);
		if (tm_time.tm_mon >= 12)
			return 0;
		/* 1994 */
		if ((isdigit(date[12]) == 0) ||
		    (isdigit(date[13]) == 0) ||
		    (isdigit(date[14]) == 0) || (isdigit(date[15]) == 0))
			return 0;
		if (date[16] != ' ')
			return 0;
		/* 08:49:37 */
		if ((isdigit(date[17]) == 0) ||
		    (isdigit(date[18]) == 0) ||
		    (date[19] != ':') ||
		    (isdigit(date[20]) == 0) ||
		    (isdigit(date[21]) == 0) ||
		    (date[22] != ':') ||
		    (isdigit(date[23]) == 0) || (isdigit(date[24]) == 0))
			return 0;
		if (date[25] != ' ')
			return 0;
		/* GMT */
		if (strncmp(&date[26], "GMT", 3) != 0)
			return 0;
		/* ok, it's valid.  Do it */
		/* parse out the day of the month */
		tm_time.tm_mday += (date[5] - 0x30) * 10;
		tm_time.tm_mday += (date[6] - 0x30);
		/* already got the month from above */
		/* parse out the year */
		tm_time.tm_year += (date[12] - 0x30) * 1000;
		tm_time.tm_year += (date[13] - 0x30) * 100;
		tm_time.tm_year += (date[14] - 0x30) * 10;
		tm_time.tm_year += (date[15] - 0x30);
		tm_time.tm_year -= 1900;
		/* parse out the time */
		tm_time.tm_hour += (date[17] - 0x30) * 10;
		tm_time.tm_hour += (date[18] - 0x30);
		tm_time.tm_min += (date[20] - 0x30) * 10;
		tm_time.tm_min += (date[21] - 0x30);
		tm_time.tm_sec += (date[23] - 0x30) * 10;
		tm_time.tm_sec += (date[24] - 0x30);
		/* ok, generate the result */
		ret = mktime(&tm_time);
	}
	/* ansi C */
	else if (date[3] == ' ') {
		if (strlen(date) != 24)
			return 0;
		/* Nov */
		tm_time.tm_mon = month_from_string_short(&date[4]);
		if (tm_time.tm_mon >= 12)
			return 0;
		if (date[7] != ' ')
			return 0;
		/* "10" or " 6" */
		if (((date[8] != ' ') && (isdigit(date[8]) == 0)) ||
		    (isdigit(date[9]) == 0))
			return 0;
		if (date[10] != ' ')
			return 0;
		/* 08:49:37 */
		if ((isdigit(date[11]) == 0) ||
		    (isdigit(date[12]) == 0) ||
		    (date[13] != ':') ||
		    (isdigit(date[14]) == 0) ||
		    (isdigit(date[15]) == 0) ||
		    (date[16] != ':') ||
		    (isdigit(date[17]) == 0) || (isdigit(date[18]) == 0))
			return 0;
		if (date[19] != ' ')
			return 0;
		/* 1994 */
		if ((isdigit(date[20]) == 0) ||
		    (isdigit(date[21]) == 0) ||
		    (isdigit(date[22]) == 0) || (isdigit(date[23]) == 0))
			return 0;
		/* looks good */
		/* got the month from above */
		/* parse out the day of the month */
		if (date[8] != ' ')
			tm_time.tm_mday += (date[8] - 0x30) * 10;
		tm_time.tm_mday += (date[9] - 0x30);
		/* parse out the time */
		tm_time.tm_hour += (date[11] - 0x30) * 10;
		tm_time.tm_hour += (date[12] - 0x30);
		tm_time.tm_min += (date[14] - 0x30) * 10;
		tm_time.tm_min += (date[15] - 0x30);
		tm_time.tm_sec += (date[17] - 0x30) * 10;
		tm_time.tm_sec += (date[18] - 0x30);
		/* parse out the year */
		tm_time.tm_year += (date[20] - 0x30) * 1000;
		tm_time.tm_year += (date[21] - 0x30) * 100;
		tm_time.tm_year += (date[22] - 0x30) * 10;
		tm_time.tm_year += (date[23] - 0x30);
		tm_time.tm_year -= 1900;
		/* generate the result */
		ret = mktime(&tm_time);
	}
	/* must be the 1036... */
	else {
		/* check to make sure we won't rn out of any bounds */
		if (strlen(date) < 11)
			return 0;
		while (date[i]) {
			if (date[i] == ' ') {
				start_date = &date[i + 1];
				break;
			}
			i++;
		}
		/* check to make sure there was a space found */
		if (start_date == NULL)
			return 0;
		/* check to make sure that we don't overrun anything */
		if (strlen(start_date) != 22)
			return 0;
		/* make sure that the rest of the date was valid */
		/* 06- */
		if ((isdigit(start_date[0]) == 0) ||
		    (isdigit(start_date[1]) == 0) || (start_date[2] != '-'))
			return 0;
		/* Nov */
		tm_time.tm_mon = month_from_string_short(&start_date[3]);
		if (tm_time.tm_mon >= 12)
			return 0;
		/* -94 */
		if ((start_date[6] != '-') ||
		    (isdigit(start_date[7]) == 0) ||
		    (isdigit(start_date[8]) == 0))
			return 0;
		if (start_date[9] != ' ')
			return 0;
		/* 08:49:37 */
		if ((isdigit(start_date[10]) == 0) ||
		    (isdigit(start_date[11]) == 0) ||
		    (start_date[12] != ':') ||
		    (isdigit(start_date[13]) == 0) ||
		    (isdigit(start_date[14]) == 0) ||
		    (start_date[15] != ':') ||
		    (isdigit(start_date[16]) == 0) ||
		    (isdigit(start_date[17]) == 0))
			return 0;
		if (start_date[18] != ' ')
			return 0;
		if (strncmp(&start_date[19], "GMT", 3) != 0)
			return 0;
		/* looks ok to parse */
		/* parse out the day of the month */
		tm_time.tm_mday += (start_date[0] - 0x30) * 10;
		tm_time.tm_mday += (start_date[1] - 0x30);
		/* have the month from above */
		/* parse out the year */
		tm_time.tm_year += (start_date[7] - 0x30) * 10;
		tm_time.tm_year += (start_date[8] - 0x30);
		/* check for y2k */
		if (tm_time.tm_year < 20)
			tm_time.tm_year += 100;
		/* parse out the time */
		tm_time.tm_hour += (start_date[10] - 0x30) * 10;
		tm_time.tm_hour += (start_date[11] - 0x30);
		tm_time.tm_min += (start_date[13] - 0x30) * 10;
		tm_time.tm_min += (start_date[14] - 0x30);
		tm_time.tm_sec += (start_date[16] - 0x30) * 10;
		tm_time.tm_sec += (start_date[17] - 0x30);
		/* generate the result */
		ret = mktime(&tm_time);
	}
	return ret;
}


/**
 * Returns 1 if current year id a leap year
 */
static inline int is_leap(int yr)
{
	if (!(yr%100))
		return (yr%400 == 0) ? 1 : 0;
	else
		return (yr%4 == 0) ? 1 : 0;
}
#if 0
static unsigned char day_of_week_get(unsigned char month, unsigned char day, unsigned short year)
{
	/* Month should be a number 0 to 11, Day should be a number 1 to 31 */

	static int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
	year -= month < 3;
	return (year + year/4 - year/100 + year/400 + t[month-1] + day) % 7;
}


static int validate_date_time(const struct tm *tm)
{
	if (tm->tm_sec > 59)
		return -1;
	if (tm->tm_min > 59)
		return -1;
	if (tm->tm_hour > 23)
		return -1;
	if (((tm->tm_year + 1900) < 1970) || ((tm->tm_year + 1900) > 2111))
		return -1;
	if (tm->tm_mon >= 12)
		return -1;
	if (tm->tm_mon == 1) {
		if (!is_leap(tm->tm_year + 1900)) {
			if (tm->tm_mday > 28)
				return -1;
		} else  {
			if (tm->tm_mday > 29)
				return -1;
		}
	}
	switch (tm->tm_mon) {
		case 3:
		case 5:
		case 8:
		case 10:
			if(tm->tm_mday > 30)
				return -1;
	}

	if ( (tm->tm_mday < 1) || (tm->tm_mday > 31) )
		return -1;

   	return 0;
}
#endif
static int net_gethostbyname(const char *cp, struct hostent **hentry)
{
	struct hostent *he;
	if ((he = gethostbyname(cp)) == NULL) {
		printf("%s failed for %s\n\r", __FUNCTION__, cp);
		return -1;
	}
	*hentry = he;
	return 0;
}

static int _http_raw_recv(session_t *s, char *buf, int maxlen)
{
#ifdef CONFIG_ENABLE_TLS
	if (s->tls_handle)
		return tls_recv(s->tls_handle, buf, maxlen);
	else
		return recv(s->sockfd, buf, maxlen, 0);
#else
	return recv(s->sockfd, buf, maxlen, 0);
#endif /* CONFIG_ENABLE_TLS */
}

static int _http_raw_send(session_t *s, const char *buf, int len)
{
#ifdef CONFIG_ENABLE_TLS
	if (s->tls_handle)
		return tls_send(s->tls_handle, buf, len);
	else
		return send(s->sockfd, buf, len, 0);
#else
	return send(s->sockfd, buf, len, 0);
#endif /* CONFIG_ENABLE_TLS */
}

int httpc_write_chunked(http_session_t handle, const char *data, int len)
{
	session_t *s = (session_t *) handle;
	if (!handle || (len && !data)) {
		httpc_e("Invalid params");
		return -2;
	}
	if (!(s->state == SESSION_REQ_DONE &&
	      s->chunk.state == STATE_START_CHUNK))  {
		httpc_e("Cannot write data. session state: %d,session chunk state: %d", s->state, s->chunk.state);
		return -1;
	}

	int post_len = sprintf(s->pbuf.buf, "%x\r\n", len);
	int sent = _http_raw_send(s, s->pbuf.buf, post_len);
	if (sent != post_len) {
		s->chunk.state = STATE_ERROR_CHUNK;
		return sent;
	}
	if (len) {
		sent = _http_raw_send(s, data, len);
		if (sent != len) {
			s->chunk.state = STATE_ERROR_CHUNK;
			return sent;
		}

		sent = _http_raw_send(s, "\r\n", 2);
		if (sent != 2) {
			s->chunk.state = STATE_ERROR_CHUNK;
			return sent;
		}
	} else {
		sent = _http_raw_send(s, "\r\n", 2);
		if (sent != 2) {
			s->chunk.state = STATE_ERROR_CHUNK;
			return sent;
		}

		s->chunk.state = STATE_END_CHUNK;
	}
	return 0;
}

/**
 * @internal
 *
 * Read the stream until header delimiter is found in the read
 * blocks. Store these read octets into the prefetch buffer.
 */
static int prefetch_header(session_t *s)
{
	prefetch_t *pbuf = &s->pbuf;
	/* One less so that delimit strings */
	int max_read_size = MAX_REQ_RESP_HDR_SIZE - 1;
	int total_read_size = 0;
	int size_read;
	httpc_d("Start header read");
	memset(pbuf->buf, 0x00, MAX_REQ_RESP_HDR_SIZE);
	while (max_read_size) {
		int read_start_offset = total_read_size;
		size_read = _http_raw_recv(s, &pbuf->buf[read_start_offset],
					   max_read_size);
		httpc_d("Header read: %d bytes", size_read);
		if (size_read <= 0) {
			httpc_e("Unable to read response header: %d",
				size_read);
			return -3;
		}

		total_read_size += size_read;
		pbuf->buf[total_read_size] = 0;
		/*
		 * Check if just read data has the header end
		 *  delimiter. The delimiter may have started in tail bytes
		 *  of last read buffer.
		 */
		int check_offset =
		    read_start_offset > 4 ? read_start_offset - 4 : 0;
		if (strstr(&pbuf->buf[check_offset], "\r\n\r\n")) {
			/*
			 * Even if we have found the delimiter in the read
			 * buffer, the buffer may have some content octets
			 * after the header. This will be read later by the
			 * prefetch-buffer-aware read function.
			 */
			pbuf->size_read = total_read_size;
			pbuf->offset = 0;
			return 0;
		}

		max_read_size -= size_read;
	}

	httpc_e("Header not found.");
	httpc_e("The stream may be in inconsistent state or");
	httpc_e("in a rare case, the header may be bigger that %d bytes",
		MAX_CHUNK_HEADER_SIZE - 1);
	return -1;
}

/**
 * @internal
 * Prefetch buffer aware network read.
 *
 * All network read requests except header reads go through this
 * function. For the reads, the data not yet read from the prefetch buffer
 * will be returned. If prefetch buffer is empty then the TCP stream will
 * be read directly.
 */
static int recv_buf(session_t *s, char *buf, int max_read_size)
{
	/* The header is already read */
	int r;
	prefetch_t *pbuf = &s->pbuf;

	if (pbuf->offset == pbuf->size_read) {
		/* All prefetched data is over. Now read directly from the
		   stream */
		httpc_d("Read content");
		r = _http_raw_recv(s, buf, max_read_size);
		httpc_d("content read: %d", r);
		if (r <= 0) {
			httpc_e("Error while reading stream %d\n",r);
			return -3;
		}
		return r;
	}

	/* There is some prefetched data left. */
	int prefetch_remaining = (pbuf->size_read - pbuf->offset);
	int to_copy = prefetch_remaining <= max_read_size ?
	    prefetch_remaining : max_read_size;
	memcpy(buf, &pbuf->buf[pbuf->offset], to_copy);
	pbuf->offset += to_copy;
	httpc_d("Using prefetch: %d %d %d %d",
		pbuf->offset, prefetch_remaining, to_copy, max_read_size);
	return to_copy;
}

/**
 * Create a socket and connect to the peer. The 'hostname' parameter can
 * either be a hostname or a ip address.
 */
static int tcp_socket(int *sockfd, struct sockaddr_in *addr,
		      char *hostname, uint16_t port, int retry_cnt)
{
	char *host = hostname;
	struct timeval tv;
	int is_ipaddr=1;
	int i;

	for(i=0;i<(int)strlen(hostname);i++)
	{
		if(isalpha(hostname[i])&&hostname[i]!='.')
		{
			is_ipaddr=0;
			break;
		}
	}

	if (!is_ipaddr) {
		/* hostname is in human readable form. get the
		   corresponding IP address */
		struct hostent *entry = NULL;
		net_gethostbyname(hostname, &entry);

		if (entry) {
			struct sockaddr_in tmp;
			memset(&tmp, 0, sizeof(struct sockaddr_in));
			memcpy(&tmp.sin_addr.s_addr, entry->h_addr_list[0], entry->h_length);
			host = inet_ntoa(tmp.sin_addr);
		} else {
			int cur_errno=errno;
			httpc_e("No entry for host %s found %d", hostname,cur_errno);
			return -1;
		}
	}

	httpc_d("(%s)is_ipadder?%s", hostname, is_ipaddr?"true":"false");
	if (!retry_cnt)
		retry_cnt = DEFAULT_RETRY_COUNT;
	while (retry_cnt--) {
		*sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (*sockfd >= 0)
			break;
		/* Wait some time to allow some sockets to get released */
		// os_thread_sleep(os_msec_to_ticks(1000));
		aos_msleep(1000);
	}

	if (*sockfd < 0) {
		httpc_e("Unable to create socket, %d", *sockfd);
		return *sockfd;
	}

	httpc_d("(%s)is_ipadder?%s", hostname, is_ipaddr?"true":"false");
#ifndef CONFIG_LWIP_STACK
	int opt;
	/* Limit Send/Receive buffer size of socket to 2KB */
	opt = 2048;
	if (setsockopt(*sockfd, SOL_SOCKET, SO_SNDBUF,
		       (char *)&opt, sizeof(opt)) == -1) {
		httpc_w("Unsupported option SO_SNDBUF");
	}

	opt = 2048+2048+12*1024;
	if (setsockopt(*sockfd, SOL_SOCKET, SO_RCVBUF,
		       (char *)&opt, sizeof(opt)) == -1) {
		httpc_w("Unsupported option SO_RCVBUF");
	}
#endif /* CONFIG_LWIP_STACK */

	tv.tv_sec=1;
	tv.tv_usec=0;
	if(setsockopt(*sockfd, SOL_SOCKET, SO_SNDTIMEO,(char*) &tv, sizeof(struct timeval))==-1)
	{
		httpc_w("Unsupported option SO_SNDTIMEO");
	}
	memset(addr, 0, sizeof(struct sockaddr_in));

	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = inet_addr(host);
	memset(&(addr->sin_zero), '\0', 8);

#ifdef HTTPC_TCP_NODELAY
	{
		int on = 1;
		socklen_t len = sizeof(on);
		int res =
		    setsockopt(*sockfd, IPPROTO_TCP, TCP_NODELAY, &on, len);
		if (res < 0) {
			httpc_e("setsockopt TCP_NODELAY failed");
			// net_close(*sockfd);
			shutdown(*sockfd, 2);
    		close(*sockfd);
			*sockfd = 0;
			return -1;
		}
	}
#endif /* HTTPC_TCP_NODELAY */
	return 0;
}

/**
 * Create a socket and connect to the given host
 *
 * @param[in,out] sock Pointer to a socket variable to be filled by callee
 * @param[in] hostname Hostname or an IP address
 * @param[in] port Port number
 *
 * @return 0 if the socket was created successfully and -1
 * if failed
 */

static int tcp_check_connect_and_wait_connection(
	/*! [in] socket. */
	int sock,
	/*! [in] result of connect. */
	int connect_res)
{
	struct timeval tmvTimeout = {1, 0};
	int result;
#ifdef WIN32
	struct fd_set fdSet;
#else
	fd_set fdSet;
#endif
	FD_ZERO(&fdSet);
	FD_SET(sock, &fdSet);

	if (connect_res < 0) {
		if (EINPROGRESS == errno )
		{
			result = select(sock + 1, NULL, &fdSet, NULL, &tmvTimeout);
			if (result < 0)
			{
				printf("%s %d\n",__FUNCTION__,__LINE__);
				return -1;
			}
			else if (result == 0)
			{
				/* timeout */
				printf("%s %d\n",__FUNCTION__,__LINE__);
				return -1;
#ifndef WIN32
			}
			else
			{
				int valopt = 0;
				socklen_t len = sizeof(valopt);
				if (getsockopt(sock, SOL_SOCKET, SO_ERROR, (void *) &valopt, &len) < 0)
				{
					/* failed to read delayed error */
					printf("%s %d\n",__FUNCTION__,__LINE__);
					return -1;
				}
				else if (valopt)
				{
					/* delayed error = valopt */
					printf("%s %d %d\n", __FUNCTION__, __LINE__, valopt);
					return -1;
				}
#endif
			}
		}
	}

	return 0;
}

static int tcp_make_blocking(int sock)
{
#if defined(WIN32)||defined(__ECOS)
	u_long val = 0;
	return ioctlsocket(sock, FIONBIO, &val);
#else
	int val;

	val = fcntl(sock, F_GETFL, 0);
	if (fcntl(sock, F_SETFL, val & ~O_NONBLOCK) == -1) {
		return -1;
	}
#endif
	return 0;
}


static int tcp_make_no_blocking(int sock)
{
#if defined(WIN32)||defined(__ECOS)
	u_long val = 1;
	return ioctlsocket(sock, FIONBIO, &val);
#else /* WIN32 */
	int val;

	val = fcntl(sock, F_GETFL, 0);
	if (fcntl(sock, F_SETFL, val | O_NONBLOCK) == -1) {
		return -1;
	}
#endif /* WIN32 */
	return 0;
}

static inline int tcp_connect(int *sockfd, char *hostname,
			      uint16_t port, int retry_cnt)
{
	struct sockaddr_in addr;
	int r = tcp_socket(sockfd, &addr, hostname, port, retry_cnt);
	if (r != 0) {
		httpc_e("Socket creation for %s:%d failed", hostname, port);
		return r;
	}

	httpc_d("Connecting .. %s:%d", hostname, port);
	tcp_make_no_blocking(*sockfd);
	r = connect(*sockfd, (const struct sockaddr *)&addr,
		    sizeof(addr));
	r = tcp_check_connect_and_wait_connection(*sockfd,r);
	if(r==-1)
	{
		httpc_e("tcp connect failed %s:%d", hostname, port);
		// net_close(*sockfd);
		shutdown(*sockfd, 2 );
		close(*sockfd);
		*sockfd = 0;
		return -WM_E_HTTPC_TCP_CONNECT_FAIL;
	}
	if(tcp_make_blocking(*sockfd)==-1)
	{
		httpc_e("tcp_make_blocking failed");
		// net_close(*sockfd);
		shutdown(*sockfd, 2 );
		close(*sockfd);
		*sockfd = 0;
		return -WM_E_HTTPC_TCP_CONNECT_FAIL;
	}
	return 0;
}

static inline session_t *new_session_object()
{
	session_t *s = (session_t*)aos_malloc(sizeof(session_t));
	if (!s) {
		httpc_e("Could not allocate session object");
		return NULL;
	}

	memset(s, 0x00, sizeof(session_t));
	s->pbuf.buf = (char*)aos_malloc(MAX_REQ_RESP_HDR_SIZE);
	if (!s->pbuf.buf) {
		httpc_e("Could not allocate prefetch buffer");
		aos_free(s);
		return NULL;
	}

	s->state = SESSION_ALLOC;
	return s;
}

static inline void delete_session_object(session_t *s)
{
	if (s->pbuf.buf)
		aos_free(s->pbuf.buf);
	aos_free(s);
}

static int _http_parse_URL(char *URL, parsed_url_t *parsed_url)
{
	char *pos = URL;
	const char *r;
	char *token;
	bool found_portno = false;

	httpc_d("Parsing: %s", URL);

	parsed_url->portno = DEFAULT_HTTP_PORT;

	/* Check for the scheme */
	token = strstr(pos, "://");
	if (token) {
		*token = 0;
		httpc_d("Scheme: %s", pos);
		parsed_url->scheme = pos;
		pos = token + 3;
	} else {
		httpc_d("No scheme in given URL");
		parsed_url->scheme = NULL;
	}

	parsed_url->hostname = pos;

	/* Check for the port number */
	token = strchr(pos, ':');
	if (token) {
		*token = 0;
		httpc_d("Parse: Hostname: %s", pos);
		/* Skip the ':' */
		token++;
		/*
		 * Set r to start of port string so that we can search for
		 * start of resource later.
		 */
		r = token;
		if (sscanf(token, "%u", &parsed_url->portno) != 1) {
			httpc_e("Port number parsing failed: %s", token);
			return -1;
		}

		found_portno = true;
		httpc_d("Parse: Port No: %d", parsed_url->portno);
	} else {
		httpc_d("No port number given");
		/*
		 * No port number given. So have to search from start of
		 * hostname for resource start
		 */
		r = parsed_url->hostname;
	}

	/* Check for the resource */
	token = (char*)strchr(r, '/');
	if (token) {
		/*
		 * Resource is found. We have to move the resource string
		 * (including the NULL termination) to he right by one byte
		 * to NULL terminate the hostname string. However, this can
		 * be avoided if the hostname was postfixed by the port no.
		 */
		if (!found_portno) {
			memmove(token + 1, token, strlen(token) + 1);
			/* NULL terminate the hostname */
			*token = 0;
			token++;
		} else
			*(token - 1) = 0;

		/* point token to the resource */
		httpc_d("Resource: %s", token);
		parsed_url->resource = token;
	} else {
		httpc_d("No resource specified in given URL");
		parsed_url->resource = NULL;
	}

	return 0;
}

int http_parse_URL(const char *URL, char *tmp_buf, int tmp_buf_len,
		   parsed_url_t *parsed_url)
{
	if (!URL || !tmp_buf || !parsed_url) {
		httpc_e("Cannot parse URL");
		httpc_e("NULL pointer(s) received as argument.");
		return -2;
	}

	int min_size_req = strlen(URL) + PARSE_EXTRA_BYTES;
	if (min_size_req > tmp_buf_len) {
		httpc_e("Cannot parse URL");
		httpc_e("Temporary buffer size is less than required");
		return -2;
	}

	strcpy(tmp_buf, URL);
	return _http_parse_URL(tmp_buf, parsed_url);
}

int http_open_session(http_session_t *handle,
		      const char *hostname, int flags,
		      const tls_init_config_t *cfg, int retry_cnt)
{
	int r;
	session_t *s;

	if (!handle || !hostname) {
		httpc_e("Cannot create session.");
		httpc_e("NULL pointer(s) received as argument.");
		return -2;
	}

	s = new_session_object();
	if (!s)
		return -1;

	if (strlen(hostname) >= MAX_HOSTNAME_LEN) {
		httpc_e("Host name corrupt");
		delete_session_object(s);
		return -1;
	}

	parsed_url_t url;
	r = http_parse_URL(hostname, s->pbuf.buf, MAX_REQ_RESP_HDR_SIZE, &url);
	if (r != 0) {
		delete_session_object(s);
		return r;
	}

	if (url.scheme && strcmp(url.scheme, "https") == 0) {
		httpc_d("Force enable TLS flag");
		flags |= TLS_ENABLE;
		if (url.portno == DEFAULT_HTTP_PORT)
			url.portno = DEFAULT_HTTPS_PORT;
	}
	
#ifndef CONFIG_ENABLE_TLS
	if (flags & TLS_ENABLE) {
		httpc_e("TLS is not enabled in configuration.");
		httpc_e("Cannot set up a TLS connection.");
		delete_session_object(s);
		return -WM_E_HTTPC_TLS_NOT_ENABLED;
	}
#endif /* CONFIG_ENABLE_TLS */

	httpc_d("Connect: %s Port: %d", url.hostname, url.portno);

	r = tcp_connect(&s->sockfd, (char *)url.hostname, url.portno,
			retry_cnt);
	if (r != 0) {
		delete_session_object(s);
		*handle = 0;
		return r;
	}

#ifdef CONFIG_ENABLE_TLS
	if (flags & TLS_ENABLE) {
		tls_lib_init();
		httpc_d("TLS init session");
		r = tls_session_init(&s->tls_handle, s->sockfd, cfg);
		if (r != 0) {
			httpc_e("TLS session init failed");
			// net_close(s->sockfd);
			shutdown(s->sockfd, 2 );
    		close(s->sockfd);
			delete_session_object(s);
			return r;
		}
	}
#endif /* CONFIG_ENABLE_TLS */

	/* Save parsed hostname in session structure. This is required to be
	   filled into request header later */
	strncpy(s->hostname, url.hostname, HOSTNM_MAX);
	*handle = (http_session_t) s;
	s->state = SESSION_INIT_DONE;

	return r;
}

static const char *get_version_string(http_ver_t version)
{
	switch (version) {
	case HTTP_VER_1_0:
		return (const char *)PROTOCOL_VER_1_0;
	case HTTP_VER_1_1:
		return (const char *)PROTOCOL_VER_1_1;
	default:
		httpc_e("Unknown version given");
		return NULL;
	}
}

/**
 * @return On success returns string corresponding to the type. On error,
 * returns NULL .
 */
static const char *get_method_string(http_method_t type)
{
	switch (type) {
	case HTTP_GET:		/* retrieve information */
		return (const char *)"GET";
	case HTTP_POST:	/* request to accept new sub-ordinate of resource */
		return (const char *)"POST";
	case HTTP_HEAD:	/* get meta-info */
		return (const char *)"HEAD";
	case HTTP_OPTIONS:	/* request to server for communication
				   options */
	case HTTP_PUT:		/* modify or create new resource referred
				   to by URI */
	case HTTP_DELETE:	/* delete the resource */
	case HTTP_TRACE:	/* echo */
	case HTTP_CONNECT:	/* do we need this  ? */
		httpc_e("Method not yet supported.");
		return NULL;
	default:
		httpc_e("Unknown method given.");
	}

	return NULL;
}

static inline int _http_add_header(session_t *s, const char *name,
				   const char *value)
{
	if (value &&
	    (strlen(s->pbuf.buf) + strlen(name) + strlen(value) + 4) <
	    MAX_REQ_RESP_HDR_SIZE) {
		strcat(s->pbuf.buf, name);
		strcat(s->pbuf.buf, ": ");
		strcat(s->pbuf.buf, value);
		strcat(s->pbuf.buf, "\r\n");
		return 0;
	}
	return -1;
}

int http_add_header(http_session_t handle, const char *name, const char *value)
{
	httpc_d("Enter: %s", __func__);
	session_t *s = (session_t *) handle;

	if (!handle || !name || !value || s->state != SESSION_PREPARE) {
		httpc_e("Cannot add header");
		return -2;
	}

	return _http_add_header(s, name, value);
}

static const char *sanitize_resource_name(session_t *s, const char *resource)
{
	parsed_url_t url;
	int r;
	const char *default_resource = "/";

	if (!resource)
		return default_resource;

	/* Check if the resource string starts with a '/' */
	if (resource[0] != default_resource[0]) {
		httpc_d("Have to extract\n\r");
		/* The resource string is either a valid URL or just garbage */
		r = http_parse_URL(resource, s->pbuf.buf, MAX_REQ_RESP_HDR_SIZE,
				   &url);
		if (r != 0) {
			httpc_e("Resource extraction error: %d", r);
			return NULL;
		}
		if (!url.resource)
			return default_resource;
		else
			return url.resource;
	} else
		return resource;
}

static inline int _http_send_request(session_t *s, const httpc_req_t *req)
{
	/* Complete the header */
	strcat(s->pbuf.buf, "\r\n");

	/* Send the header on the network */
	int to_send = strlen(s->pbuf.buf);
	httpc_d("send data=%s",s->pbuf.buf);
	int sent = _http_raw_send(s, s->pbuf.buf, to_send);
	if (sent != to_send) {
		httpc_e("Failed to send header");
		return -3;
	}

	/* Send the data if this was POST request */
	if (req->type == HTTP_POST && req->content_len) {
		sent = _http_raw_send(s, req->content, req->content_len);
		if (sent != req->content_len) {
			httpc_e("Failed to send data.");
			return -3;
		}
	}
	return 0;
}

static int _http_prepare_req(session_t *s, const httpc_req_t *req,
			     http_hdr_field_sel_t field_flags)
{
	const char *method, *version, *resource;
	method = get_method_string(req->type);
	if (!method) {
		httpc_e("HTTPC: Method invalid");
		s->state = SESSION_ERROR;
		return -2;
	}

	version = get_version_string(req->version);
	if (!version) {
		httpc_e("HTTPC: version invalid");
		s->state = SESSION_ERROR;
		return -2;
	}

	resource = sanitize_resource_name(s, req->resource);
	if (!resource) {
		httpc_e("HTTPC: resource invalid");
		s->state = SESSION_ERROR;
		return -2;
	}

	/* Start generating the header */
	sprintf(s->pbuf.buf, "%s %s %s/%s\r\n", method, resource,
		PROTOCOL_STR, version);

	/* Fill in saved hostname */
	_http_add_header(s, "Host", s->hostname);

	if (field_flags & HDR_ADD_DEFAULT_USER_AGENT)
		_http_add_header(s, "User-Agent", DEFAULT_USER_AGENT);

	if (field_flags & HDR_ADD_CONN_KEEP_ALIVE)
		_http_add_header(s, "Connection", "keep-alive");

	if (field_flags & HDR_ADD_CONN_CLOSE) {
		assert(!(field_flags & HDR_ADD_CONN_KEEP_ALIVE));
		_http_add_header(s, "Connection", "close");
	}
	if (field_flags & HDR_ADD_TYPE_CHUNKED) {
		_http_add_header(s, "Transfer-Encoding", "chunked");
		s->chunk.state = STATE_START_CHUNK;
	}

	if (req->type == HTTP_POST) {
		int cur_pos = strlen(s->pbuf.buf);
		sprintf(&s->pbuf.buf[cur_pos], "Content-Length: %d\r\n",
			req->content_len);
	}

	s->state = SESSION_PREPARE;
	return 0;
}

int http_prepare_req(http_session_t handle, const httpc_req_t *req,
		     http_hdr_field_sel_t field_flags)
{
	httpc_d("Enter: %s", __func__);
	session_t *s = (session_t *) handle;

	if (!handle || !req || s->state != SESSION_INIT_DONE)  {
		httpc_e("Cannot prepare request. State: %d", s->state);
		return -2;
	}
	memset(&s->chunk, 0x00, sizeof(chunk_t));
	return _http_prepare_req(s, req, field_flags);
}

int http_send_request(http_session_t handle, const httpc_req_t *req)
{
	session_t *s = (session_t *) handle;

	httpc_d("Enter: http_send_request");

	if (!req || !handle || s->state != SESSION_PREPARE) {
		httpc_e("Cannot send request");
		return -2;
	}

	s->pbuf.size_read = 0;
	s->pbuf.offset = 0;
	s->pbuf.hdr_fields_start_offset = -1;
	memset(&s->resp, 0x00, sizeof(http_resp_t));
	s->content_returned = 0;

	if (req->type == HTTP_POST && !req->content && req->content_len) {
		httpc_e("Cannot send request. Buffer empty.");
		return -2;
	}

	int r = _http_send_request(s, req);
	if (r == 0) {
		s->state = SESSION_REQ_DONE;
		return r;
	}

	s->state = SESSION_ERROR;
	return r;
}

static void parse_keep_alive_header(const char *value, http_resp_t *resp)
{
	int ret;
	if (!strstr(value, "timeout")) {
		httpc_w("No timeout specified in response header.");
		httpc_w("NAK'ing keep alive by force");
		resp->keep_alive_ack = false;
		return;
	}

	/* The header has a timeout value spec */
	ret = sscanf(value, "timeout=%d", &resp->keep_alive_timeout);
	if (ret <= 0) {
		httpc_w("timeout value not found !");
		resp->keep_alive_ack = false;
		return;
	}

	httpc_d("Server timeout value: %d", resp->keep_alive_timeout);
	value = strstr(value, "max");
	if (!value) {
		httpc_w("max value not found !");
		resp->keep_alive_ack = false;
		return;
	}

	ret = sscanf(value, "max=%d", &resp->keep_alive_max);
	if (ret <= 0) {
		httpc_w("max value invalid !");
		resp->keep_alive_ack = false;
		return;
	}

	httpc_d("Alive Max value:%d", resp->keep_alive_max);
	resp->keep_alive_ack = true;
}

/**
 * The parameter off is updated with the new offset pointing at the data,
 * if any.
 * 'len' is the amount of valid data present in the buffer
 *
 * @return On success, i.e. if the header end was found 0 will be
 * returned. On error -1 will be returned.
 */
static int load_header_fields(char *header, int len, int *off,
			      http_resp_t *resp)
{
	int offset = *off;
	bool header_done = false;
	char *substr;
	char *pair, *name, *value;

	while (offset < len) {
		if (header[offset] == '\r' && header[offset + 1] == '\n') {
			/* Point to the start of data in case of standard
			   read or start of first chunk header in case of
			   chunked read */
			offset += 2;
			header_done = true;
			break;
		}
		/* FIXME: can this happen ? */
		assert(header[offset] != '\r' && header[offset + 1] != '\n');

		/* Extract the name-value pair */
		pair = &header[offset];
		substr = strstr(pair, "\r\n");
		if (!substr) {
			httpc_e("Header parsing error in pair %s", pair);
			return -1;
		}
		*substr = 0;

		int pair_len = strlen(pair);
		if (((pair + pair_len) - header) > len) {
			httpc_e
			    ("Hdr parsing over bound:len: %d pair_len: %d",
			     len, pair_len);
			httpc_e("#### %s", pair);
			return -1;
		}

		/* Go past the header: value and the header boundary '\r\n' */
		offset += pair_len + 2;

		/* Separate name-value */
		name = pair;
		substr = strstr(name, ": ");
		if (!substr) {
			httpc_e("Header parsing error in name %s", pair);
			//return -1;
			continue;
		}
		*substr = 0;

		/* Skip the name string, the delimited :
		   and the space after that */
		value = name + strlen(name) + 2;

		if (!strcasecmp(name, "Server"))
			resp->server = value;
		if (!strcasecmp(name, "Last-Modified"))
			resp->modify_time = http_date_to_time(value);
		if (!strcasecmp(name, "Content-Type"))
			resp->content_type = value;
		if (!strcasecmp(name, "Connection")) {
			if (!strcasecmp(value, "Keep-Alive")) {
				httpc_d("Server allows keep alive");
				resp->keep_alive_ack = true;
			} else {
				if (resp->keep_alive_ack == true) {
					httpc_d("Keep-Alive present but "
						"connection: Close ?");
				}
				httpc_d("Server does not allow keep alive");
				resp->keep_alive_ack = false;
			}
		}
		if (!strcasecmp(name, "Keep-Alive"))
			parse_keep_alive_header(value, resp);
		if (!strcasecmp(name, "Content-Length"))
			resp->content_length = strtol(value, NULL, 10);
		if (!strcasecmp(name, "Content-Encoding"))
			resp->content_encoding = value;
		if (!strcasecmp(name, "Transfer-Encoding"))
			if (!strcasecmp(value, "chunked"))
				resp->chunked = true;
	}

	if (header_done) {
		*off = offset;
		return 0;
	} else
		return -1;
}

/**
 * @param[out] data_offset Points to start of data after last byte of header (if any)
 */
static inline int _parse_http_header(char *header, int len,
				     http_resp_t *resp,
				     int *hdr_fields_start_offset,
				     int *data_offset)
{
	int offset = 0;
	char *http_ver = NULL, *status_str = NULL, *substr;
	char *str = header;
	char *delim;

	httpc_d("Parsing HTTP header");
	/* Get the status line */
	delim = strchr(str, '/');
	if (delim) {
		*delim = 0;
		resp->protocol = str;
	}
	if (!resp->protocol || strcasecmp(resp->protocol, PROTOCOL_STR)) {
		httpc_e("Protocol mismatch in header.");
		return -1;
	}

	str = NEXT_STR(resp->protocol);
	delim = strchr(str, ' ');
	if (delim) {
		*delim = 0;
		http_ver = str;
	}
	if (!http_ver) {
		httpc_e("Protocol version not found.");
		return -1;
	} else {
		if (!strcasecmp(http_ver, PROTOCOL_VER_1_0))
			resp->version = HTTP_VER_1_0;
		else if (!strcasecmp(http_ver, PROTOCOL_VER_1_1)) {
			resp->version = HTTP_VER_1_1;
			/*
			 * Since it is HTTP 1.1 we will default to
			 * persistent connection. If there is explicit
			 * "Connection: Close" in the header, this
			 * variable to be reset later to false
			 * automatically.
			 */
			httpc_d("Defaulting to keep alive");
			resp->keep_alive_ack = true;
		} else {
			httpc_e("Protocol version mismatch.");
			return -1;
		}
	}

	str = NEXT_STR(http_ver);
	delim = strchr(str, ' ');
	if (delim) {
		*delim = 0;
		status_str = str;
	}
	if (!status_str) {
		httpc_e("Status code not found.");
		return -1;
	}

	resp->status_code = strtol(status_str, NULL, 10);
	str = NEXT_STR(status_str);
	substr = strstr(str, "\r\n");
	if (substr) {
		*substr = 0;
		resp->reason_phrase = str;
	} else {
		httpc_e("Status code string not found.");
		return -1;
	}

	offset = (NEXT_STR(resp->reason_phrase) + 1 - header);
	*hdr_fields_start_offset = offset;
	load_header_fields(header, len, &offset, resp);
	/*      if (len > offset) { */
	/* buffer has more data to be  processed later */
	*data_offset = offset;
	/*} */

	return 0;
}

int http_get_response_hdr(http_session_t handle, http_resp_t **resp)
{
	session_t *s = (session_t *) handle;
	if (!handle || s->state != SESSION_REQ_DONE) {
		httpc_e("Unable to get resp header.");
		return -2;
	}

	/*
	 * We read even past the header data. The data after the header
	 * will be returned to the caller in next call to http_read_content.
	 */
	int r = prefetch_header(s);
	if (r != 0)
		return r;

	r = _parse_http_header(s->pbuf.buf, s->pbuf.size_read, &s->resp,
			       &s->pbuf.hdr_fields_start_offset,
			       &s->pbuf.offset);
	if (r == 0) {
		httpc_d("Header has used up %d bytes from the %d bytes header",
			s->pbuf.offset, s->pbuf.size_read);
		s->state = SESSION_RESP_DONE;
		if (resp)
			*resp = &s->resp;
		/* Set the chunk state to start if the transfer encoding is
		 * given as chunked  */
		if (s->resp.chunked) {
			s->chunk.state = STATE_START_CHUNK;
		}
	} else {
		s->state = SESSION_ERROR;
	}

	return r;
}

/* A function very dependant on the design of httpc. It checks for
   '\0\n'. Note that the pair length returned, does not include the NULL
   terminating character. Also note that this function totally assumes that
   the name and value both are NULL terminated already. Basically should
   work because the header as been already parsed once.*/
static inline int get_header_pair(char *buf, int maxlen, int *pair_len,
				  char **name, char **value)
{
	if (buf[0] == '\0')
		return -1;

	bool name_terminator_found = false;
	char *name_term_address;

	/* We are right now at the start of a name-value pair */
	int i;
	for (i = 1; i < maxlen; i++) {
		if (buf[i] == '\0') {
			if (!name_terminator_found) {
				name_terminator_found = true;
				name_term_address = &buf[i];
				continue;
			}

			if ((i + 1) < maxlen && buf[i + 1] == '\n') {
				if ((&buf[i] - name_term_address) < 1) {
					/* Sanity check */
					return -1;
				}
				*name = buf;
				*value = name_term_address + 2;
				*pair_len = i;
				return 0;
			}
		}
	}

	return -1;
}

static int _http_get_response_hdr_value(session_t *s,
					const char *header_name,
					char **value,
					http_header_pair_t *arr, int *count)
{
	/* We should already know the offset from where the name-value
	   pairs in the http header start */
	assert(s->pbuf.hdr_fields_start_offset != -1);

	/* Create a local copy as this function may be called more than
	   once */
	int offset = s->pbuf.hdr_fields_start_offset;
	/* This is the total size of the header in the buffer */
	const int len = s->pbuf.size_read;
	char *header = s->pbuf.buf;
	int arr_index = 0;
	while (offset < len) {
		if (header[offset] == '\r' && header[offset + 1] == '\n') {
			/* Point to the start of data in case of standard
			   read or start of first chunk header in case of
			   chunked read */
			offset += 2;
			break;
		}
		/* FIXME: can this happen ? */
		assert(header[offset] != '\r' && header[offset + 1] != '\n');

		int pair_len;
		char *name, *val;
		int rv = get_header_pair(&header[offset], len - offset,
					 &pair_len, &name, &val);
		if (rv != 0) {
			httpc_e("Error in parsing header: %d", rv);
			return rv;
		}
		if (header_name == NULL) {
			/* User has asked us for the entire list */
			arr[arr_index].name = name;
			arr[arr_index].value = val;
			arr_index++;
			if (arr_index == *count)
				break;
			/* Go past the header: value and the header boundary
			   '\0\n' */
			offset += pair_len + 2;
			continue;
		}

		if (strncasecmp(name, header_name, len - offset)) { //Jim
			/* This is not the header caller needs */

			/* Go past the header: value and the header boundary
			   '\0\n' */
			offset += pair_len + 2;
			continue;
		}

		*value = val;
		return 0;
	}

	/* Comes here only if complete list is required */
	/* Sanity check for single name value requested */
	if (header_name != NULL)
		return -1;

	*count = arr_index;
	return 0;
}

int http_get_response_hdr_value(http_session_t handle,
				const char *header_name, char **value)
{
	session_t *s = (session_t *) handle;
	if (!handle) {
		httpc_e("Unable to get resp header.");
		return -2;
	}

	switch (s->state) {
	case SESSION_RESP_DONE:
	case SESSION_REQ_DONE:
		break;
	default:
		httpc_e("Invalid state: %d. Cannot read hdr value", s->state);
		return -2;
	}

	if (s->state == SESSION_REQ_DONE) {
		/*
		 * The caller has not called http_get_response_hdr. We need
		 * to do it.
		 */
		int status = http_get_response_hdr(handle, NULL);
		if (status != 0)
			return status;
	}

	/* Failure here is not a session error */
	return _http_get_response_hdr_value(s, header_name, value,
					    NULL, NULL);
}


int http_get_response_hdr_all(http_session_t handle, http_header_pair_t *arr,
			      int *count)
{
	session_t *s = (session_t *) handle;
	if (!handle || !arr || !count) {
		httpc_e("Unable to get resp header.");
		return -2;
	}

	switch (s->state) {
	case SESSION_RESP_DONE:
	case SESSION_REQ_DONE:
		break;
	default:
		httpc_e("Invalid state: %d. Cannot read hdr value", s->state);
		return -2;
	}

	if (s->state == SESSION_REQ_DONE) {
		/*
		 * The caller has not called http_get_response_hdr. We need
		 * to do it.
		 */
		int status = http_get_response_hdr(handle, NULL);
		if (status != 0)
			return status;
	}

	/* Failure here is not a session error */
	if (*count == 0)
		return 0;
	return _http_get_response_hdr_value(s, NULL, NULL, arr, count);
}

/**
 * @return On success, returns the amount of data read. On error, return -1
 */
static int http_read_standard(session_t *s, void *buf, uint32_t max_len)
{
	if (s->resp.content_length == (uint32_t)s->content_returned) {
		httpc_d("Standard read complete");
		if (s->resp.keep_alive_ack)
			s->state = SESSION_INIT_DONE;
		else
			s->state = SESSION_COMPLETE;
		return 0;
	}

	int content_len = s->resp.content_length;
	int size_remaining = content_len - s->content_returned;
	int size_to_read = size_remaining <= (int)max_len ? size_remaining : max_len;
	int size_read = recv_buf(s, (char*)buf, size_to_read);
	if (size_read < 0) {
		httpc_e("Error in reading content: %d", size_read);
		s->state = SESSION_ERROR;
		return -1;
	}
	s->content_returned += size_read;
	s->state = SESSION_DATA_IN_PROGRESS;
	return size_read;
}

static int  http_read_no_special_len(session_t *s, void *buf, uint32_t max_len)
{
	int size_to_read = max_len;
	int size_read =0;
	fd_set frset;
	struct timeval timeout;
	int ret;

	FD_ZERO(&frset);
	FD_SET(s->sockfd,&frset);
	timeout.tv_sec= 3;
	timeout.tv_usec= 0;
	ret= select(s->sockfd+1,&frset,NULL,NULL,&timeout);
	if (ret<= 0||!FD_ISSET(s->sockfd, &frset))
	{
		httpc_d("No special len read complete,timeout!");
		if (s->resp.keep_alive_ack)
			s->state = SESSION_INIT_DONE;
		else
			s->state = SESSION_COMPLETE;
		return 0;
	}
	
	size_read=recv_buf(s, (char*)buf, size_to_read);
	if (size_read < 0)
	{
		httpc_e("Error in reading content: %d", size_read);
		s->state = SESSION_ERROR;
		return -1;
	}
	s->content_returned += size_read;
	s->state = SESSION_DATA_IN_PROGRESS;
	
	return size_read;
}
/**
 * @pre The buffer should point to the start of the chunk header.
 *
 * @return On success, i.e. if the data size was found correctly. If data
 * size was not found then -1 is returned.
 */
static int get_content_len(session_t *s)
{
	char chunk_header[MAX_CHUNK_HEADER_SIZE];
	bool first_delim_found = false;
	int i;
	char c;
	for (i = 0; i < MAX_CHUNK_HEADER_SIZE; i++) {
		int size_read = recv_buf(s, &c, 1);
		if (size_read <= 0) {
			httpc_e("Error while reading chunk len");
			return -1;
		}

		chunk_header[i] = c;
		if (first_delim_found) {
			if (chunk_header[i] != '\n') {
				httpc_e("Unable to read len. No newline");
				return -1;
			}
			break;
		}
		if (chunk_header[i] == '\r')
			first_delim_found = true;
	}

	if (i == MAX_CHUNK_HEADER_SIZE) {
		httpc_e("Unable to read length. Header corrupted.");
		return -1;
	}

	/* replace \r with string delim */
	chunk_header[i - 1] = 0;
	httpc_d("Chunk size str: %s", chunk_header);
	return strtol(chunk_header, NULL, 16);
}

static int zap_chunk_boundary(session_t *s)
{
	char chunk_boundary[2];
	int size_read;
	size_read = recv_buf(s, &chunk_boundary[0], 1);
	if (size_read != 1) {
		s->chunk.state = STATE_ERROR_CHUNK;
		return -1;
	}
	size_read = recv_buf(s, &chunk_boundary[1], 1);
	if (size_read != 1) {
		s->chunk.state = STATE_ERROR_CHUNK;
		return -1;
	}

	if (strncmp(chunk_boundary, "\r\n", 2)) {
		s->chunk.state = STATE_ERROR_CHUNK;
		return -1;
	}
	return 0;
}

/**
 * @return On success, returns the amount of data read. On error, return -1
 */
static int http_read_chunked(session_t *s, void *buf, uint32_t max_len)
{
	if (s->chunk.state == STATE_BETWEEN_CHUNK) {
		/* partial length from a chunk was returned earlier */
		int size_remaining = s->chunk.size - s->content_returned;
		int size_to_read =
		    size_remaining <= (int)max_len ? size_remaining : max_len;
		httpc_d
		    ("Readng %d bytes of chunked data (b/w)... remn'g %d",
		     size_to_read, size_remaining);
		int size_read = recv_buf(s, (char*)buf, size_to_read);
		if (size_read < 0) {
			httpc_e("Failed to read partial chunk");
			s->chunk.state = STATE_ERROR_CHUNK;
			return -1;
		}

		s->content_returned += size_read;
		httpc_d("Read %d bytes of partial chunked data.", size_read);
		httpc_d("Cumulative: %d  Total: %d",
			s->content_returned, s->chunk.size);
		if (s->content_returned == s->chunk.size) {
			/* This chunk is done with */
			if (zap_chunk_boundary(s) == 0) {
				s->content_returned = 0;
				s->chunk.size = 0;
				s->chunk.state = STATE_START_CHUNK;
			} else {
				httpc_e("Zap chunk boundary failed");
				return -1;
			}
		}
		return size_read;
	} else if (s->chunk.state == STATE_START_CHUNK) {
		/* We are start of chunk header */
		s->chunk.size = get_content_len(s);
		if (s->chunk.size < 0) {
			s->chunk.state = STATE_ERROR_CHUNK;
			s->state = SESSION_ERROR;
			/* Return error code */
			return s->chunk.size;
		}
		httpc_d("Chunk size: %d", s->chunk.size);
		if (s->chunk.size == 0) {
			/* HTTP transaction complete */
			if (zap_chunk_boundary(s) == 0) {
				if (s->resp.keep_alive_ack)
					s->state = SESSION_INIT_DONE;
				else
					s->state = SESSION_COMPLETE;
				return 0;
			} else {
				httpc_e("At end of transaction, ");
				httpc_e("could not zap chunk boundary");
				return -1;
			}
		}
		int to_read =
		    s->chunk.size <= (int)max_len ? s->chunk.size : max_len;
		int size_read = recv_buf(s,(char*)buf, to_read);
		if (size_read < 0) {
			httpc_e("Failed to read chunk data");
			s->chunk.state = STATE_ERROR_CHUNK;
			return -1;
		}
		s->content_returned = size_read;
		if (s->content_returned == s->chunk.size) {
			/* This chunk was done with in one go */
			if (zap_chunk_boundary(s) == 0) {
				s->content_returned = 0;
				s->chunk.size = 0;
				s->chunk.state = STATE_START_CHUNK;
			} else {
				httpc_e("Could not zap chunk boundary");
				return -1;
			}
		} else
			s->chunk.state = STATE_BETWEEN_CHUNK;

		httpc_d("Read %d bytes of start chunked data.", size_read);
		httpc_d("Cumulative: %d  Total: %d",
			s->content_returned, s->chunk.size);
		return size_read;
	} else {
		httpc_e("Unknown state %d", s->chunk.state);
	}

	return -1;
}

int http_check_data_ready(http_session_t handle,uint32_t timeous)
{
	fd_set rset;
	struct timeval timeout;
	session_t *s = (session_t *) handle;
	prefetch_t *pbuf;

	if (!handle )
	{
		return 0;
	}
	pbuf = &s->pbuf;
	/* There is some prefetched data left. */
	if((pbuf->size_read - pbuf->offset)>0)
		return 1;
	FD_ZERO(&rset);
	FD_SET(s->sockfd,&rset);
	timeout.tv_sec= timeous/1000;	
	timeout.tv_usec= (timeous%1000)*1000;	
	if(select(s->sockfd+1,&rset,NULL,NULL,&timeout)>0)
		return 1;
	else
		return 0;
}

int http_read_content_timeout(http_session_t handle, void *buf, uint32_t max_len,uint32_t timeouts)
{
	if(!http_check_data_ready(handle,timeouts))
		return -1;

	return http_read_content(handle,buf,max_len);
}

int http_read_content(http_session_t handle, void *buf, uint32_t max_len)
{
	if (!handle || !buf || !max_len) {
		httpc_e("Cannot read content.");
		return -2;
	}
	int size_read;
	int status;
	session_t *s = (session_t *) handle;
	http_resp_t *resp;

	switch (s->state) {
	case SESSION_REQ_DONE:
		/*
		 * The caller has not called http_get_response_hdr. We need
		 * to do it.
		 */
		status = http_get_response_hdr(handle, &resp);
		if (status != 0)
			return status;
		if (resp->status_code != 200 && resp->status_code != 206) {
			httpc_e("Not reading any content.");
			httpc_e("HTTP status code: %d", resp->status_code);
			return -1;
		}
		break;
	case SESSION_RESP_DONE:
	case SESSION_DATA_IN_PROGRESS:
		if (STATE_ERROR_CHUNK == s->chunk.state)
			return -1;
		break;
	case SESSION_ALLOC:
	case SESSION_PREPARE:
	case SESSION_INIT_DONE:
	case SESSION_COMPLETE:
	case SESSION_ERROR:
	case SESSION_RAW:
	default:
		httpc_e("Cannot read content. Unknown state %d", s->state);
		return -1;
	}

	if (s->resp.chunked)
		size_read = http_read_chunked(s, buf, max_len);
	else if(s->resp.content_length==0)
	{
		 size_read = http_read_no_special_len(s, buf, max_len);
	}
	else
		size_read = http_read_standard(s, buf, max_len);

#ifdef DUMP_RECV_DATA
	char *buff = buf;
	int i;
	wmprintf("\n\rHTTPC: ********** HTTPC data *************\n\r");
	if (size_read > 0) {
		for (i = 0; i < size_read; i++) {
			if (buff[i] == '\r')
				continue;
			if (buff[i] == '\n') {
				wmprintf("\n\r");
				continue;
			}
			wmprintf("%c", buff[i]);
		}
		wmprintf("\n\r");
	} else {
		wmprintf("Size: %d Not reading\n\r", size_read);
	}
	wmprintf("HTTPC: ***********************************\n\r");
#endif /* DUMP_RECV_DATA */

	return size_read;
}

void http_close_session(http_session_t *handle)
{
	session_t *s = (session_t *) *handle;

#ifdef CONFIG_ENABLE_TLS
	if (s->tls_handle)
		tls_close(&s->tls_handle);
#endif /* CONFIG_ENABLE_TLS */

	// net_close(s->sockfd);
	shutdown(s->sockfd, 2 );
    close(s->sockfd);
	delete_session_object(s);
	s = NULL;
	// *handle = NULL;
}


int http_lowlevel_read(http_session_t handle, void *buf, unsigned maxlen)
{
	if (!handle || !buf || !maxlen) {
		httpc_e("Cannot read lowlevel");
		return -2;
	}

	session_t *s = (session_t *)handle;
	if (s->state <= SESSION_INIT_DONE || s->state == SESSION_ERROR) {
		httpc_e("Unable to do lowlevel read");
		return -2;
	}

	s->state = SESSION_RAW;
	return recv_buf(s, (char*)buf, maxlen);
}

int http_lowlevel_write(http_session_t handle, const void *buf, unsigned len)
{
	if (!handle || !buf || !len) {
		httpc_e("Cannot write lowlevel");
		return -2;
	}

	session_t *s = (session_t *)handle;
	if (s->state <= SESSION_INIT_DONE || s->state == SESSION_ERROR) {
		httpc_e("Unable to do lowlevel write");
		return -2;
	}

	s->state = SESSION_RAW;
	/* After this point user is not allowed any call apart from
	   http_close_session */
	return _http_raw_send(s, (const char*)buf, len);
}


