#include <stdio.h>
#include <stdlib.h>

typedef struct bitstream
{
	char* data;
	size_t offset;
	size_t curr_alloc;
} bitstream;


char bytes_needed(long n)
{
	size_t size = 0;

	while (n)
	{
		n >>= 7;
		size++;
	};

	return n ? n : 1;
};

void bs_init(bitstream* s)
{
	s->data = malloc(1);
	s->offset = 0;
	s->curr_alloc = 1;
};

void bs_exit(bitstream* s)
{
	free(s->data);
};

void bs_resize(bitstream* s, size_t needed)
{
	while (s->curr_alloc < needed)
		s->curr_alloc <<= 1;

	s->data = realloc(s->data, s->curr_alloc);
	if (!s->data)
		fprintf(stderr, "realloc returned invalid memory\n");
};

void bs_prealloc(bitstream* s, const size_t size)
{
	s->data = realloc(s->data, s->curr_alloc += size);
	if (!s->data)
		fprintf(stderr, "realloc returned invalid memory\n");
};

void bs_importfile(bitstream* s, const char* filename)
{
	const char* fname = filename ? filename : "in.dat";
	FILE* f = fopen(fname, "r");

	if (!f)
		fprintf(stderr, "cannot open file for reading\n");

	fseek(f, 0, SEEK_END);
	s->curr_alloc = ftell(f);
	fseek(f, 0, SEEK_SET);

	s->data = malloc(s->curr_alloc);

	fread(s->data, sizeof(char), s->curr_alloc, f);
	fclose(f);
};

void bs_exportfile(bitstream* s, const char* filename)
{
	const char* fname = filename ? filename : "out.dat";
	FILE* f = fopen(fname, "w+");

	if (!f)
		fprintf(stderr, "cannot open file for writing\n");

	fwrite(s->data, sizeof(char), s->offset, f);
	fclose(f);
};

#define bs_readraw(s, type, ret) \
	type __v = *(type*)(s->data + s->offset); \
	s->offset += sizeof(type); \
	ret = __v

#define bs_readenc(s, type, ret) \
	{ \
		type __v = 0; size_t __s = 0; \
		while (1) { \
			char b = *(s->data + s->offset++); \
			__v |= (b & 0x7f) << __s; \
			__s += 7; \
			if (b & 0x80) { \
				ret = __v; break; \
			}; \
		}; \
	}

#define bs_writeraw(s, type, var) \
	bs_resize(s, s->offset + sizeof(type)); \
	*(type*)(s->data + s->offset) = (type)var; \
	s->offset += sizeof(type)

#define bs_writeenc(s, type, var) \
	{ \
		type tmp = var; \
		size_t n_sz = s->offset + bytes_needed(var); \
		bs_resize(s, n_sz); \
		while (1) { \
			const char b = tmp & 0x7f; \
			if (b != tmp) { \
				*(s->data + s->offset++) = b | 0x80; \
				tmp >>= 7; \
			} else { \
				*(s->data + s->offset++) = b; break; \
			} \
		} \
	}


/*
char bs_readraw_byte(bitstream* s)
{
	short v = *(short*)(s->data + s->offset);
	s->offset += sizeof(short);
	return v;
};


short bs_readraw_word(bitstream* s)
{
	short v = *(short*)(s->data + s->offset);
	s->offset += sizeof(short);
	return v;
};


int bs_readraw_dword(bitstream* s)
{
	int v = *(int*)(s->data + s->offset);
	s->offset += sizeof(int);
	return v;
};


long bs_readraw_qword(bitstream* s)
{
	long v = *(long*)(s->data + s->offset);
	s->offset += sizeof(long);
	return v;
};
*/

int main(int argc, char* argv)
{
	bitstream _s;
	bitstream* s = &_s;

	bs_init(s);
	bs_writeraw(s, int, 69);
	bs_writeenc(s, int, 672);
	bs_exportfile(s, "test.dat");
	bs_exit(s);

	bs_init(s);
	bs_importfile(s, "test.dat");
	int a = 0, b = 0;
	bs_readraw(s, int, a);
	bs_readenc(s, int, b);
	printf("%i %i\n", a, b);
	bs_exit(s);

	return 0;
};