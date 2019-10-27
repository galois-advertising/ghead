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

namespace galois::ghead
{
enum GHEAD_FLAGS_T {
    GHEAD_CHECK_NONE = 0,
    GHEAD_CHECK_MAGICNUM = 0x01,
    GHEAD_CHECK_PARAM = 0x02,
};

enum RETURN_CODE {
    SUCCESS = 0,
    EPARAM = -1,
    EBODYLEN = -2,
    WRITE = -3,
    READ = -4,
    READHEAD = -5,
    WRITEHEAD = -6,
    PEARCLOSE = -7,
    ETIMEDOUT = -8,
    EMAGICNUM = -9,
    UNKNOWN = -10
};
typedef int socket_t;
enum LOG_LEVEL {
    FATAL,
    ERROR,
    WARNING,
    INFO,
    DEBUG,
    TRACE
};
struct ghead 
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

    static const std::map<RETURN_CODE, std::string> return_code_to_string;
    static const unsigned int GHEAD_MAGICNUM;
    const std::string & return_code_to_string(RETURN_CODE);
    static void log(LOG_LEVEL loglevel, char * fmt, ...);
    static RETURN_CODE read(socket_t sock, ghead * head, void * req, size_t req_size,
            void * buf, size_t buf_size, int timeout, unsigned flags);
    static ssize_t sync_read_n_tmo(socket_t fd, void * ptr1, size_t nbytes, int msecs);
    static ssize_t read_n_tmo(socket_t fd, void * ptr, size_t nbytes, struct timeval * tv);
    static int read_tmo(socket_t fd, struct timeval * tv);
    static int timeval_2_ms(const struct timeval * ptv);
    static void ms_2_timeval(const int msec, struct timeval * ptv);
    static int poll_wrap(struct pollfd * fdarray, unsigned int nfds, int timeout);
};
}