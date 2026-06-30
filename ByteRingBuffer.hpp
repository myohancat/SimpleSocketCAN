/**
 * My simple data structure library.
 *
 * Note: This code was written while learning modern C++
 *       and may contain design or implementation issues.
 *
 * author: Kyungin Kim <myohancat@naver.com>
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <array>
#include <mutex>
#include <algorithm>

template <std::size_t Capacity>
class ByteRingBuffer
{
    static_assert(Capacity > 0, "Ring Buffer capacity must be greater than 0");

public:
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    static constexpr bool SYSTEM_BIG_ENDIAN = false;
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    static constexpr bool SYSTEM_BIG_ENDIAN = true;
#else
#   error "Unknown byte order"
#endif

    ByteRingBuffer() : mSize(0), mFront(0), mRear(0) { }
    ~ByteRingBuffer() { }

    ByteRingBuffer(const ByteRingBuffer&) = delete;
    ByteRingBuffer& operator=(const ByteRingBuffer&) = delete;

    std::size_t write(const uint8_t* buf, std::size_t len)
    {
        if (buf == nullptr || len == 0)
            return 0;

        std::lock_guard<std::mutex> lock(mLock);

        std::size_t can_write = std::min(len, Capacity - mSize);
        if (can_write == 0) return 0;

        std::size_t first_part = std::min(can_write, Capacity - mRear);
        std::memcpy(mData.data() + mRear, buf, first_part);

        if (can_write > first_part)
        {
            std::memcpy(mData.data(), buf + first_part, can_write - first_part);
        }

        mRear = (mRear + can_write) % Capacity;
        mSize += can_write;

        return can_write;
    }

    std::size_t read(uint8_t* buf, std::size_t len)
    {
        if (buf == nullptr || len == 0)
            return 0;

        std::lock_guard<std::mutex> lock(mLock);

        std::size_t bytesRead = peekLocked(0, buf, len);
        if (bytesRead > 0)
            dropLocked(bytesRead);

        return bytesRead;
    }

    std::size_t peek(uint8_t* buf, std::size_t len)
    {
        if (buf == nullptr || len == 0)
            return 0;

        std::lock_guard<std::mutex> lock(mLock);
        return peekLocked(0, buf, len);
    }

    bool drop(std::size_t size)
    {
        std::lock_guard<std::mutex> lock(mLock);
        return dropLocked(size);
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock(mLock);
        mFront = 0;
        mRear  = 0;
        mSize  = 0;
    }

    bool read8(uint8_t* val);
    bool read16(uint16_t* val, bool bigEndian = SYSTEM_BIG_ENDIAN);
    bool read32(uint32_t* val, bool bigEndian = SYSTEM_BIG_ENDIAN);

    bool peek8(std::size_t offset, uint8_t* val);
    bool peek16(std::size_t offset, uint16_t* val, bool bigEndian = SYSTEM_BIG_ENDIAN);
    bool peek32(std::size_t offset, uint32_t* val, bool bigEndian = SYSTEM_BIG_ENDIAN);

    std::size_t capacity() const;
    std::size_t size() const;
    std::size_t available() const;

private:
    std::size_t peekLocked(std::size_t offset, uint8_t* buf, std::size_t len)
    {
        if (offset >= mSize || len == 0)
            return 0;

        std::size_t can_read = std::min(len, mSize - offset);
        std::size_t start_idx = (mFront + offset) % Capacity;

        std::size_t first_part = std::min(can_read, Capacity - start_idx);
        if (buf)
        {
            std::memcpy(buf, mData.data() + start_idx, first_part);
            if (can_read > first_part)
            {
                std::memcpy(buf + first_part, mData.data(), can_read - first_part);
            }
        }

        return can_read;
    }

    bool dropLocked(std::size_t size)
    {
        if (mSize < size)
            return false;

        mFront = (mFront + size) % Capacity;
        mSize -= size;

        return true;
    }

    template <typename T>
    bool peekTypeLocked(std::size_t offset, T* val, bool bigEndian)
    {
        uint8_t temp[sizeof(T)];

        if (val == nullptr)
            return false;

        if (peekLocked(offset, temp, sizeof(T)) != sizeof(T))
            return false;

        *val = 0;
        for (std::size_t i = 0; i < sizeof(T); ++i)
        {
            if (bigEndian)
                *val |= (static_cast<T>(temp[i]) << (8 * (sizeof(T) - 1 - i)));
            else
                *val |= (static_cast<T>(temp[i]) << (8 * i));
        }

        return true;
    }

    template <typename T>
    bool readTypeLocked(T* val, bool bigEndian)
    {
        if (peekTypeLocked(0, val, bigEndian))
        {
            dropLocked(sizeof(T));
            return true;
        }
        return false;
    }

private:
    mutable std::mutex mLock;

    std::array<uint8_t, Capacity> mData{};
    std::size_t mSize;
    std::size_t mFront;
    std::size_t mRear;
};

template <std::size_t Capacity>
inline std::size_t ByteRingBuffer<Capacity>::capacity() const
{
    return Capacity;
}

template <std::size_t Capacity>
inline std::size_t ByteRingBuffer<Capacity>::size() const
{
    std::lock_guard<std::mutex> lock(mLock);
    return mSize;
}

template <std::size_t Capacity>
inline std::size_t ByteRingBuffer<Capacity>::available() const
{
    std::lock_guard<std::mutex> lock(mLock);
    return Capacity - mSize;
}

template <std::size_t Capacity>
inline bool ByteRingBuffer<Capacity>::read8(uint8_t* val)
{
    std::lock_guard<std::mutex> lock(mLock);
    return readTypeLocked<uint8_t>(val, false);
}

template <std::size_t Capacity>
inline bool ByteRingBuffer<Capacity>::read16(uint16_t* val, bool bigEndian)
{
    std::lock_guard<std::mutex> lock(mLock);
    return readTypeLocked<uint16_t>(val, bigEndian);
}

template <std::size_t Capacity>
inline bool ByteRingBuffer<Capacity>::read32(uint32_t* val, bool bigEndian)
{
    std::lock_guard<std::mutex> lock(mLock);
    return readTypeLocked<uint32_t>(val, bigEndian);
}

template <std::size_t Capacity>
inline bool ByteRingBuffer<Capacity>::peek8(std::size_t offset, uint8_t* val)
{
    std::lock_guard<std::mutex> lock(mLock);
    return peekTypeLocked<uint8_t>(offset, val, false);
}

template <std::size_t Capacity>
inline bool ByteRingBuffer<Capacity>::peek16(std::size_t offset, uint16_t* val, bool bigEndian)
{
    std::lock_guard<std::mutex> lock(mLock);
    return peekTypeLocked<uint16_t>(offset, val, bigEndian);
}

template <std::size_t Capacity>
inline bool ByteRingBuffer<Capacity>::peek32(std::size_t offset, uint32_t* val, bool bigEndian)
{
    std::lock_guard<std::mutex> lock(mLock);
    return peekTypeLocked<uint32_t>(offset, val, bigEndian);
}
