#pragma one
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/in.h>

namespace galois
{
enum GALOIS_HEAD_FLAGS_T {
    GALOIS_HEAD_CHECK_NONE = 0,
    GALOIS_HEAD_CHECK_MAGICNUM = 0x01,
    GALOIS_HEAD_CHECK_PARAM = 0x02,
};

enum GALOIS_HEAD_RET_ERROR_T {
    GALOIS_HEAD_RET_SUCCESS =   0,
    GALOIS_HEAD_RET_EPARAM =  -1,
    GALOIS_HEAD_RET_EBODYLEN =  -2,
    GALOIS_HEAD_RET_WRITE =  -3,
    GALOIS_HEAD_RET_READ =  -4,
    GALOIS_HEAD_RET_READHEAD =  -5,
    GALOIS_HEAD_RET_WRITEHEAD =  -6,
    GALOIS_HEAD_RET_PEARCLOSE =  -7,
    GALOIS_HEAD_RET_ETIMEDOUT =  -8,
    GALOIS_HEAD_RET_EMAGICNUM =  -9,
    GALOIS_HEAD_RET_UNKNOWN =  -10
};
typedef int socket_t;
struct galois_head 
{
    // No virtual, No 'this' pointer, all static except
    unsigned short id;
    unsigned short version;
    unsigned int log_id;
    char provider[16];
    unsigned int magic_num;
    unsigned int reserved1;
    unsigned int reserved2;
    unsigned int reserved3;
    unsigned int body_len;

    static const unsigned int GALOIS_HEAD_MAGICNUM;
    static GALOIS_HEAD_RET_ERROR_T read(socket_t sock, galois_head * head, void * req, size_t req_size,
            void * buf, size_t buf_size, int timeout, unsigned flags);
    static ssize_t sync_read_n_tmo(socket_t fd, void * ptr1, size_t nbytes, int msecs);
    static ssize_t reado_tv(socket_t fd, void * ptr, size_t nbytes, struct timeval * tv);
    static int sreadable_tv(socket_t fd, struct timeval * tv);
    static int timeval_2_ms(const struct timeval * ptv);
    static void ms_2_timeval(const int msec, struct timeval * ptv);
    static int poll_wrap(struct pollfd * fdarray, unsigned int nfds, int timeout);
};
}