#ifndef __BYTESTREAM
#define __BYTESTREAM

#include <cstdlib>
#include <type_traits>
#include <string>
#include <fstream>
#include <cstring>
#include <stdexcept>

class Bitstream
{
private:
    char* Data;
    size_t Offset;
    size_t CurrentAllocated;

    inline void ResizeNeeded(const size_t SizeNeeded);
public:
	Bitstream(const Bitstream&) = delete;

	Bitstream();

	void Preallocate(const size_t Size);
	template <typename T> void WriteArray(T* Array, const size_t Count);
	template <typename T> const size_t EncodedSize(T Value) const;
	template <typename T> T ReadRaw();
	template <typename T> T ReadEnc();
	template <typename T> T ArbitraryReadRaw(const size_t ReadOffset, size_t* NextOffset = nullptr);
	template <typename T> T ArbitraryReadEnc(size_t ReadOffset, size_t* NextOffset = nullptr);
	template <typename T> void WriteRaw(T Value);
	template <typename T> void WriteEnc(T Value);
	inline void WriteEncString(const char* String, const size_t Size);
	inline void WriteEncString(const std::string& String, const size_t Size);
	std::string ReadString();
	std::string ReadEncString();
	void FlushToFile(const std::string Filename, int Filemode = std::ios::trunc | std::ios::binary);

	~Bitstream();
}

#endif