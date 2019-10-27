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

RETURN_CODE ghead::read(int sock, ghead * head, void *req, size_t req_size,
        void *buf, size_t buf_size, int timeout, unsigned flags)
{

    if (sock < 0 || !head)
        return RET_EPARAM;
    int rlen = sync_read_n_tmo(sock, reinterpret_cast<uint8_t*>(head), sizeof(ghead), timeout);
    if (rlen <= 0) {
        goto ghead_read_fail;
    } else if (rlen != sizeof(ghead)) {
        log(WARNING, "<%u> ghead_read head fail: ret %d want %u ERR[%m]",
                head->log_id, rlen, (unsigned int)req_size);
        return RET_READHEAD;
    }

    if (head->magic_num != GHEAD_MAGICNUM) {
        log(ERROR, "<%u> ghead_read magic num mismatch: ret %x want %x",
                head->log_id, head->magic_num, GHEAD_MAGICNUM);
        return RET_EMAGICNUM;
    }

    if (head->body_len < req_size || head->body_len - req_size > buf_size) {
        log(WARNING, "<%u> ghead_read body_len error: req_size=%u buf_size=%u body_len=%u",
                head->log_id, (unsigned int)req_size, (unsigned int)buf_size, head->body_len);
        return RET_EBODYLEN;
    }

    if (req_size > 0) {
        rlen = sync_read_n_tmo(sock, static_cast<uint8_t*>(req), req_size, timeout);
        if (rlen <= 0) {
            goto ghead_read_fail;
        }
        else if (rlen != (int)req_size) {
            log(WARNING, "<%u> ghead_read fail: ret %d want %u ERR[%m]",
                    head->log_id, rlen, (unsigned int)req_size);
            return RET_READ;
        }
    }

    if (head->body_len > req_size) {
        rlen = sync_read_n_tmo(sock, static_cast<uint8_t*>(buf), head->body_len - req_size, timeout);
        if (rlen <= 0) {
            goto ghead_read_fail;
        }
        else if (rlen != (int)(head->body_len - req_size)) {
            printf("<%u> ghead_read fail: ret %d want %d ERR[%m]",
                    head->log_id, rlen, int(head->body_len - req_size));
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