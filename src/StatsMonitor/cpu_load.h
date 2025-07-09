#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

float cpu_load_get_core(uint8_t coreId);

float cpu_load_get_total(void);

#ifdef __cplusplus
}
#endif
