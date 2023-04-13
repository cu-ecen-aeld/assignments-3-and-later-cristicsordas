/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

uint8_t getNextPos(uint8_t current_pos)
{
    uint8_t next_pos = 0;
    if(current_pos != AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED-1)
    {
        next_pos = current_pos + 1;
    }
    return next_pos;
}

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    uint8_t it = buffer->out_offs;
    size_t it_size=0, prev_size=0;
    struct aesd_buffer_entry *entry = NULL;
    int i=0;
    for(i=0; i<AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++)
    {
        it_size += buffer->entry[it].size;
        if((char_offset >= prev_size) && (char_offset < it_size))
        {
            *entry_offset_byte_rtn = char_offset - prev_size;
            entry = &buffer->entry[it];
            break;
        }
        prev_size = it_size;
        it = getNextPos(it);
    }

    return entry;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
char* aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    const char *ptr_replaced_entry = NULL;
    if(buffer->full)
    {
        buffer->out_offs = getNextPos(buffer->in_offs);
        ptr_replaced_entry = buffer->entry[buffer->in_offs].buffptr;
    }

    buffer->entry[buffer->in_offs].buffptr = add_entry->buffptr;
    buffer->entry[buffer->in_offs].size = add_entry->size;

    buffer->in_offs = getNextPos(buffer->in_offs);
    buffer->full = (buffer->in_offs == buffer->out_offs);
    return ptr_replaced_entry;
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
    buffer->in_offs = 0;
    buffer->out_offs = 0;
    buffer->full = false;
}
