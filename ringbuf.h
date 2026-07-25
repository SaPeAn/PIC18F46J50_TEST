#pragma once

#ifndef RING_BUF_H
#define RING_BUF_H

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

/**
 * @struct RINGBUF_t
 * @brief Ring buffer unit
 */
typedef struct RINGBUF_t {
    uint8_t* buf;         ///< Storage of the buffer
    volatile size_t tail; ///< Place of read point [cells]
    volatile size_t head; ///< Place of write point [cells]
    volatile size_t size; ///< Size of buffer [cells]
    volatile size_t cell_size; ///< Size of one cell [bytes]
} RINGBUF_t;

/**
 * @enum RINGBUF_STATUS
 * @brief Ring buf status enum
 *
 * RINGBUF_X
 * X: OK, ERR, PARAM_ERR, OVERFLOW
 */
typedef enum RINGBUF_STATUS {
    RINGBUF_OK,		  ///< Success status
    RINGBUF_ERR,      ///< Error
    RINGBUF_PARAM_ERR, ///< Parameter error
    RINGBUF_OVERFLOW, ///< Buffer overflow
    RINGBUF_EMPTY,    ///< Buffer empty
} RINGBUF_STATUS;

RINGBUF_STATUS RingBuf_Init(void* buf, uint16_t size, size_t cellsize, RINGBUF_t* rb); // Init buf
RINGBUF_STATUS RingBuf_Clear(RINGBUF_t* rb);			 	 // Clear buf
RINGBUF_STATUS RingBuf_Available(uint16_t* len, RINGBUF_t* rb); // Data available

// Put: add data to buffer
RINGBUF_STATUS RingBuf_DataPut(const void* data, uint16_t len, RINGBUF_t* rb); // Put data to the buf

// Read: Get data & flush it
RINGBUF_STATUS RingBuf_DataRead(void* data, uint16_t len, RINGBUF_t* rb); // Read data from buf

// Watch: Get data without flushing
RINGBUF_STATUS RingBuf_DataWatch(void* data, uint16_t len, RINGBUF_t* rb); // Watch data form buf

#endif /* RING_BUF_H */