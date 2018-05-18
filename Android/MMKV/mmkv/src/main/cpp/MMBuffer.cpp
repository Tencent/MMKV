//
// Created by Ling Guo on 2018/5/4.
//

#include "MMBuffer.h"
#include <string.h>
#include <stdlib.h>
#include <utility>

MMBuffer::MMBuffer(size_t length)
        : ptr(nullptr), size(length), isNoCopy(false)
{
    if (size > 0) {
        ptr = malloc(size);
    }
}

MMBuffer::MMBuffer(void *source, size_t length, bool noCopy)
        : ptr(source), size(length), isNoCopy(noCopy)
{
    if (!isNoCopy) {
        ptr = malloc(size);
        memcpy(ptr, source, size);
    }
}

MMBuffer::MMBuffer(MMBuffer &&other) noexcept
        : ptr(other.ptr), size(other.size), isNoCopy(other.isNoCopy)
{
    other.ptr = nullptr;
    other.size = 0;
    other.isNoCopy = false;
}

MMBuffer& MMBuffer::operator=(MMBuffer &&other) noexcept
{
    std::swap(ptr, other.ptr);
    std::swap(size, other.size);
    std::swap(isNoCopy, other.isNoCopy);

    return *this;
}

MMBuffer::~MMBuffer() {
    if (!isNoCopy && ptr) {
        free(ptr);
    }
    ptr = nullptr;
}