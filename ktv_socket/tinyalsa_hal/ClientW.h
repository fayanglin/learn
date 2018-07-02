#ifndef _CLIENT_W_H_
#define _CLIENT_W_H_

#ifdef __cplusplus
extern "C" {
#endif

extern int share_write_init();
extern int share_write_data(unsigned char *data, int dataLen);
extern int share_write_clear();

#ifdef __cplusplus
}
#endif

#endif


