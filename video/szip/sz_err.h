#ifndef ERR_H
#define ERR_H

#include "esp_log.h"  // ESP32 logging

#define ERR_TAG "SZ_ERROR"

#ifdef __cplusplus
extern "C" {
#endif

// Define an error reporting function
#define sz_error(x) do { \
    ESP_LOGE(ERR_TAG, "Error #%x", x); \
    abort(); \
} while(0)

// Memory allocation errors
#define NOMEM             0x6500
#define SZ_NOMEM_HASH     0x6502
#define SZ_NOMEM_SORT     0x6503

// Bugs / Unexpected errors
#define UNEXPECTED        0x6600
#define SZ_NOTCYCLIC      0x6601
#define SZ_NOTFOUND       0x6602
#define SZ_NOTIMPLEMENTED 0x6603
#define SZ_DOUBLEINDIRECT 0x6604
#define AR_OUTSTANDING    0x6605

#ifdef __cplusplus
}
#endif

#endif // ERR_H
