//
// Created by Ling Guo on 2018/5/25.
//

#ifndef MMKV_MMAPEDFILE_H
#define MMKV_MMAPEDFILE_H

#include <string>

class MmapedFile {
    std::string m_name;
    int m_fd;
    void* m_segmentPtr;
    size_t m_segmentSize;

    // just forbid it for possibly misuse
    MmapedFile(const MmapedFile& other) = delete;
    MmapedFile& operator =(const MmapedFile& other) = delete;

public:
    MmapedFile(const std::string& path);
    ~MmapedFile();

    size_t getFileSize();
    void* getMemory();
    std::string& getName();

    bool truncate(size_t length);
};

class MMBuffer;

extern bool mkpath(char *path);
extern bool isFileExist(const std::string& nsFilePath);
extern int createFile(const std::string& nsFilePath);
extern bool removeFile(const std::string &nsFilePath);
extern MMBuffer* readWholeFile(const char *path);

#endif //MMKV_MMAPEDFILE_H
