#include "ghead.h"
#include <stdio.h>
#include <stdarg.h>
#include <boost/log/trivial.hpp>
#include <chrono>


namespace galois::ghead
{
const unsigned int ghead::GHEAD_MAGICNUM = 0xe8c4a59;

const std::map<RETURN_CODE, std::string> ghead::return_code_to_string = {
    {SUCCESS, "SUCCESS"},
    {EPARAM, "EPARAM"}, 
    {EBODYLEN, "EBODYLEN"},
    {WRITE, "WRITE"},
    {READ, "READ"},
    {READHEAD, "READHEAD"},
    {WRITEHEAD, "WRITEHEAD"},
    {PEARCLOSE, "PEARCLOSE"},
    {ETIMEDOUT, "ETIMEDOUT"},
    {EMAGICNUM, "EMAGICNUM"},
    {UNKNOWN, "UNKNOWN"},
};

const std::string & return_code_to_string(RETURN_CODE retcode)
{
    static const std::string dft = "CODEERROR";
    auto iter = std::find(return_code_to_string.cbegin(), 
        return_code_to_string.cend(), retcode);
    if (iter == return_code_to_string.end()) {

        return dft;
    } else
    {
        return *iter;
    }
}

void ghead::log(LOG_LEVEL loglevel, char * fmt, ...)
{
    va_list args;
    char buf[1024];
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    switch(loglevel) {
    case FATAL:BOOST_LOG_TRIVIAL(trace) << buf;break;
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
        return EPARAM;

    int rlen = sync_read_n_tmo(sock, head, sizeof(ghead), timeout);

    if (rlen <= 0)
        goto ghead_read_fail;
    else if (rlen != sizeof(ghead)) {
        printf("<%u> ghead_read head fail: ret %d want %u ERR[%m]",
                head->log_id, rlen, (unsigned int)req_size);
        return GHEAD_RET_READHEAD;
    }

    if (head->magic_num != GHEAD_MAGICNUM) {
        log(ERROR, "<%u> ghead_read magic num mismatch: ret %x want %x",
                head->log_id, head->magic_num, GHEAD_MAGICNUM);
        return EMAGICNUM;
    }

    /* 对 body 的长度进行检查, 过长和过短都不符合要求 */
    if (head->body_len < req_size || head->body_len - req_size > buf_size) {
        printf("<%u> ghead_read body_len error: req_size=%u buf_size=%u body_len=%u",
                head->log_id, (unsigned int)req_size, (unsigned int)buf_size, head->body_len);
        return GHEAD_RET_EBODYLEN;
    }

    /* Read Request */
    if (req_size > 0) {
        rlen = sync_read_n_tmo(sock, req, req_size, timeout);
        if (rlen <= 0)
            goto ghead_read_fail;
        else if (rlen != (int)req_size) {
            printf("<%u> ghead_read fail: ret %d want %u ERR[%m]",
                    head->log_id, rlen, (unsigned int)req_size);
            return GHEAD_RET_READ;
        }
    }

    /* Read Detail */
    if (head->body_len - req_size > 0) {
        rlen = sync_read_n_tmo(sock, buf, head->body_len - req_size, timeout);
        if (rlen <= 0)
            goto ghead_read_fail;
        else if (rlen != (int)(head->body_len - req_size)) {
            printf("<%u> ghead_read fail: ret %d want %d ERR[%m]",
                    head->log_id, rlen, int(head->body_len - req_size));
            return GHEAD_RET_READ;
        }
    }

    return GHEAD_RET_SUCCESS;

ghead_read_fail:
    if (rlen == 0)
        return GHEAD_RET_PEARCLOSE;

    printf("<%u> ghead_read fail: ret=%d ERR[%m]",head->log_id, rlen);
    if (rlen == -1 && errno == ETIMEDOUT)
        return GHEAD_RET_ETIMEDOUT;
    else
        return GHEAD_RET_READ;
}

ssize_t ghead::sync_read_n_tmo(int fd, void *ptr1, size_t nbytes, int msecs)
{
    if (ptr1 == nullptr || nbytes == 0)
        return -1;
    ssize_t nread;
    size_t nleft = nbytes;
    char * ptr = (char *) ptr1;
    struct timeval tv;
    struct timeval *tp;

    if (msecs < 0) {
        tp = nullptr;
    } else {
        ms_2_timeval(msecs, &tv);
        tp = &tv;
    }
    while (nleft > 0) {
      again:
        nread = reado_tv(fd, ptr, nleft, tp);
        if (nread < 0) {
            if ((nread == -1) && (errno == EINTR)) {
                goto again;
            } else if (nread == -2) {
                errno = ETIMEDOUT;
                return -1;
            } else if (nread == -3) {
                errno = EIO;
                return -1;
            } else {
                return -1;
            }
        } else if (nread == 0) {
            break;
        } else {
            ptr += nread;
            nleft -= nread;
        }
    }
    return (ssize_t)(nbytes - nleft);
}

ssize_t ghead::reado_tv(int fd, void *ptr, size_t nbytes, struct timeval * tv)
{
    int err;
    err = sreadable_tv(fd, tv);
    if (err < 0) {
        return -3;
    } else if (err == 0) {
        return -2;
    } else {
        return read(fd, ptr, nbytes);
    }
}


ssize_t ghead::readn(int fd, void *vptr, size_t n)
{
	size_t	nleft;
	ssize_t	nread;
	char	*ptr;
	ptr = vptr;
	nleft = n;
	while (nleft > 0) {
		if ((nread = read(fd, ptr, nleft)) < 0) {
			if (errno == EINTR)
				nread = 0;		/* and call read() again */
			else
				return(-1);
		} else if (nread == 0)
			break;				/* EOF */

		nleft -= nread;
		ptr += nread;
	}
	return(n - nleft);		/* return >= 0 */
}

ssize_t ghead::read_n_tmo(int fd, void * ptr, size_t nbytes, int timeout_ms)
{
    if (ptr == nullptr || nbytes == 0)
      return 0;
    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    size_t nleft = nbytes;
    while(nleft > 0) {
        int result = poll_wrap(&pfd, 1, timeout_ms);
        if (result > 0) {
            int nread = readn(fd, ptr, nleft);
            if (nread < 0) {
                if (errno == EINTR) {
                    nread = 0;
                } else {
                    return -1;
                }
            } else if (nread == 0) {
                break;
            }
            ptr += nread;
            nleft -= nread;
        }

    }
}

// A simple wrap function of linux poll
// fdarray : pollfd array
// nfds : count of fds
// timeout : timeout(ms) or (-1,0,>0)
// return  -1, 0, nfds
int ghead::poll_wrap(struct pollfd * fdarray, 
    unsigned int nfds, int timeout_ms)
{
    using namespace std::chrono;
    if (fdarray == nullptr || nfds == 0)
        return -1;
    int ret_val = 0;
    auto start = system_clock::now();
    auto rest_timeout = milliseconds(timeout); 
    while(true) {
        ret_val = poll(fdarray, nfds, timeout);
        if (ret_val > 0) {
            return ret_val;
        } else if (ret_val == 0) {
            errno = ETIMEDOUT;
            return ret_val;
        } else if (ret_val < 0 && errno == EINTR) {
            rest_timeout -= system_clock::now() - start; 
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

void ghead::ms_2_timeval(const int msec, struct timeval *ptv)
{
    if (ptv == nullptr) 
        return;
    ptv->tv_sec = msec / 1000;
    ptv->tv_usec = (msec % 1000) * 1000;
}

int ghead::timeval_2_ms(const struct timeval * ptv)
{
    if ( ptv == nullptr )
        return -1;
    return ptv->tv_sec * 1000 + ptv->tv_usec / 1000;
}

}