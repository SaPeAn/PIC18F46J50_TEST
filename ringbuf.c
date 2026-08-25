#include "ringbuf.h"

/**
 * @brief Init ring buffer
 *
 * @param[in] buf Pointer to the allocated buffer
 * @param[in] size Size of buffer
 * @param[in] cellsize Size of 1 cell [bytes]
 * @param[in] rb #RINGBUF_t structure instance
 * @return #RINGBUF_STATUS enum
 */
RINGBUF_STATUS RingBuf_Init(void *buf, uint16_t size, uint16_t cellsize, RINGBUF_t *rb) {
    rb->size = size; // size of array
    rb->cell_size = cellsize; // size of 1 cell of array
    rb->buf = buf;      // set pointer to buffer
    RingBuf_Clear(rb); // clear all
    return rb->buf ? RINGBUF_OK : RINGBUF_PARAM_ERR;
}

/**
 * @brief Clear ring buffer
 * @note Disable interrupts while clearing
 *
 * @param[in] rb #RINGBUF_t structure instance
 * @return #RINGBUF_STATUS enum
 */
RINGBUF_STATUS RingBuf_Clear(RINGBUF_t *rb) {
    if (rb->buf == NULL) return RINGBUF_PARAM_ERR;
    rb->head = rb->tail = 0;
    return RINGBUF_OK;
}

/**
 * @brief Check available size to read
 *
 * @param[out] len Size to read [bytes]
 * @param[in] rb #RINGBUF_t structure instance
 * @return #RINGBUF_STATUS enum
 */
RINGBUF_STATUS RingBuf_Available(uint16_t *len, RINGBUF_t *rb) {
    if (rb->buf == NULL) return RINGBUF_PARAM_ERR;
    if (rb->head < rb->tail)
        *len = rb->size - rb->tail + rb->head;
    else
        *len = rb->head - rb->tail;
    return RINGBUF_OK;
}

/**
 * @brief Put some data to the buffer
 *
 * @param[in] data Data to be put
 * @param[in] len Length of data to be written [bytes]
 * @param[in] rb #RINGBUF_t structure instance
 * @return #RINGBUF_STATUS enum
 */
RINGBUF_STATUS RingBuf_DataPut(const void *data, uint16_t len, RINGBUF_t *rb) {
    if ((rb->buf == NULL) || (data == NULL)) return RINGBUF_PARAM_ERR;
    if (len >= rb->size)
        return RINGBUF_OVERFLOW;
    const char *input = data; // recast pointer
    // INPUT data index start address
    size_t s_addr = 0;
    // available space in the end of buffer
    size_t space = rb->size - rb->head;

    //for tail calc
    uint16_t avaldat = 0;
    RingBuf_Available(&avaldat, rb);
    uint16_t len_tmp = len;

    if (len > space) { // if len > available space
        // copy data to available space
        memcpy(&rb->buf[rb->head*rb->cell_size], &input[s_addr * rb->cell_size], space * rb->cell_size);
        // next writing will start from 0
        rb->head = 0;
        // new start address = space length
        s_addr = space;
        // new length = len-space
        len -= space;
    }
    // copy all the data to the buf storage
    memcpy(&rb->buf[rb->head*rb->cell_size], &input[s_addr * rb->cell_size], len * rb->cell_size);
    // shift to the next head
    rb->head += len;
    if (rb->head >= rb->size)
        rb->head -= rb->size;
    
    //for tail calc
    if((rb->size - avaldat) <= len_tmp) rb->tail = rb->head + 1;
      
    if (rb->tail >= rb->size)
        rb->tail = 0;
    
    return RINGBUF_OK;
}

/**
 * @brief Read some next data from the buffer
 *
 * @param[out] data Data from the buffer
 * @param[in] len Length of data to be read [bytes]
 * @param[in] rb #RINGBUF_t structure instance
 * @return #RINGBUF_STATUS enum
 */
RINGBUF_STATUS RingBuf_DataRead(void *data, uint16_t len, RINGBUF_t *rb) {
    if (rb->buf == NULL) return RINGBUF_PARAM_ERR;
    uint16_t aval_len = 0;
    RingBuf_Available(&aval_len, rb);
    if (len > aval_len) len = aval_len;
    // read data
    RINGBUF_STATUS st = RingBuf_DataWatch(data, len, rb);
    if (st != RINGBUF_OK)
        return st;
    // shift to the next head
    rb->tail += len;
    if (rb->tail >= rb->size)
        rb->tail -= rb->size;
    return st;
}

/**
 * @brief Watch current data in the buf
 * @note Reads data without shifting in the buffer
 *
 * @param[out] data Data from buffer
 * @param[in] len Length of data to be read [bytes]
 * @param[in] rb #RINGBUF_t structure instance
 * @return #RINGBUF_STATUS enum
 */
RINGBUF_STATUS RingBuf_DataWatch(void *data, uint16_t len, RINGBUF_t *rb) {
    if (data == NULL)
        return RINGBUF_PARAM_ERR;
    if (len > rb->size)
        return RINGBUF_OVERFLOW;
    if (rb->head == rb->tail)
        return RINGBUF_EMPTY;
    uint16_t aval_len = 0;
    RingBuf_Available(&aval_len, rb);
    if (len > aval_len) len = aval_len;
    // OUTPUT data index start address
    uint16_t s_addr = 0;
    // available space in the end of buffer
    uint16_t space = rb->size - rb->tail;
    uint16_t loc_tail = rb->tail;
    if (len > space) { // if len > available space
        // recast pointer to u8_t
        // copy data from available space
        memcpy((uint8_t*)data + (s_addr * rb->cell_size), &rb->buf[loc_tail * rb->cell_size], space * rb->cell_size);
        // next reading will start from 0
        loc_tail = 0;
        // new start address - space length
        s_addr = space;
        // new length - len-space
        len -= space;
    }
    // copy all the data from the buf storage
    memcpy((uint8_t*)data + (s_addr * rb->cell_size), &rb->buf[loc_tail * rb->cell_size], len * rb->cell_size);
    return RINGBUF_OK;
}
