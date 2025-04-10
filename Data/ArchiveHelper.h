#pragma once

#include <zlib.h>
#include <QByteArray>
#include <QDebug>
#include <vector>

#define ZLIB_CHUNK_SIZE 16384 // Process 16KB chunks

// Helper function for zlib decompression using streaming API
QByteArray decompressZlib(const QByteArray& compressedData) {
    if (compressedData.isEmpty()) {
        return QByteArray();
    }

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;

    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        qWarning() << "zlib inflateInit failed with error code:" << ret;
        return QByteArray(); // Return empty on init error
    }

    QByteArray decompressedResult;
    std::vector<Bytef> outBuffer(ZLIB_CHUNK_SIZE); 

    strm.avail_in = compressedData.size();
    // Need non-const pointer for zlib C API
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(compressedData.constData()));

    // Run inflate() on input until output buffer is not full or end of stream
    do {
        strm.avail_out = ZLIB_CHUNK_SIZE;
        strm.next_out = outBuffer.data(); 

        ret = inflate(&strm, Z_NO_FLUSH);
        switch (ret) {
        case Z_NEED_DICT:
            ret = Z_DATA_ERROR; // Dictionary needed error
            // Fall through intentional
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
        case Z_STREAM_ERROR: // Include stream error
            qWarning() << "zlib inflate failed with error code:" << ret << strm.msg;
            inflateEnd(&strm);
            return QByteArray(); // Return empty on error
        }

        // Calculate how much data was actually decompressed in this chunk
        unsigned int have = ZLIB_CHUNK_SIZE - strm.avail_out;
        if (have > 0) {
            // Append the valid data from outBuffer to our result QByteArray
            decompressedResult.append(reinterpret_cast<const char*>(outBuffer.data()), have);
        }
    } while (strm.avail_out == 0); // Continue if the output buffer was filled completely

    // Clean up
    inflateEnd(&strm);

    // Check if stream ended properly (could potentially have ended mid-chunk)
    if (ret != Z_STREAM_END) {
        qWarning() << "zlib inflate finished, but stream did not end properly (final ret code:" << ret << "). Data might be incomplete or corrupt.";
        // Depending on requirements, you might return empty or the partial result here.
        // Returning what we have for now.
    }


    return decompressedResult;
}

// Helper function for zlib compression using streaming API
QByteArray compressZlib(const QByteArray& inputData, int level = Z_DEFAULT_COMPRESSION) {
    if (inputData.isEmpty()) {
        return QByteArray();
    }

    if (level < -1 || level > 9) { // zlib compression levels are -1 to 9
        qWarning() << "Invalid zlib compression level:" << level << ". Using default.";
        level = Z_DEFAULT_COMPRESSION;
    }

    z_stream strm;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    int ret = deflateInit(&strm, level);
    if (ret != Z_OK) {
        qWarning() << "zlib deflateInit failed with error code:" << ret;
        return QByteArray();
    }

    QByteArray compressedResult;
    std::vector<Bytef> outBuffer(ZLIB_CHUNK_SIZE);

    strm.avail_in = inputData.size();
    strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(inputData.constData()));

    // Run deflate() until input is consumed and output flushed
    do {
        strm.avail_out = ZLIB_CHUNK_SIZE;
        strm.next_out = outBuffer.data();

        // Use Z_FINISH only on the last call when all input has been provided
        ret = deflate(&strm, (strm.avail_in == 0) ? Z_FINISH : Z_NO_FLUSH);

        if (ret == Z_STREAM_ERROR) {
            qWarning() << "zlib deflate failed with stream error:" << ret;
            deflateEnd(&strm);
            return QByteArray();
        }

        unsigned int have = ZLIB_CHUNK_SIZE - strm.avail_out;
        if (have > 0) {
            compressedResult.append(reinterpret_cast<const char*>(outBuffer.data()), have);
        }
    } while (strm.avail_out == 0); // Continue if output buffer was full

    // Stream should have finished if Z_FINISH was used correctly
    if (ret != Z_STREAM_END) {
        qWarning() << "zlib deflate did not end stream properly (ret:" << ret << ")";
        // Might indicate an issue, but compression might still be partially valid?
    }

    // Clean up
    deflateEnd(&strm);

    return compressedResult;
}
