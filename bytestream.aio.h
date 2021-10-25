#ifndef __BYTESTREAM_H
#define __BYTESTREAM_H

const int buffer_check = 0;
const int bounds_check = 0;

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

// paranoid programming for any people that really need C

//typedef long long I64;
typedef int       I32;
typedef short     I16;
typedef char      I8;

//typedef unsigned long long U64;
typedef unsigned int       U32;
typedef unsigned short     U16;
typedef unsigned char      U8;

MAXINT encoded_size(MAXINT value)
{
	MAXINT value_size = 0;
	while (value)
	{
		value >>= 7;
		++value_size;
	};
	return value_size + (value_size == 0);
}

//MAXINT roundpow2(MAXINT n) // 64 bit variant
//{
//    n |= (n |= (n |= (n |= (n |= (n |= (n >> 1)) >> 2) >> 4) >> 8) >> 16) >> 32;
//    return n + 1; /* 0b0111 -> 0b1000 (2^n instead of 2^n-1) */
//}

MAXINT roundpow2(MAXINT n) // 32 bit variant
{
    n |= (n |= (n |= (n |= (n |= (n >> 1)) >> 2) >> 4) >> 8) >> 16;
    return n + 1; /* 0b0111 -> 0b1000 (2^n instead of 2^n-1) */
}

struct bitstream
{
	U8* data;
	MAXINT current_allocated;
	MAXINT offset;
};

void bs_openfile(struct bitstream* bs, const char* filename)
{
	FILE* f = fopen(filename);
	if (f)
	{
		MAXINT filesize;
		fseek(f, 0, SEEK_END);
		filesize = ftell(f);
		rewind(f);
		if (bs->data)
		{
			free(bs->data);
		}
		MAXINT bufsize = roundpow2(filesize);
		bs->data = (U8*)malloc(bufsize);
		bs->current_allocated = bufsize;
	}
}

void bs_initialize(struct bitstream* bs)
{
	if (bs->data != NULL)
	{
		free(bs->data);
	}
	bs->data = (U8*)malloc(1);
	bs->current_allocated = 1;
	bs->offset = 0;
}

void bs_free(struct bitstream* bs)
{
	free((void*)bs->data);
	bs->data = NULL;
}

void bs_prealloc(struct bitstream* bs, MAXINT size)
{
	bs->current_allocated = roundpow2(size);
	bs->data = realloc(bs->data, bs->current_allocated);
}

void bs_resize(struct bitstream* bs, MAXINT size)
{
	while (bs->current_allocated < size)
	{
		bs->current_allocated <<= 1;
	}
	bs->data = (U8*)realloc(bs->data, bs->current_allocated);
}

MAXINT bs_readenc(struct bitstream* bs)
{
	MAXINT value = 0;
	MAXINT shift = 0;

	while (1)
	{
		U8 byte = *(bs->data + bs->offset++);
		value |= (byte & 0x7F) << shift;
		if ((byte & 0x80) == 0)
		{
			return value;
		}
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

//U64 bs_read64(struct bitstream* bs)
//{
//	U64 value = *(U64*)(bs->data + bs->offset);
//	bs->offset += 8;
//	return value;
//}

void bs_writeenc(struct bitstream* bs, MAXINT value)
{
	const MAXINT new_size = bs->offset + encoded_size(value);
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

//void bs_write32(struct bitstream* bs, U64 value)
//{
//	*(U64*)(bs->data + bs->offset) = value;
//	bs->offset += 8;
//}


#endif