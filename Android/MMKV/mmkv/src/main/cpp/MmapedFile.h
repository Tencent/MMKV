//
// Created by Ling Guo on 2018/5/25.
//

#ifndef MMKV_MMAPEDFILE_H
#define MMKV_MMAPEDFILE_H

#include <string>

class MmapedFile {
    std::string m_name;
    int m_fd;
    void *m_segmentPtr;
    size_t m_segmentSize;

    // just forbid it for possibly misuse
    MmapedFile(const MmapedFile &other) = delete;
    MmapedFile &operator=(const MmapedFile &other) = delete;

public:
    MmapedFile(const std::string &path);
    ~MmapedFile();

    size_t getFileSize() { return m_segmentSize; }

    void *getMemory() { return m_segmentPtr; }

    std::string &getName() { return m_name; }

    int getFd() { return m_fd; }
};

class MMBuffer;

extern bool mkPath(char *path);
extern bool isFileExist(const std::string &nsFilePath);
extern bool removeFile(const std::string &nsFilePath);
extern MMBuffer *readWholeFile(const char *path);
extern bool zeroFillFile(int fd, size_t startPos, size_t size);

#endif //MMKV_MMAPEDFILE_H