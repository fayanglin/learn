#ifndef _CLIENT_R_H_
#define _CLIENT_R_H_

#ifdef __cplusplus
extern "C" {
#endif

extern int share_read_init();
extern int share_read_data(unsigned char *data, int dataLen);
extern int share_read_clear();

#ifdef __cplusplus
}
#endif

#endif
