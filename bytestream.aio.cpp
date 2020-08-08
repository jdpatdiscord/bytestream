#include <cstdlib>
#include <iostream>
#include <type_traits>
#include <string>
#include <fstream>
#include <cstring>
#include <stdexcept>

constexpr bool ReadBoundsCheck = false;

class Bitstream
{
private:
    char* Data;
    size_t Offset;
    size_t CurrentAllocated;

    inline void ResizeNeeded(const size_t SizeNeeded)
    {
        while (CurrentAllocated < SizeNeeded)
            CurrentAllocated <<= 1;

        Data = (char*)realloc(Data, CurrentAllocated);
        if (!Data)
            throw std::runtime_error("realloc returned invalid memory");
    };

public:
    Bitstream(const Bitstream&) = delete;

    Bitstream() : Offset(0), CurrentAllocated(1)
    {
        Data = (char*)malloc(1); /* Initial buffer for realloc to operate later */
    };

    void Preallocate(const size_t Size)
    {
        Data = (char*)realloc(Data, CurrentAllocated += Size);
        if (!Data)
            throw std::runtime_error("malloc returned invalid pointer");
    };

    template <typename T> void WriteArray(T* Array, const size_t Count)
    {
        const size_t Size = Count * sizeof(T);

        const size_t NewSize = Offset + Size;
        ResizeNeeded(NewSize);

        memcpy(Data + Offset, Array, Size);
        Offset += Size;
    };

    template <typename T> const size_t EncodedSize(T Value) const
    {
        size_t ValueSize = 0;

        while (Value)
        {
            Value >>= 7;
            ValueSize++;
        };

        return ValueSize ? ValueSize : 1; /* If Value == 0, the incrementation of ValueSize never occurs, and that's not good. */
    };

    template <typename T> T ReadRaw()
    {
        static_assert(std::is_scalar_v<T>, "not a trivial (scalar) type");

        if constexpr (ReadBoundsCheck)
            if (Offset >= CurrentAllocated)
                throw std::runtime_error("out of bounds read");

        T Value = *(T*)(Data + Offset);
        Offset += sizeof(T);
        return Value;
    };

    template <typename T> T ReadEnc()
    {
        static_assert(std::is_arithmetic_v<T>, "not an operable type (arithmetic not supported)");

        T Value = 0;
        size_t Shift = 0;

        while (true)
        {
            if constexpr (ReadBoundsCheck)
                if (Offset > CurrentAllocated)
                    throw std::runtime_error("out of bounds read");

            auto Byte = *(Data + Offset++);
            Value |= (Byte & 0x7f) << Shift;
            Shift += 7;
            if (Byte & 0x80)
                return Value;
        };
    };

    template <typename T> T ArbitraryReadRaw(const size_t ReadOffset, size_t* NextOffset = nullptr)
    {
        static_assert(std::is_scalar_v<T>, "not a trivial (scalar) type");

        if (NextOffset != nullptr)
            *NextOffset = ReadOffset + sizeof(T);
        if (ReadOffset + sizeof(T) <= Offset)
            return *(T*)(Data + ReadOffset);
        else
            throw std::runtime_error("out of bounds arbitrary read");
    };

    template <typename T> T ArbitraryReadEnc(size_t ReadOffset, size_t* NextOffset = nullptr)
    {
        static_assert(std::is_scalar_v<T>, "not a trivial (scalar) type");

        auto BeginOffset = ReadOffset;

        if (ReadOffset <= Offset)
        {
            T Value = 0;
            size_t Shift = 0;

            while (true)
            {
                auto Byte = *(Data + ReadOffset++);
                Value |= (Byte & 0x7f) << Shift;
                Shift += 7;
                if (Byte & 0x80)
                    return Value;
            };
            if (NextOffset != nullptr)
                *NextOffset = BeginOffset + (Shift / 7); /* Divides by 7 to get how many bytes iterated */
        }
        else
            throw std::runtime_error("out of bounds arbitrary read");
    };

    template <typename T> void WriteRaw(T Value)
    {
        static_assert(std::is_scalar_v<T>, "not an operable type (non-scalar)");

        constexpr size_t Size = sizeof(T);

        const size_t NewSize = Offset + Size;
        ResizeNeeded(NewSize);

        *(T*)(Data + Offset) = Value;
        Offset += Size;
    };

    template <typename T> void WriteEnc(T Value)
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

    std::string ReadString()
    {
        auto Begin = Data + Offset;
        auto String = Begin;
        size_t Size = 0;

        while (1)
        {
            if constexpr (ReadBoundsCheck)
                if (String > Data + CurrentAllocated)
                    throw std::runtime_error("out of bounds read");
            if (*String++ != '\0')
                Size++;
            else
                break;
        };

        Offset += Size;

        return std::string(Begin, Size);
    };

    std::string ReadEncString()
    {
        const size_t Size = ReadEnc<size_t>();

        if constexpr (ReadBoundsCheck)
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

    ~Bitstream()
    {
        free(Data);
    };
};