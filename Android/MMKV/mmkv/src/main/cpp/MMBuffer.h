//
// Created by Ling Guo on 2018/5/4.
//

#ifndef MMKV_MMBUFFER_H
#define MMKV_MMBUFFER_H

#include <stdint.h>

class MMBuffer {
private:
    void* ptr;
    size_t size;
    bool isNoCopy;
public:
    void* getPtr() const {
        return ptr;
    }
    size_t length() const {
        return size;
    }

    MMBuffer(size_t length = 0);
    MMBuffer(void* source, size_t length, bool noCopy = false);
    MMBuffer(MMBuffer&& other) noexcept;
    MMBuffer&operator =(MMBuffer&& other) noexcept;
    ~MMBuffer();

private:
    // those are expensive, just forbid it for possibly misuse
    MMBuffer(const MMBuffer& other) = delete;
    MMBuffer& operator =(const MMBuffer& other) = delete;
};


#endif //MMKV_MMBUFFER_H
