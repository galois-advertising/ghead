#include "ghead.h"
#include <stdio.h>
#include <stdarg.h>
#include <poll.h>
#include <boost/log/trivial.hpp>
#include <chrono>


namespace galois::ghead
{
const unsigned int ghead::GHEAD_MAGICNUM = 0xe8c4a59;

void ghead::log(LOG_LEVEL loglevel, const char * fmt, ...)
{
    va_list args;
    char buf[1024];
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    switch(loglevel) {
    case TRACE:BOOST_LOG_TRIVIAL(trace) << buf;break;
    case DEBUG:BOOST_LOG_TRIVIAL(debug) << buf;break;
    case INFO:BOOST_LOG_TRIVIAL(info) << buf;break;
    case WARNING:BOOST_LOG_TRIVIAL(warning) << buf;break;
    case ERROR:BOOST_LOG_TRIVIAL(error) << buf;break;
    case FATAL:BOOST_LOG_TRIVIAL(fatal) << buf;break;
    };
}

RETURN_CODE ghead::read(int sock, ghead * head, size_t buflen, int timeout)
{

    if (sock < 0 || !head || buflen < sizeof(ghead))
        return RET_EPARAM;

    // read head
    int rlen = sync_read_n_tmo(sock, reinterpret_cast<uint8_t*>(head), sizeof(ghead), timeout);
    if (rlen <= 0) {
        goto ghead_read_fail;
    } else if (rlen != sizeof(ghead)) {
        log(WARNING, "<%u> ghead_read head fail: ret[%d] want[%u]",
                head->log_id, rlen, sizeof(ghead));
        return RET_READHEAD;
    }

    // check magic
    if (head->magic_num != GHEAD_MAGICNUM) {
        log(ERROR, "<%u> ghead_read magic num mismatch: ret[%x] want[%x]",
                head->log_id, head->magic_num, GHEAD_MAGICNUM);
        return RET_EMAGICNUM;
    }

    // check reqsize
    if (buflen < sizeof(ghead) + head->body_len) {
        log(WARNING, "<%u> ghead_read body_len error: bodylen[%u] buflen[%u][%u|%u]",
                head->log_id, head->body_len, buflen - sizeof(ghead), buflen, sizeof(ghead));
        return RET_EBODYLEN;
    }
    // read body
    if (head->body_len > 0) {
        rlen = sync_read_n_tmo(sock, head->body, head->body_len, timeout);
        if (rlen <= 0) {
            goto ghead_read_fail;
        }
        else if (rlen != (int)head->body_len) {
            log(WARNING, "<%u> ghead_read body fail: ret[%d] want[%u]",
                    head->log_id, rlen, rlen, head->body_len);
            return RET_READ;
        }
    }
    return RET_SUCCESS;

ghead_read_fail:
    if (rlen == 0) {
        return RET_PEARCLOSE;
    }
    log(WARNING, "<%u> ghead_read fail: ret=%d ERR[%m]",head->log_id, rlen);
    if (rlen == -1 && errno == ETIMEDOUT) {
        return RET_ETIMEDOUT;
    } else {
        return RET_READ;
    }
}

ssize_t ghead::sync_read_n_tmo(int fd, uint8_t * ptr, size_t nbytes, int timeout_ms)
{
    if (ptr == nullptr || nbytes == 0)
      return 0;
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    size_t nleft = nbytes;
    while(nleft > 0) {
        int presult = poll_wrap(&pfd, 1, timeout_ms);
        if (presult > 0) {
            int nread = ::read(fd, ptr, nleft);
            if (nread < 0) {
                if (errno == EINTR) {
                    continue;
                } else {
                    return -1;
                }
            } else if (nread == 0) {
                break;
            }
            ptr += nread;
            nleft -= nread;
        } else if (presult == 0) {
            // timeout
            return nbytes - nleft;
        } else {
            return -1;
        }
    }
    return nbytes;
}

// A simple wrap function of linux poll
// fdarray : pollfd array
// nfds : count of fds
// timeout : timeout(ms) or (-1,0,>0)
// return  -1, 0, nfds
int ghead::poll_wrap(pollfd * fdarray, 
    nfds_t nfds, int timeout_ms)
{
    using namespace std::chrono;
    if (fdarray == nullptr || nfds == 0)
        return -1;
    int ret_val = 0;
    auto start = system_clock::now();
    duration rest_timeout = milliseconds(timeout_ms); 
    while(true) {
        ret_val = poll(fdarray, nfds, 
            static_cast<int>(duration_cast<milliseconds>(rest_timeout).count()));
        if (ret_val > 0) {
            return ret_val;
        } else if (ret_val == 0) {
            errno = ETIMEDOUT;
            return ret_val;
        } else if (ret_val < 0 && errno == EINTR) {
            rest_timeout -= duration_cast<milliseconds>(system_clock::now() - start); 
            if (duration_cast<milliseconds>(rest_timeout).count() > 0) {
                continue;
            } else {
                errno = ETIMEDOUT;
                ret_val = 0;
                return ret_val;
            }
        } else {
            log(WARNING, "poll() error:[%d][%s]", errno, strerror(errno));
            return ret_val;
        }
    }
    errno = ETIMEDOUT;
    return ret_val;
}

}