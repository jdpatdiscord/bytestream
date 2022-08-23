#ifndef __BYTESTREAM_HPP
#define __BYTESTREAM_HPP

#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <string>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <stdexcept>

#if defined(_WIN32)
#include <Windows.h>
#include <winternl.h>
#endif

constexpr bool BoundsCheck = false;
constexpr bool BufferCheck = false;

/* feature list
flush to file [y]
load from file [y]
read encoded string [y]
write encoded string [y]
read string [y]
write string [y]
write encoded value [y]
write raw value [y]
read encoded value [y]
read raw value [y]
arbitrary read encoded [y]
arbitrary write encoded [y]
arbitrary read raw [y]
arbitrary write raw [y]
write array [y]
read array [y]
*/

inline void* malloc_Impl(size_t Size)
{
    return malloc(Size);
}
inline void* realloc_Impl(void* Buffer, size_t NewSize)
{
    return realloc(Buffer, NewSize);
}
inline void free_Impl(void* Buffer)
{
    free(Buffer);

    return;
}
inline void* memcpy_Impl(void* dst, void* src, size_t size)
{
    return memcpy(dst, src, size);
}
inline uint64_t roundpow2_64(uint64_t n)
{
    n |= (n |= (n |= (n |= (n |= (n |= (n >> 1)) >> 2) >> 4) >> 8) >> 16) >> 32;
    return n + 1; /* 0b0111 -> 0b1000 (2^n instead of 2^n-1) */
}

class Bitstream
{
private:
    char* __restrict Data;
    size_t CurrentAllocated;

    inline void ResizeNeeded(const size_t SizeNeeded)
    {
        auto CurrentAllocCache = CurrentAllocated;
        while (CurrentAllocated < SizeNeeded)
            CurrentAllocated <<= 1;
        if (CurrentAllocated != CurrentAllocCache) [[unlikely]]
        {
            if constexpr (BufferCheck)
            {
                // Safe
                auto BufferCache = Data;
                Data = (char*)realloc_Impl(Data, CurrentAllocated);
                if (Data == NULL)
                {
                    free_Impl(BufferCache);
                    throw std::runtime_error("realloc returned invalid pointer");
                }
            }
            else
            {
                // Unsafe
                Data = (char*)realloc_Impl(Data, CurrentAllocated);
            }
        }
    };

public:
    size_t Offset;

    Bitstream(const Bitstream&) = delete;

    Bitstream() : Offset(0), CurrentAllocated(1)
    {
        Data = (char*)malloc_Impl(1); /* Initial buffer for realloc to operate later */
    };

    /// <summary>
    /// Guarantees a preallocated amount of memory in the bitstream.
    /// </summary>
    /// <param name="Size"></param>
    void Preallocate(const size_t Size)
    {

        if constexpr (BufferCheck)
        {
            auto BufferCache = Data;
            Data = (char*)realloc_Impl(Data, CurrentAllocated = roundpow2_64(CurrentAllocated + Size)); /* Keep buffer power of 2 */
            if (Data == NULL) [[unlikely]]
            {
                free_Impl(BufferCache);
                throw std::runtime_error("malloc returned invalid pointer");
            }
        }
        else
        {
            Data = (char*)realloc_Impl(Data, CurrentAllocated = roundpow2_64(CurrentAllocated + Size)); /* Keep buffer power of 2 */
        }
    };

    /// <summary>
    /// Writes data to the bitstream utilizing the type of the array iff type size can be inferred by `sizeof`.
    /// </summary>
    /// <param name="Array">Pointer of the array that will be copied</param>
    /// <param name="Count">Number of elements in the array</param>
    template <typename T> inline void WriteArray(T* __restrict Array, const size_t Count)
    {
        const size_t Size = Count * sizeof(T);

        const size_t NewSize = Offset + Size;
        ResizeNeeded(NewSize);

        memcpy_Impl(Data + Offset, Array, Size);
        Offset += Size;
    };

    /// <summary>
    /// Reads data from the bitstream utilizing the type of the requested array and element count to calculate total bytes read.
    /// </summary>
    /// <param name="Array">Pointer to the array that will be written to</param>
    /// <param name="Count">Number of elements in the array</param>
    template <typename T> inline void ReadArray(T* __restrict Array, const size_t ElementCount)
    {
        const size_t ArraySize = sizeof(T) * ElementCount;

        if constexpr (BoundsCheck)
            if (Offset + ArraySize > CurrentAllocated) [[unlikely]]
                throw std::runtime_error("out of bounds read");

        if constexpr (BufferCheck)
        {
            if (Array == NULL) [[unlikely]]
            {
                throw std::runtime_error("`Array` parameter is NULL");
            }
        }
        memcpy_Impl(Array, Data + Offset, ArraySize);
        Offset += (ArraySize);

        return;
    }

    /// <summary>
    /// Provides the number of bytes needed to write a LEB128-encoded value.
    /// </summary>
    /// <param name="Value">Primitive number to be used</param>
    template <typename T> inline size_t EncodedSize(T Value) const
    {
        size_t ValueSize = 0;

        while (Value)
        {
            Value >>= 7;
            ++ValueSize;
        };

        return ValueSize + (ValueSize == 0); /* If Value == 0, the incrementation of ValueSize never occurs, and that's not good. */
    };

    /// <summary>
    /// Reads a primitive type from the bitstream.
    /// </summary>
    template <typename T> inline T ReadRaw()
    {
        static_assert(std::is_scalar_v<T>, "not a trivial (scalar) type");

        if constexpr (BoundsCheck)
            if (Offset + sizeof(T) > CurrentAllocated) [[unlikely]]
                throw std::runtime_error("out of bounds read");

        T Value = *(T*)(Data + Offset);
        Offset += sizeof(T);
        return Value;
    };

    /// <summary>
    /// Reads a LEB-128 encoded primitive type from the bitstream.
    /// </summary>
    template <typename T> inline T ReadEnc()
    {
        static_assert(std::is_arithmetic_v<T>, "not an operable type (arithmetic not supported)");

        T Value = 0;
        size_t Shift = 0;

        while (true)
        {
            if constexpr (BoundsCheck)
                if (Offset > CurrentAllocated) [[unlikely]]
                    throw std::runtime_error("out of bounds read");

            auto Byte = *(Data + Offset++);
            Value |= (Byte & 0x7f) << Shift;
            if ((Byte & 0x80) == 0)
                return Value;
            Shift += 7;
        };
    };

    template <typename T> inline T ArbitraryReadRaw(const size_t ReadOffset, size_t* __restrict NextOffset = nullptr)
    {
        static_assert(std::is_scalar_v<T>, "not a trivial (scalar) type");

        if (NextOffset != nullptr)
            *NextOffset = ReadOffset + sizeof(T);
        if constexpr (BoundsCheck)
            if (ReadOffset + sizeof(T) > ReadOffset) [[unlikely]]
                throw std::runtime_error("out of bounds arbitrary read");
        return *(T*)(Data + ReadOffset);
    };

    template <typename T> inline T ArbitraryReadEnc(size_t ReadOffset, size_t* __restrict NextOffset = nullptr)
    {
        static_assert(std::is_scalar_v<T>, "not a trivial (scalar) type");

        auto BeginOffset = ReadOffset;

        if constexpr (BoundsCheck)
            if (ReadOffset + sizeof(T) > Offset) [[unlikely]]
                throw std::runtime_error("out of bounds arbitrary read");

        T Value = 0;
        size_t Shift = 0;

        while (true)
        {
            auto Byte = *(Data + ReadOffset++);
            Value |= (Byte & 0x7f) << Shift;
            if ((Byte & 0x80) == 0)
                return Value;
            Shift += 7;
        };
        if (NextOffset != nullptr)
            *NextOffset = BeginOffset + (Shift / 7); /* Divides by 7 to get how many bytes iterated */
    };

    template <typename T> inline void ArbitraryWriteRaw(T Value, size_t WriteOffset, size_t* __restrict NextOffset = nullptr)
    {
        char* NewData = (char*)malloc_Impl(Offset + sizeof(T));
        if constexpr (BufferCheck)
        {
            if (NewData == NULL) [[unlikely]]
            {
                throw std::runtime_error("malloc returned invalid pointer");
            }
        }
        memcpy_Impl(NewData, Data, WriteOffset);
        *(T*)(NewData + WriteOffset) = Value;
        memcpy_Impl(NewData + WriteOffset + sizeof(T), Data + WriteOffset, Offset - WriteOffset);
        free_Impl(Data);
        Data = NewData;
        if (NextOffset != nullptr)
            *NextOffset = WriteOffset + sizeof(T);
    };

    template <typename T> inline void ArbitraryWriteEnc(T Value, size_t WriteOffset, size_t* __restrict NextOffset)
    {
        char* __restrict NewData = malloc_Impl(Offset + EncodedSize(Value));
        if constexpr (BufferCheck)
        {
            if (NewData == NULL) [[unlikely]]
            {
                throw std::runtime_error("malloc returned invalid pointer");
            }
        }
        memcpy_Impl(NewData, Data, WriteOffset); /* first segment */

        auto ConstWriteOffset = WriteOffset;

        while (true)
        {
            if (const uint8_t Byte = Value & 0x7f; Byte != Value)
            {
                *(NewData + WriteOffset++) = Byte | 0x80;
                Value >>= 7;
                continue;
            }
            else
            {
                *(NewData + WriteOffset++) = Byte;
                return;
            };
        };

        memcpy_Impl(NewData + WriteOffset, Data + ConstWriteOffset, Offset - ConstWriteOffset); /* second segment */

        free_Impl(Data);
        Data = NewData;

        if (NextOffset != nullptr)
            *NextOffset = WriteOffset;
    }

    /// <summary>
    /// Writes a raw value to the bitstream. Number of bytes written is based on the type passed.
    /// </summary>
    template <typename T> inline void WriteRaw(T Value)
    {
        static_assert(std::is_scalar_v<T>, "not an operable type (non-scalar)");

        constexpr size_t Size = sizeof(T);

        const size_t NewSize = Offset + Size;
        ResizeNeeded(NewSize);

        *(T*)(Data + Offset) = Value;
        Offset += Size;
    };

    /// <summary>
    /// Writes a LEB-128 encoded value to the bitstream. Number of bytes written is based on the encoded output.
    /// </summary>
    template <typename T> inline void WriteEnc(T Value)
    {
        static_assert(std::is_arithmetic_v<T>, "not an operable type (arithmetic not supported)");

        const size_t NewSize = Offset + EncodedSize(Value);
        ResizeNeeded(NewSize);

        while (true)
        {
            if (const uint8_t Byte = Value & 0x7f; Byte != Value)
            {
                *(Data + Offset++) = Byte | 0x80;
                Value >>= 7;
                continue;
            }
            else
            {
                *(Data + Offset++) = Byte;
                return;
            };
        };
    };

    /// <summary>
    /// Helper function that writes size of string (equiv. to WriteEnc) followed by the actual string. Useful in programs that need to parse quickly.
    /// </summary>
    inline void WriteEncString(const char* __restrict String, const size_t Size)
    {
        WriteEnc<size_t>(Size);
        WriteArray<const char>(String, Size);
    }

    /// <summary>
    /// Helper function that writes size of string (equiv. to WriteEnc) followed by the actual string. Useful in programs that need to parse quickly.
    /// </summary>
    inline void WriteEncString(const std::string& String, const size_t Size)
    {
        WriteEnc<size_t>(Size);
        WriteArray<const char>(String.c_str(), Size);
    };

    /// <summary>
    /// Reads and returns a null-terminated string. 
    /// </summary>
    inline std::string ReadString()
    {
        auto Begin = Data + Offset;
        auto String = Begin;
        size_t Size = 0;

        while (true)
        {
            if constexpr (BoundsCheck)
                if (String > Data + CurrentAllocated) [[unlikely]]
                    throw std::runtime_error("out of bounds read");
            if (*String++ != '\0')
                ++Size;
            else
                break;
        };

        Offset += Size;

        return std::string(Begin, Size);
    };

    /// <summary>
    /// Reads and returns a length-prefixed (LEB-128) string.
    /// </summary>
    inline std::string ReadEncString()
    {
        const size_t Size = ReadEnc<size_t>();

        if constexpr (BoundsCheck)
            if (Offset + Size > CurrentAllocated) [[unlikely]]
                throw std::runtime_error("out of bounds read");

        std::string String = std::string(Data + Offset, Size);

        Offset += Size;
        return String;
    };

    /// <summary>
    /// Writes current buffer to a file. If a file exists of the same name, it is overwritten.
    /// </summary>
    void FlushToFile(const std::string Filename, std::ios::openmode Filemode = std::ios::trunc | std::ios::binary)
    {
        std::ofstream F(Filename, Filemode);
        F.write(Data, Offset);
        F.close();
    };

    /// <summary>
    /// Reads a file from disk into the internal buffer.
    /// </summary>
    void LoadFromFile(const std::string Filename)
    {
        std::ifstream F(Filename, std::ios::binary);
        auto Filesize = std::filesystem::file_size(Filename);
        if (Data)
        {
            free_Impl(Data);
            Data = (char*)malloc_Impl(1);
        }
        auto BufferCache = Data;
        Data = (char*)realloc_Impl(Data, CurrentAllocated = roundpow2_64(Filesize));
        if constexpr (BufferCheck)
        {
            if (Data == NULL) [[unlikely]]
            {
                free_Impl(BufferCache);
                throw std::runtime_error("malloc returned invalid pointer");
            }
        }
        Offset = Filesize; /* Indicate size with Offset */
        F.read(Data, Filesize);
        F.close();
    }

    template <typename T> inline T& GetAllocatedRef()
    {
        constexpr size_t Size = sizeof(T);

        const size_t NewSize = Offset + Size;
        ResizeNeeded(NewSize);

        T& Ref = *(T*)(Data + Offset);
        Offset += Size;
        return Ref;
    }

    void Rewind()
    {
        Offset = 0;
    }

    ~Bitstream()
    {
        free_Impl(Data);
        Data = NULL;
    };
};

#endif