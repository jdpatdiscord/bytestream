#ifndef __BYTESTREAM_H
#define __BYTESTREAM_H

//#define CASSERT(predicate, file) _impl_CASSERT_LINE(predicate,__LINE__,file)
//
//#define _impl_PASTE(a,b) a##b
//#define _impl_CASSERT_LINE(predicate, line, file) \
//    typedef char _impl_PASTE(assertion_failed_##file##_,line)[2*!!(predicate)-1];

//#ifndef TRUE
//#define TRUE 1
//#endif
//#ifndef FALSE
//#define FALSE 0
//#endif

const int buffer_check = 0;
const int bounds_check = 0;

//CASSERT(sizeof(char) == 1);
//CASSERT(sizeof(short) == 2);
//CASSERT(sizeof(int) == 4);
//CASSERT(sizeof(long long) == 8);

// paranoid programming for any people that really need C

typedef long long I64;
typedef int       I32;
typedef short     I16;
typedef char      I8;

typedef unsigned long long U64;
typedef unsigned int       U32;
typedef unsigned short     U16;
typedef unsigned char      U8;

#if (sizeof(void*) == 8)
#define MAXINT U64
#endif
#if (sizeof(void*) == 4)
#define MAXINT U32
#endif
#if (sizeof(void*) == 2)
// sizeof(short) on AVR-GCC: 2
#define MAXINT U16
#endif

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

struct bitstream
{
	U8* data;
	MAXINT current_allocated;
	MAXINT offset;
};

void bs_initialize(struct bitstream* bs)
{

}

void bs_free(struct bitstream* bs)
{

}

void bs_prealloc(struct bitstream* bs, MAXINT size)
{

}

MAXINT bs_readenc(struct bitstream* bs)
{

}

U8 bs_read8(struct bitstream* bs)
{
	
}

U16 bs_read16(struct bitstream* bs)
{

}

#if (sizeof(U32) == 4)
U32 bs_read32(struct bitstream* bs)
{
	
}
#endif

#if (sizeof(U64) == 8)
U64 bs_read64(struct bitstream* bs)
{
	
}
#endif

#endif