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
arbitrary write encoded [n]
arbitrary read raw [y]
arbitrary write raw [y]
write array [y]
read array [y]
*/

inline uint64_t roundpow2_64(uint64_t n)
{
    n |= (n |= (n |= (n |= (n |= (n |= (n >> 1)) >> 2) >> 4) >> 8) >> 16) >> 32;
    return n + 1; /* 0b0111 -> 0b1000 (2^n instead of 2^n-1) */
}

class Bitstream
{
private:
    char* Data;
    size_t CurrentAllocated;

    inline void ResizeNeeded(const size_t SizeNeeded)
    {
        while (CurrentAllocated < SizeNeeded) 
            CurrentAllocated <<= 1;

        Data = (char*)realloc(Data, CurrentAllocated);
        
        if constexpr (BufferCheck)
            if (!Data)
                throw std::runtime_error("realloc returned invalid pointer");
    };

public:
    size_t Offset;

    Bitstream(const Bitstream&) = delete;

    Bitstream() : Offset(0), CurrentAllocated(1)
    {
        Data = (char*)malloc(1); /* Initial buffer for realloc to operate later */
    };

    void Preallocate(const size_t Size)
    {
        Data = (char*)realloc(Data, CurrentAllocated = roundpow2_64(CurrentAllocated + Size)); /* Keep buffer power of 2 */

        if constexpr (BufferCheck)
            if (!Data)
                throw std::runtime_error("malloc returned invalid pointer");
    };

    template <typename T> inline void WriteArray(T* Array, const size_t Count)
    {
        const size_t Size = Count * sizeof(T);

        const size_t NewSize = Offset + Size;
        ResizeNeeded(NewSize);

        memcpy(Data + Offset, Array, Size);
        Offset += Size;
    };

    template <typename T> inline T* ReadArray(const size_t ElementCount, T* Array = nullptr)
    {
        const size_t ArraySize = sizeof(T) * ElementCount;
        if (Array == NULL)
            Array = (T*)malloc(ArraySize);
        memcpy(Array, Data + Offset, ArraySize);
        Offset += (ArraySize);
        return Array;
    }

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

    template <typename T> inline T ReadRaw()
    {
        static_assert(std::is_scalar_v<T>, "not a trivial (scalar) type");

        if constexpr (BoundsCheck)
            if (Offset >= CurrentAllocated)
                throw std::runtime_error("out of bounds read");

        T Value = *(T*)(Data + Offset);
        Offset += sizeof(T);
        return Value;
    };

    template <typename T> inline T ReadEnc()
    {
        static_assert(std::is_arithmetic_v<T>, "not an operable type (arithmetic not supported)");

        T Value = 0;
        size_t Shift = 0;

        while (true)
        {
            if constexpr (BoundsCheck)
                if (Offset > CurrentAllocated)
                    throw std::runtime_error("out of bounds read");

            auto Byte = *(Data + Offset++);
            Value |= (Byte & 0x7f) << Shift;
            if ((Byte & 0x80) == 0)
                return Value;
            Shift += 7;
        };
    };

    template <typename T> inline T ArbitraryReadRaw(const size_t ReadOffset, size_t* NextOffset = nullptr)
    {
        static_assert(std::is_scalar_v<T>, "not a trivial (scalar) type");

        if (NextOffset != nullptr)
            *NextOffset = ReadOffset + sizeof(T);
        if constexpr (BoundsCheck)
            if (ReadOffset + sizeof(T) > ReadOffset)
                throw std::runtime_error("out of bounds arbitrary read");
        return *(T*)(Data + ReadOffset);
    };

    template <typename T> inline T ArbitraryReadEnc(size_t ReadOffset, size_t* NextOffset = nullptr)
    {
        static_assert(std::is_scalar_v<T>, "not a trivial (scalar) type");

        auto BeginOffset = ReadOffset;

        if constexpr (BoundsCheck)
            if (ReadOffset + sizeof(T) > Offset)
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

    template <typename T> inline void ArbitraryWriteRaw(T Value, size_t WriteOffset, size_t* NextOffset = nullptr)
    {
        char* NewData = (char*)malloc(Offset + sizeof(T));
        memcpy(NewData, Data, WriteOffset);
        *(T*)(NewData + WriteOffset) = Value;
        memcpy(NewData + WriteOffset + sizeof(T), Data + WriteOffset, Offset - WriteOffset);
        free(Data);
        Data = NewData;
        if (NextOffset != nullptr)
            *NextOffset = WriteOffset + sizeof(T);
    };

    template <typename T> inline void ArbitraryWriteEnc(T Value, size_t WriteOffset, size_t* NextOffset)
    {
        char* NewData = malloc(Offset + EncodedSize(Value));
        memcpy(NewData, Data, WriteOffset); /* first segment */

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

        memcpy(NewData + WriteOffset, Data + ConstWriteOffset, Offset - ConstWriteOffset); /* second segment */

        free(Data);
        Data = NewData;

        if (NextOffset != nullptr)
            *NextOffset = WriteOffset;
    }

    template <typename T> inline void WriteRaw(T Value)
    {
        static_assert(std::is_scalar_v<T>, "not an operable type (non-scalar)");

        constexpr size_t Size = sizeof(T);

        const size_t NewSize = Offset + Size;
        ResizeNeeded(NewSize);

        *(T*)(Data + Offset) = Value;
        Offset += Size;
    };

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

    inline void WriteEncString(const char* String, const size_t Size)
    {
        WriteEnc<size_t>(Size);
        WriteArray<const char>(String, Size);
    }
    inline void WriteEncString(const std::string& String, const size_t Size)
    {
        WriteEnc<size_t>(Size);
        WriteArray<const char>(String.c_str(), Size);
    };

    inline std::string ReadString()
    {
        auto Begin = Data + Offset;
        auto String = Begin;
        size_t Size = 0;

        while (true)
        {
            if constexpr (BoundsCheck)
                if (String > Data + CurrentAllocated)
                    throw std::runtime_error("out of bounds read");
            if (*String++ != '\0')
                ++Size;
            else
                break;
        };

        Offset += Size;

        return std::string(Begin, Size);
    };

    inline std::string ReadEncString()
    {
        const size_t Size = ReadEnc<size_t>();

        if constexpr (BoundsCheck)
            if (Offset + Size > CurrentAllocated)
                throw std::runtime_error("out of bounds read");

        std::string String = std::string(Data + Offset, Size);

        Offset += Size;
        return String;
    };

    void FlushToFile(const std::string Filename, std::ios::openmode Filemode = std::ios::trunc | std::ios::binary)
    {
        std::ofstream F(Filename, Filemode);
        F.write(Data, Offset);
        F.close();
    };

    void LoadFromFile(const std::string Filename)
    {
        std::ifstream F(Filename, std::ios::binary);
        auto Filesize = std::filesystem::file_size(Filename);
        if (Data)
        {
            free(Data);
            Data = (char*)malloc(1);
        }
        Data = (char*)realloc(Data, CurrentAllocated = roundpow2_64(Filesize));
        Offset = Filesize; /* Indicate size with Offset */
        F.read(Data, Filesize);
        F.close();
    }

    void Rewind()
    {
        Offset = 0;
    }

    ~Bitstream()
    {
        free(Data);
    };
};

#endif