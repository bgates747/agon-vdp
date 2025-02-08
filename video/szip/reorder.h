#include "port.h"

#ifdef __cplusplus
extern "C" {
#endif

void reorder(unsigned char *in, unsigned char *out, uint4 length, uint recordsize);

void unreorder(unsigned char *in, unsigned char *out, uint4 length, uint recordsize);

#ifdef __cplusplus
}
#endif