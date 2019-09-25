#include "galois-head.h"
#include <stdio.h>


namespace galois
{
const unsigned int galois_head::GALOIS_HEAD_MAGICNUM = 0xe8c4a59;

GALOIS_HEAD_RET_ERROR_T galois_head::read(int sock, galois_head * head, void *req, size_t req_size,
        void *buf, size_t buf_size, int timeout, unsigned flags)
{

    if (flags & GALOIS_HEAD_CHECK_PARAM) {
        if (sock < 0 || !head)
            return GALOIS_HEAD_RET_EPARAM;
    }

    int rlen = sync_read_n_tmo(sock, head, sizeof(galois_head), timeout);
    if (rlen <= 0)
        goto nshead_read_fail;
    else if (rlen != sizeof(galois_head)) {
        printf("<%u> nshead_read head fail: ret %d want %u ERR[%m]",
                head->log_id, rlen, (unsigned int)req_size);
        return GALOIS_HEAD_RET_READHEAD;
    }

    if (flags & GALOIS_HEAD_CHECK_MAGICNUM) {
        if (head->magic_num != NSHEAD_MAGICNUM) {
            ul_writelog(UL_LOG_WARNING,
                    "<%u> nshead_read magic num mismatch: ret %x want %x",
                    head->log_id, head->magic_num, NSHEAD_MAGICNUM);
            return GALOIS_HEAD_RET_EMAGICNUM;
        }
    }

    /* 对 body 的长度进行检查, 过长和过短都不符合要求 */
    if (head->body_len < req_size || head->body_len - req_size > buf_size) {
        printf("<%u> nshead_read body_len error: req_size=%u buf_size=%u body_len=%u",
                head->log_id, (unsigned int)req_size, (unsigned int)buf_size, head->body_len);
        return GALOIS_HEAD_RET_EBODYLEN;
    }

    /* Read Request */
    if (req_size > 0) {
        rlen = sync_read_n_tmo(sock, req, req_size, timeout);
        if (rlen <= 0)
            goto nshead_read_fail;
        else if (rlen != (int)req_size) {
            printf("<%u> nshead_read fail: ret %d want %u ERR[%m]",
                    head->log_id, rlen, (unsigned int)req_size);
            return GALOIS_HEAD_RET_READ;
        }
    }

    /* Read Detail */
    if (head->body_len - req_size > 0) {
        rlen = sync_read_n_tmo(sock, buf, head->body_len - req_size, timeout);
        if (rlen <= 0)
            goto nshead_read_fail;
        else if (rlen != (int)(head->body_len - req_size)) {
            ul_writelog(UL_LOG_WARNING,
            printf("<%u> nshead_read fail: ret %d want %d ERR[%m]",
                    head->log_id, rlen, int(head->body_len - req_size));
            return GALOIS_HEAD_RET_READ;
        }
    }

    return GALOIS_HEAD_RET_SUCCESS;

nshead_read_fail:
    if (rlen == 0)
        return GALOIS_HEAD_RET_PEARCLOSE;

    printf("<%u> nshead_read fail: ret=%d ERR[%m]",head->log_id, rlen);
    if (rlen == -1 && errno == ETIMEDOUT)
        return GALOIS_HEAD_RET_ETIMEDOUT;
    else
        return GALOIS_HEAD_RET_READ;
}

ssize_t galois_head::sync_read_n_tmo(int fd, void *ptr1, size_t nbytes, int msecs)
{
    ssize_t nread;
    size_t nleft = nbytes;
    char * ptr = (char *) ptr1;
    struct timeval tv;
    struct timeval *tp;

    if (msecs < 0) {
        tp = NULL;
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

ssize_t galois_head::reado_tv(int fd, void *ptr, size_t nbytes, struct timeval * tv)
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

int galois_head::sreadable_tv(int fd, struct timeval *tv)
{

    pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    if ( tv == NULL ) {
        return poll_wrap(&pfd, 1, -1);
    } else {
        struct timeval before, after;
        gettimeofday(&before, NULL);
        int result = (poll_wrap(&pfd, 1, timeval_2_ms(tv)));
        gettimeofday(&after, NULL);

        ul_sub_timeval(tv, &before, &after);

        return result;
    }

}

int galois_head::poll_wrap(struct pollfd *fdarray, unsigned int nfds, int timeout)
{
    int val, otimeout = timeout;
    struct timeval ftv, btv;
    gettimeofday(&ftv, NULL);
    for (;;) {
        val = poll(fdarray, nfds, timeout);
        if (val < 0) {
            if (errno == EINTR) {
                if ( otimeout >= 0 ) {
                    gettimeofday(&btv, NULL);
                    timeout = otimeout - ((btv.tv_sec - ftv.tv_sec) * 1000 + (btv.tv_usec - ftv.tv_usec) / 1000);
                    if ( timeout <= 0 ) {
                        val = 0;
                        errno = ETIMEDOUT;
                        break;
                    }
                }
                continue;
            }
        }
        else if (val == 0) {
            errno = ETIMEDOUT;
        }
        return (val);
    }
    return (val);
}

void galois_head::ms_2_timeval(const int msec, struct timeval *ptv)
{
    if (ptv == NULL) {
        return;
    }
    ptv->tv_sec = msec / 1000;
    ptv->tv_usec = (msec % 1000) * 1000;
}

int galois_head::timeval_2_ms(const struct timeval *ptv)
{
    if ( ptv == NULL ) {
        return -1;
    }
    return ptv->tv_sec * 1000 + ptv->tv_usec / 1000;
}

}