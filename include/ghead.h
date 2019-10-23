static const unsigned int GALOIS_HEAD_MAGICNUM = 0xe8c4a59;
struct galois_head 
{
    unsigned short id;
    unsigned short version;
    unsigned int log_id;
    char provider[16];
    unsigned int magic_num;
    unsigned int reserved1;
    unsigned int reserved2;
    unsigned int reserved3;
    unsigned int body_len;
};