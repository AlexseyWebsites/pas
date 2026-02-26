/*
    pas_http1.h - single-header HTTP/1.1 client (stb-style)

    - No malloc: user provides response_buffer; all parsing points into it.
    - Uses OS sockets only (Winsock2 on Windows, BSD sockets on Unix).
    - GET and POST; URL parsing; request/response headers; timeouts.

    Usage:
        In ONE translation unit:
            #define PAS_HTTP1_IMPLEMENTATION
            #include "pas_http1.h"

        In others:
            #include "pas_http1.h"
*/

#ifndef PAS_HTTP1_H
#define PAS_HTTP1_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Result: 0 = success, non-zero = error (same as *status) */
#define PAS_HTTP_OK            0
#define PAS_HTTP_E_INVALID_URL -1
#define PAS_HTTP_E_CONNECTION -2
#define PAS_HTTP_E_TIMEOUT    -3
#define PAS_HTTP_E_NOSPACE    -4

typedef struct pas_http_response {
    int         status_code;   /* e.g. 200, 404 */
    const char *headers;       /* points into your buffer; not null-terminated */
    size_t      headers_len;
    const char *body;          /* points into your buffer */
    size_t      body_len;
} pas_http_response_t;

/*
    pas_http_get:
        Fetches url via GET, reads response into response_buffer (up to buffer_size).
        Fills out_response with status_code, headers, body (all pointing into response_buffer).
        timeout_ms: send/recv timeout in milliseconds (0 = use implementation default).
        status: receives PAS_HTTP_OK or PAS_HTTP_E_*.
        Returns 0 on success, non-zero on error (same as *status).
*/
int pas_http_get(const char *url,
                 char *response_buffer, size_t buffer_size,
                 int timeout_ms,
                 pas_http_response_t *out_response,
                 int *status);

/*
    pas_http_post:
        Sends body (body_len bytes) to url via POST.
        Same semantics as pas_http_get for response_buffer and out_response.
*/
int pas_http_post(const char *url,
                  const void *body, size_t body_len,
                  char *response_buffer, size_t buffer_size,
                  int timeout_ms,
                  pas_http_response_t *out_response,
                  int *status);

#ifdef __cplusplus
}
#endif

/* ========== Implementation ========== */

#ifdef PAS_HTTP1_IMPLEMENTATION

#include <string.h>
#include <stdio.h>

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    #define PAS_SOCKET_INVALID INVALID_SOCKET
    #define PAS_CLOSE_SOCKET(s) closesocket(s)
    #define PAS_ERRNO WSAGetLastError()
    #define PAS_EAGAIN WSAETIMEDOUT
    #define PAS_EWOULDBLOCK WSAEWOULDBLOCK
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <errno.h>
    typedef int SOCKET;
    #define PAS_SOCKET_INVALID (-1)
    #define PAS_CLOSE_SOCKET(s) close(s)
    #define PAS_ERRNO errno
    #define PAS_EAGAIN EAGAIN
    #define PAS_EWOULDBLOCK EWOULDBLOCK
#endif

#define PAS_HTTP_MAX_HOST 256
#define PAS_HTTP_MAX_PATH 1024
#define PAS_HTTP_DEFAULT_TIMEOUT_MS 30000

static int pas_http_parse_url(const char *url, char *host_buf, size_t host_size,
                              int *port, char *path_buf, size_t path_size)
{
    const char *p;
    if (!url || !host_buf || host_size == 0 || !port || !path_buf || path_size == 0)
        return -1;

    *port = 80;
    if (path_size > 0) path_buf[0] = '\0';

    /* "http://" */
    if (url[0] != 'h' || url[1] != 't' || url[2] != 't' || url[3] != 'p')
        return -1;
    if (url[4] == 's') return -1; /* no HTTPS */
    if (url[4] != ':' || url[5] != '/' || url[6] != '/')
        return -1;
    p = url + 7;

    /* host */
    {
        size_t i = 0;
        while (*p && *p != ':' && *p != '/' && i < host_size - 1) {
            host_buf[i++] = *p++;
        }
        host_buf[i] = '\0';
        if (i == 0) return -1;
    }

    /* port */
    if (*p == ':') {
        p++;
        *port = 0;
        while (*p >= '0' && *p <= '9') {
            *port = *port * 10 + (*p - '0');
            if (*port > 65535) return -1;
            p++;
        }
    }

    /* path */
    if (*p == '\0' || *p != '/') {
        if (path_size < 2) return -1;
        path_buf[0] = '/';
        path_buf[1] = '\0';
    } else {
        size_t i = 0;
        while (*p && i < path_size - 1) {
            path_buf[i++] = *p++;
        }
        path_buf[i] = '\0';
    }

    return 0;
}

static int pas_http_parse_response(char *buf, size_t buf_len,
                                  pas_http_response_t *out)
{
    char *p = buf;
    char *end = buf + buf_len;
    if (buf_len < 12 || !out) return -1;

    out->headers = NULL;
    out->headers_len = 0;
    out->body = NULL;
    out->body_len = 0;
    out->status_code = 0;

    /* "HTTP/1.x " */
    if (p + 9 > end) return -1;
    if (p[0] != 'H' || p[1] != 'T' || p[2] != 'T' || p[3] != 'P' ||
        p[4] != '/' || p[5] != '1' || p[6] != '.') return -1;
    p += 8;
    while (p < end && *p == ' ') p++;
    if (p >= end || *p < '0' || *p > '9') return -1;
    out->status_code = 0;
    while (p < end && *p >= '0' && *p <= '9') {
        out->status_code = out->status_code * 10 + (*p - '0');
        p++;
    }

    /* find \r\n\r\n */
    while (p + 4 <= end) {
        if (p[0] == '\r' && p[1] == '\n' && p[2] == '\r' && p[3] == '\n') {
            out->headers = buf + 0;
            out->headers_len = (size_t)(p - buf);
            out->body = p + 4;
            out->body_len = (size_t)(end - (p + 4));
            return 0;
        }
        p++;
    }
    return -1;
}

static int pas_http_do_request(const char *method, const char *host_buf, int port,
                               const char *path_buf,
                               const void *body, size_t body_len,
                               char *response_buffer, size_t buffer_size,
                               int timeout_ms,
                               pas_http_response_t *out_response,
                               int *status)
{
    SOCKET sock = PAS_SOCKET_INVALID;
    struct addrinfo hints, *res = NULL;
    char port_str[8];
    char req_buf[2048];
    int req_len;
    size_t total_read = 0;
    int r;

#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    static int wsa_done;
    if (!wsa_done) {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
            if (status) *status = PAS_HTTP_E_CONNECTION;
            return PAS_HTTP_E_CONNECTION;
        }
        wsa_done = 1;
    }
#endif

    if (status) *status = PAS_HTTP_OK;
    if (out_response) {
        out_response->status_code = 0;
        out_response->headers = NULL;
        out_response->headers_len = 0;
        out_response->body = NULL;
        out_response->body_len = 0;
    }

    if (buffer_size == 0 || !response_buffer) {
        if (status) *status = PAS_HTTP_E_NOSPACE;
        return PAS_HTTP_E_NOSPACE;
    }

    (void)snprintf(port_str, sizeof(port_str), "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host_buf, port_str, &hints, &res) != 0) {
        if (status) *status = PAS_HTTP_E_CONNECTION;
        return PAS_HTTP_E_CONNECTION;
    }

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock == PAS_SOCKET_INVALID) {
        freeaddrinfo(res);
        if (status) *status = PAS_HTTP_E_CONNECTION;
        return PAS_HTTP_E_CONNECTION;
    }

    if (timeout_ms <= 0) timeout_ms = PAS_HTTP_DEFAULT_TIMEOUT_MS;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
    {
        DWORD t = (DWORD)timeout_ms;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&t, sizeof(t));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&t, sizeof(t));
    }
#else
    {
        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    }
#endif

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        PAS_CLOSE_SOCKET(sock);
        freeaddrinfo(res);
        if (status) *status = PAS_HTTP_E_CONNECTION;
        return PAS_HTTP_E_CONNECTION;
    }
    freeaddrinfo(res);
    res = NULL;

    if (body && body_len > 0) {
        req_len = snprintf(req_buf, sizeof(req_buf),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "Content-Length: %zu\r\n"
            "\r\n",
            method, path_buf, host_buf, body_len);
    } else {
        req_len = snprintf(req_buf, sizeof(req_buf),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path_buf, host_buf);
    }

    if (req_len <= 0 || (size_t)req_len >= sizeof(req_buf)) {
        PAS_CLOSE_SOCKET(sock);
        if (status) *status = PAS_HTTP_E_CONNECTION;
        return PAS_HTTP_E_CONNECTION;
    }

    if (send(sock, req_buf, (size_t)req_len, 0) != (size_t)req_len) {
        PAS_CLOSE_SOCKET(sock);
        if (status) *status = PAS_HTTP_E_CONNECTION;
        return PAS_HTTP_E_CONNECTION;
    }

    if (body && body_len > 0) {
        if (send(sock, (const char *)body, body_len, 0) != (int)body_len) {
            PAS_CLOSE_SOCKET(sock);
            if (status) *status = PAS_HTTP_E_CONNECTION;
            return PAS_HTTP_E_CONNECTION;
        }
    }

    total_read = 0;
    while (total_read < buffer_size) {
        size_t to_read = buffer_size - total_read;
#if defined(_WIN32) || defined(_WIN64) || defined(__CYGWIN__)
        r = recv(sock, response_buffer + total_read, (int)to_read, 0);
#else
        r = (int)recv(sock, response_buffer + total_read, to_read, 0);
#endif
        if (r > 0) {
            total_read += (size_t)r;
        } else if (r == 0) {
            break;
        } else {
            if (PAS_ERRNO == PAS_EAGAIN || PAS_ERRNO == PAS_EWOULDBLOCK) {
                if (status) *status = PAS_HTTP_E_TIMEOUT;
                PAS_CLOSE_SOCKET(sock);
                return PAS_HTTP_E_TIMEOUT;
            }
            break;
        }
    }

    PAS_CLOSE_SOCKET(sock);

    if (total_read == 0) {
        if (status) *status = PAS_HTTP_E_CONNECTION;
        return PAS_HTTP_E_CONNECTION;
    }

    if (total_read >= buffer_size && status)
        *status = PAS_HTTP_E_NOSPACE;

    if (pas_http_parse_response(response_buffer, total_read, out_response) != 0) {
        if (status) *status = PAS_HTTP_E_CONNECTION;
        return PAS_HTTP_E_CONNECTION;
    }

    return (*status == PAS_HTTP_E_NOSPACE) ? PAS_HTTP_E_NOSPACE : 0;
}

int pas_http_get(const char *url,
                 char *response_buffer, size_t buffer_size,
                 int timeout_ms,
                 pas_http_response_t *out_response,
                 int *status)
{
    char host_buf[PAS_HTTP_MAX_HOST];
    char path_buf[PAS_HTTP_MAX_PATH];
    int port;

    if (!url || !response_buffer || !out_response || !status) {
        if (status) *status = PAS_HTTP_E_INVALID_URL;
        return PAS_HTTP_E_INVALID_URL;
    }

    if (pas_http_parse_url(url, host_buf, sizeof(host_buf), &port,
                           path_buf, sizeof(path_buf)) != 0) {
        *status = PAS_HTTP_E_INVALID_URL;
        return PAS_HTTP_E_INVALID_URL;
    }

    return pas_http_do_request("GET", host_buf, port, path_buf,
                               NULL, 0,
                               response_buffer, buffer_size,
                               timeout_ms, out_response, status);
}

int pas_http_post(const char *url,
                  const void *body, size_t body_len,
                  char *response_buffer, size_t buffer_size,
                  int timeout_ms,
                  pas_http_response_t *out_response,
                  int *status)
{
    char host_buf[PAS_HTTP_MAX_HOST];
    char path_buf[PAS_HTTP_MAX_PATH];
    int port;

    if (!url || !response_buffer || !out_response || !status) {
        if (status) *status = PAS_HTTP_E_INVALID_URL;
        return PAS_HTTP_E_INVALID_URL;
    }

    if (body_len > 0 && !body) {
        if (status) *status = PAS_HTTP_E_INVALID_URL;
        return PAS_HTTP_E_INVALID_URL;
    }

    if (pas_http_parse_url(url, host_buf, sizeof(host_buf), &port,
                           path_buf, sizeof(path_buf)) != 0) {
        *status = PAS_HTTP_E_INVALID_URL;
        return PAS_HTTP_E_INVALID_URL;
    }

    return pas_http_do_request("POST", host_buf, port, path_buf,
                               body, body_len,
                               response_buffer, buffer_size,
                               timeout_ms, out_response, status);
}

#endif /* PAS_HTTP1_IMPLEMENTATION */

#endif /* PAS_HTTP1_H */
