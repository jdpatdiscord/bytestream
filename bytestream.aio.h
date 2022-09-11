#ifndef __BYTESTREAM_H
#define __BYTESTREAM_H

const int buffer_check = 0;
const int bounds_check = 0;

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

typedef long long I64;
typedef int       I32;
typedef short     I16;
typedef char      I8;

typedef unsigned long long U64;
typedef unsigned int       U32;
typedef unsigned short     U16;
typedef unsigned char      U8;

size_t encoded_size(size_t value)
{
    size_t value_size = 0;
    while (value)
    {
        value >>= 7;
        ++value_size;
    };
    return value_size + (value_size == 0);
}

size_t roundpow2(size_t n)
{
#if defined(_MSC_VER) && !defined(__clang__) // plain MSVC
    unsigned long t = 0;
#pragma intrinsic(_BitScanReverse64)
    _BitScanReverse64(&t, n);
    return t + 1;
#elif defined(__clang__) || defined(__GNUC__) // clang-cl (VisualStudio), clang, gcc
    return 1 << (64 - __builtin_clzll(n));
#else
    n |= (n |= (n |= (n |= (n |= (n |= (n >> 1)) >> 2) >> 4) >> 8) >> 16) >> 32;
    return n + 1; /* 0b0111 -> 0b1000 (2^n instead of 2^n-1) */
#endif
}

struct bitstream
{
    U8* data;
    size_t current_allocated;
    size_t offset;
};

void bs_openfile(struct bitstream* bs, const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (f)
    {
        size_t filesize;
        fseek(f, 0, SEEK_END);
        filesize = ftell(f);
        rewind(f);
        if (bs->data)
        {
            free(bs->data);
        }
        size_t bufsize = roundpow2(filesize);
        bs->data = (U8*)malloc(bufsize);
        bs->current_allocated = bufsize;
    }
}

void bs_initialize(struct bitstream* bs)
{
    // this should only run once per bitstream struct!

    bs->data = (U8*)malloc(16);
    bs->current_allocated = 16;
    bs->offset = 0;
}

void bs_free(struct bitstream* bs)
{
    free((void*)bs->data);
    bs->data = NULL;
}

void bs_prealloc(struct bitstream* bs, size_t size)
{
    bs->current_allocated = roundpow2(size);
    if (bs->data != NULL)
        bs->data = (U8*)realloc(bs->data, bs->current_allocated);
    else
        bs->data = (U8*)malloc(bs->current_allocated);
}

void bs_resize(struct bitstream* bs, size_t size)
{
    size_t current_allocated_cache = bs->current_allocated;
    while (bs->current_allocated < size)
        bs->current_allocated <<= 1;
    if (current_allocated_cache != bs->current_allocated)
        bs->data = (U8*)realloc(bs->data, bs->current_allocated);
}

size_t bs_readenc(struct bitstream* bs)
{
    size_t value = 0;
    size_t shift = 0;

    while (1)
    {
        U8 byte = *(bs->data + bs->offset++);
        value |= (byte & 0x7F) << shift;
        if ((byte & 0x80) == 0)
            return value;
        shift += 7;
    }
}

U8 bs_read8(struct bitstream* bs)
{
    U8 value = *(U8*)(bs->data + bs->offset);
    bs->offset += 1;
    return value;
}

U16 bs_read16(struct bitstream* bs)
{
    U16 value = *(U16*)(bs->data + bs->offset);
    bs->offset += 2;
    return value;
}

U32 bs_read32(struct bitstream* bs)
{
    U32 value = *(U32*)(bs->data + bs->offset);
    bs->offset += 4;
    return value;
}

U64 bs_read64(struct bitstream* bs)
{
    U64 value = *(U64*)(bs->data + bs->offset);
    bs->offset += 8;
    return value;
}

void bs_writebuffer(struct bitstream* bs, void* buffer, size_t arraysize)
{
    const size_t old_size = bs->offset;
    const size_t new_size = old_size + arraysize;
    bs_resize(bs, new_size);
    memcpy(bs->data + old_size, buffer, arraysize);
    bs->offset += arraysize;

    return;
}

void bs_writeenc(struct bitstream* bs, size_t value)
{
    const size_t new_size = bs->offset + encoded_size(value);
    bs_resize(bs, new_size);
    while (1)
    {
        const U8 byte = value & 0x7F;
        if (byte != value)
        {
            *(bs->data + bs->offset++) = byte | 0x80;
            value >>= 7;
            continue;
        }
        else
        {
            *(bs->data + bs->offset++) = byte;
            return;
        }
    }
}

void bs_write8(struct bitstream* bs, U8 value)
{
    *(U8*)(bs->data + bs->offset) = value;
    bs->offset += 1;
}

void bs_write16(struct bitstream* bs, U16 value)
{
    *(U16*)(bs->data + bs->offset) = value;
    bs->offset += 2;
}

void bs_write32(struct bitstream* bs, U32 value)
{
    *(U32*)(bs->data + bs->offset) = value;
    bs->offset += 4;
}

void bs_write64(struct bitstream* bs, U64 value)
{
    *(U64*)(bs->data + bs->offset) = value;
    bs->offset += 8;
}

#endif