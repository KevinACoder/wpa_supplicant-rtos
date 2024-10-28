/*
 * Copyright 2021 NXP
 * All rights reserved.
 *
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __THREADING_ALT_H__
#define __THREADING_ALT_H__

#ifdef __ZEPHYR__
#include "fsl_os_abstraction.h"
#else
#include "FreeRTOS.h"
#include "semphr.h"
#endif

/**
 * @brief Mutex struct used to synchronize mbed TLS operations.
 *
 */
typedef struct
{
#ifdef __ZEPHYR__
    struct k_mutex mutex;
#else
    SemaphoreHandle_t mutex; /**< @brief FreeRTOS semaphore. */
#endif
    char is_valid;           /**< @brief Flag used by mbedTLS to track wether a mutex is valid. */
} mbedtls_threading_mutex_t;

extern void mbedtls_threading_set_alt(void (*mutex_init)(mbedtls_threading_mutex_t *),
                                      void (*mutex_free)(mbedtls_threading_mutex_t *),
                                      int (*mutex_lock)(mbedtls_threading_mutex_t *),
                                      int (*mutex_unlock)(mbedtls_threading_mutex_t *));

#endif /* ifndef __THREADING_ALT_H__ */
