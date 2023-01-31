#pragma once

#include <iomanip>
#include <chrono>
#include <sys/stat.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <ios>
#include <sstream>
#include <thread>
#include <list>
#include <vector>
#include <condition_variable>

#include "logger.h"

extern simplelogger::Logger* logger;


#ifdef _WINERROR_
inline bool check(HRESULT e, int iLine, const char* szFile) {
    if (e != S_OK) {
        std::stringstream stream;
        stream << std::hex << std::uppercase << e;
        LOG(FATAL) << "HRESULT error 0x" << stream.str() << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}
#endif


#if defined(__gl_h_) || defined(__GL_H__)
inline bool check(GLenum e, int iLine, const char* szFile) {
    if (e != 0) {
        LOG(ERROR) << "GLenum error " << e << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}
#endif

inline bool check(int e, int iLine, const char* szFile) {
    if (e < 0) {
        LOG(ERROR) << "General error " << e << " at line " << iLine << " in file " << szFile;
        return false;
    }
    return true;
}

#define ck(call) check(call, __LINE__, __FILE__)
#define MAKE_FOURCC( ch0, ch1, ch2, ch3 )                               \
                ( (uint32_t)(uint8_t)(ch0) | ( (uint32_t)(uint8_t)(ch1) << 8 ) |    \
                ( (uint32_t)(uint8_t)(ch2) << 16 ) | ( (uint32_t)(uint8_t)(ch3) << 24 ) )

/**
* @brief Wrapper class around std::thread
*/
class NvThread
{
public:
    NvThread() = default;
    NvThread(const NvThread&) = delete;
    NvThread& operator=(const NvThread& other) = delete;

    NvThread(std::thread&& thread) : t(std::move(thread))
    {

    }

    NvThread(NvThread&& thread) : t(std::move(thread.t))
    {

    }

    NvThread& operator=(NvThread&& other)
    {
        t = std::move(other.t);
        return *this;
    }

    ~NvThread()
    {
        join();
    }

    void join()
    {
        if (t.joinable())
        {
            t.join();
        }
    }
private:
    std::thread t;
};



#ifndef _WIN32
#define _stricmp strcasecmp
#define _stat64 stat64
#endif

/**
* @brief Utility class to allocate buffer memory. Helps avoid I/O during the encode/decode loop in case of performance tests.
*/
class BufferedFileReader {
public:
    /**
    * @brief Constructor function to allocate appropriate memory and copy file contents into it
    */
    BufferedFileReader(const char* szFileName, bool bPartial = false) {
        struct _stat64 st;

        if (_stat64(szFileName, &st) != 0) {
            return;
        }

        nSize = st.st_size;
        while (nSize) {
            try {
                pBuf = new uint8_t[(size_t)nSize];
                if (nSize != st.st_size) {
                    LOG(WARNING) << "File is too large - only " << std::setprecision(4) << 100.0 * nSize / st.st_size << "% is loaded";
                }
                break;
            }
            catch (std::bad_alloc) {
                if (!bPartial) {
                    LOG(ERROR) << "Failed to allocate memory in BufferedReader";
                    return;
                }
                nSize = (uint32_t)(nSize * 0.9);
            }
        }

        std::ifstream fpIn(szFileName, std::ifstream::in | std::ifstream::binary);
        if (!fpIn)
        {
            LOG(ERROR) << "Unable to open input file: " << szFileName;
            return;
        }

        std::streamsize nRead = fpIn.read(reinterpret_cast<char*>(pBuf), nSize).gcount();
        fpIn.close();

        assert(nRead == nSize);
    }
    ~BufferedFileReader() {
        if (pBuf) {
            delete[] pBuf;
        }
    }
    bool GetBuffer(uint8_t** ppBuf, uint64_t* pnSize) {
        if (!pBuf) {
            return false;
        }

        *ppBuf = pBuf;
        *pnSize = nSize;
        return true;
    }

private:
    uint8_t* pBuf = NULL;
    uint64_t nSize = 0;
};

/**
* @brief Template class to facilitate color space conversion
*/
template<typename T>
class YuvConverter {
public:
    YuvConverter(int nWidth, int nHeight) : nWidth(nWidth), nHeight(nHeight) {
        pQuad = new T[((nWidth + 1) / 2) * ((nHeight + 1) / 2)];
    }
    ~YuvConverter() {
        delete[] pQuad;
    }
    void PlanarToUVInterleaved(T* pFrame, int nPitch = 0) {
        if (nPitch == 0) {
            nPitch = nWidth;
        }

        // sizes of source surface plane
        int nSizePlaneY = nPitch * nHeight;
        int nSizePlaneU = ((nPitch + 1) / 2) * ((nHeight + 1) / 2);
        int nSizePlaneV = nSizePlaneU;

        T* puv = pFrame + nSizePlaneY;
        if (nPitch == nWidth) {
            memcpy(pQuad, puv, nSizePlaneU * sizeof(T));
        }
        else {
            for (int i = 0; i < (nHeight + 1) / 2; i++) {
                memcpy(pQuad + ((nWidth + 1) / 2) * i, puv + ((nPitch + 1) / 2) * i, ((nWidth + 1) / 2) * sizeof(T));
            }
        }
        T* pv = puv + nSizePlaneU;
        for (int y = 0; y < (nHeight + 1) / 2; y++) {
            for (int x = 0; x < (nWidth + 1) / 2; x++) {
                puv[y * nPitch + x * 2] = pQuad[y * ((nWidth + 1) / 2) + x];
                puv[y * nPitch + x * 2 + 1] = pv[y * ((nPitch + 1) / 2) + x];
            }
        }
    }
    void UVInterleavedToPlanar(T* pFrame, int nPitch = 0) {
        if (nPitch == 0) {
            nPitch = nWidth;
        }

        // sizes of source surface plane
        int nSizePlaneY = nPitch * nHeight;
        int nSizePlaneU = ((nPitch + 1) / 2) * ((nHeight + 1) / 2);
        int nSizePlaneV = nSizePlaneU;

        T* puv = pFrame + nSizePlaneY,
            * pu = puv,
            * pv = puv + nSizePlaneU;

        // split chroma from interleave to planar
        for (int y = 0; y < (nHeight + 1) / 2; y++) {
            for (int x = 0; x < (nWidth + 1) / 2; x++) {
                pu[y * ((nPitch + 1) / 2) + x] = puv[y * nPitch + x * 2];
                pQuad[y * ((nWidth + 1) / 2) + x] = puv[y * nPitch + x * 2 + 1];
            }
        }
        if (nPitch == nWidth) {
            memcpy(pv, pQuad, nSizePlaneV * sizeof(T));
        }
        else {
            for (int i = 0; i < (nHeight + 1) / 2; i++) {
                memcpy(pv + ((nPitch + 1) / 2) * i, pQuad + ((nWidth + 1) / 2) * i, ((nWidth + 1) / 2) * sizeof(T));
            }
        }
    }

private:
    T* pQuad;
    int nWidth, nHeight;
};

/**
* @brief Class for writing IVF format header for AV1 codec
*/
class IVFUtils {
public:
    void WriteFileHeader(std::vector<uint8_t>& vPacket, uint32_t nFourCC, uint32_t nWidth, uint32_t nHeight, uint32_t nFrameRateNum, uint32_t nFrameRateDen, uint32_t nFrameCnt)
    {
        char header[32];

        header[0] = 'D';
        header[1] = 'K';
        header[2] = 'I';
        header[3] = 'F';
        mem_put_le16(header + 4, 0);                    // version
        mem_put_le16(header + 6, 32);                   // header size
        mem_put_le32(header + 8, nFourCC);              // fourcc
        mem_put_le16(header + 12, nWidth);              // width
        mem_put_le16(header + 14, nHeight);             // height
        mem_put_le32(header + 16, nFrameRateNum);       // rate
        mem_put_le32(header + 20, nFrameRateDen);       // scale
        mem_put_le32(header + 24, nFrameCnt);           // length
        mem_put_le32(header + 28, 0);                   // unused

        vPacket.insert(vPacket.end(), &header[0], &header[32]);
    }

    void WriteFrameHeader(std::vector<uint8_t>& vPacket, size_t nFrameSize, int64_t pts)
    {
        char header[12];
        mem_put_le32(header, (int)nFrameSize);
        mem_put_le32(header + 4, (int)(pts & 0xFFFFFFFF));
        mem_put_le32(header + 8, (int)(pts >> 32));

        vPacket.insert(vPacket.end(), &header[0], &header[12]);
    }

private:
    static inline void mem_put_le32(void* vmem, int val)
    {
        unsigned char* mem = (unsigned char*)vmem;
        mem[0] = (unsigned char)((val >> 0) & 0xff);
        mem[1] = (unsigned char)((val >> 8) & 0xff);
        mem[2] = (unsigned char)((val >> 16) & 0xff);
        mem[3] = (unsigned char)((val >> 24) & 0xff);
    }

    static inline void mem_put_le16(void* vmem, int val)
    {
        unsigned char* mem = (unsigned char*)vmem;
        mem[0] = (unsigned char)((val >> 0) & 0xff);
        mem[1] = (unsigned char)((val >> 8) & 0xff);
    }

};

/**
* @brief Utility class to measure elapsed time in seconds between the block of executed code
*/
class StopWatch {
public:
    void Start() {
        t0 = std::chrono::high_resolution_clock::now();
    }
    double Stop() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch() - t0.time_since_epoch()).count() / 1.0e9;
    }

private:
    std::chrono::high_resolution_clock::time_point t0;
};

template<typename T>
class ConcurrentQueue
{
public:

    ConcurrentQueue() {}
    ConcurrentQueue(size_t size) : maxSize(size) {}
    ConcurrentQueue(const ConcurrentQueue&) = delete;
    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;

    void setSize(size_t s) {
        maxSize = s;
    }

    void push_back(const T& value) {
        // Do not use a std::lock_guard here. We will need to explicitly
        // unlock before notify_one as the other waiting thread will
        // automatically try to acquire mutex once it wakes up
        // (which will happen on notify_one)
        std::unique_lock<std::mutex> lock(m_mutex);
        auto wasEmpty = m_List.empty();

        while (full()) {
            m_cond.wait(lock);
        }

        m_List.push_back(value);
        if (wasEmpty && !m_List.empty()) {
            lock.unlock();
            m_cond.notify_one();
        }
    }

    T pop_front() {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (m_List.empty()) {
            m_cond.wait(lock);
        }
        auto wasFull = full();
        T data = std::move(m_List.front());
        m_List.pop_front();

        if (wasFull && !full()) {
            lock.unlock();
            m_cond.notify_one();
        }

        return data;
    }

    T front() {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (m_List.empty()) {
            m_cond.wait(lock);
        }

        return m_List.front();
    }

    size_t size() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_List.size();
    }

    bool empty() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_List.empty();
    }
    void clear() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_List.clear();
    }

private:
    bool full() {
        if (m_List.size() == maxSize)
            return true;
        return false;
    }

private:
    std::list<T> m_List;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    size_t maxSize;
};

inline void CheckInputFile(const char* szInFilePath) {
    std::ifstream fpIn(szInFilePath, std::ios::in | std::ios::binary);
    if (fpIn.fail()) {
        std::ostringstream err;
        err << "Unable to open input file: " << szInFilePath << std::endl;
        throw std::invalid_argument(err.str());
    }
}

inline void ValidateResolution(int nWidth, int nHeight) {

    if (nWidth <= 0 || nHeight <= 0) {
        std::ostringstream err;
        err << "Please specify positive non zero resolution as -s WxH. Current resolution is " << nWidth << "x" << nHeight << std::endl;
        throw std::invalid_argument(err.str());
    }
}
