#pragma once

#include <QByteArray>
#include <QString>
#include <zlib.h>  // Make sure zlib header is included

#include "Glob/Logger.h" // Use your logger

#define CHUNK_SIZE 16384 // Process in 16KB chunks (adjust as needed)

namespace Compressor {

    inline QByteArray compressZlib(const QByteArray& inputData, int level = Z_DEFAULT_COMPRESSION) {
        if (inputData.isEmpty()) {
            return QByteArray();
        }

        if (level < -1 || level > 9) { // zlib compression levels are -1 to 9
            Log.msg(QString("[compressZlib] Invalid zlib compression level: %1. Using default.")
                .arg(level), Logger::Level::WARNING);
            level = Z_DEFAULT_COMPRESSION;
        }

        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;

        int ret = deflateInit(&strm, level);
        if (ret != Z_OK) {
            Log.msg(QString("[compressZlib] deflateInit failed with error code: %1").arg(ret), Logger::Level::ERROR);
            return QByteArray();
        }

        QByteArray compressedResult;
        compressedResult.reserve(inputData.size() / 2 + 128); // Heuristic preallocation
        std::vector<Bytef> outBuffer(CHUNK_SIZE);

        strm.avail_in = inputData.size();
        // Need non-const pointer for zlib C API
        strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(inputData.constData()));

        // --- Main Compression Loop ---
        // Process input data until it's all consumed
        do {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = outBuffer.data();

            // Deflate with Z_NO_FLUSH while there's input
            ret = deflate(&strm, Z_NO_FLUSH);

            if (ret == Z_STREAM_ERROR) {
                Log.msg(QString("[compressZlib] deflate failed with stream error: %1").arg(ret), Logger::Level::ERROR);
                deflateEnd(&strm);
                return QByteArray();
            }

            // Calculate output generated in this chunk
            unsigned int have = CHUNK_SIZE - strm.avail_out;
            if (have > 0) {
                compressedResult.append(reinterpret_cast<const char*>(outBuffer.data()), have);
            }
        } while (strm.avail_out == 0); // Continue if output buffer was filled

        // Assert that all input should have been consumed now
        Q_ASSERT(strm.avail_in == 0);

        // --- Finalization Loop ---
        // Now flush the remaining output using Z_FINISH until deflate returns Z_STREAM_END
        do {
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = outBuffer.data();

            // Call with Z_FINISH to signal end of input and flush buffers
            ret = deflate(&strm, Z_FINISH);

            if (ret == Z_STREAM_ERROR) {
                Log.msg(QString("[compressZlib] deflate (finish stage) failed with stream error: %1").arg(ret), Logger::Level::ERROR);
                deflateEnd(&strm);
                return QByteArray(); // Error during final flush
            }

            unsigned int have = CHUNK_SIZE - strm.avail_out;
            if (have > 0) {
                compressedResult.append(reinterpret_cast<const char*>(outBuffer.data()), have);
            }
            // Keep calling with Z_FINISH until the stream ends
        } while (ret != Z_STREAM_END); // <<< Loop until Z_STREAM_END is returned

        // Clean up
        deflateEnd(&strm);

        Log.msg(QString("[compressZlib] Compression successful. Input: %1 bytes, Output: %2 bytes")
            .arg(inputData.size()).arg(compressedResult.size()), Logger::Level::DEBUG);
        return compressedResult;
    }


    // Function to decompress data using zlib inflate
    inline QByteArray decompressZlib(const QByteArray& compressedData) {
        if (compressedData.isEmpty()) {
            Log.msg(FNAME + "Input compressed data is empty, returning empty array.", Logger::Level::WARNING);
            return QByteArray();
        }

        z_stream strm;
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0; // Will be set in the loop
        strm.next_in = Z_NULL; // Will be set in the loop

        // Initialize inflate
        // Use windowBits = 15 for standard zlib, add 16 for gzip, add 32 to auto-detect header
        int windowBits = 15 + 32; // Auto-detect zlib or gzip header
        int ret = inflateInit2(&strm, windowBits);
        if (ret != Z_OK) {
            Log.msg(FNAME + "inflateInit failed, error code: " + QString::number(ret), Logger::Level::ERROR);
            return QByteArray();
        }

        QByteArray decompressedData;
        unsigned char outChunk[CHUNK_SIZE]; // Temporary buffer for output chunks
        int inputOffset = 0; // Track position in compressedData

        // --- Decompression Loop ---
        do {
            // Provide more input if needed
            if (strm.avail_in == 0) {
                int remainingInput = compressedData.size() - inputOffset;
                if (remainingInput == 0) {
                    // No more input data left to provide
                    break; // Exit loop if all input consumed
                }
                strm.avail_in = qMin(remainingInput, CHUNK_SIZE); // Provide up to CHUNK_SIZE input
                strm.next_in = (Bytef*)compressedData.constData() + inputOffset;
                inputOffset += strm.avail_in;
                // Log.msg(FNAME + "Providing inflate input. avail_in=" + QString::number(strm.avail_in), Logger::Level::DEBUG);
            }

            // Provide output buffer space
            strm.avail_out = CHUNK_SIZE;
            strm.next_out = outChunk;

            // Perform decompression - Z_NO_FLUSH is typical for inflate
            // Log.msg(FNAME + "Calling inflate. avail_in=" + QString::number(strm.avail_in) + ", avail_out=" + QString::number(strm.avail_out), Logger::Level::DEBUG);
            ret = inflate(&strm, Z_NO_FLUSH);
            // Log.msg(FNAME + "inflate returned: " + QString::number(ret), Logger::Level::DEBUG);


            // Check for errors
            switch (ret) {
            case Z_STREAM_ERROR:
                Log.msg(FNAME + "inflate stream error!", Logger::Level::ERROR);
                inflateEnd(&strm);
                return QByteArray(); // Critical error
            case Z_NEED_DICT:
                Log.msg(FNAME + "inflate needs dictionary (not supported)!", Logger::Level::ERROR);
                inflateEnd(&strm);
                return QByteArray();
            case Z_DATA_ERROR:
                Log.msg(FNAME + "inflate data error (input data corrupted?)!", Logger::Level::ERROR);
                inflateEnd(&strm);
                return QByteArray();
            case Z_MEM_ERROR:
                Log.msg(FNAME + "inflate memory error!", Logger::Level::ERROR);
                inflateEnd(&strm);
                return QByteArray();
            }

            // Calculate how much data was written
            unsigned have = CHUNK_SIZE - strm.avail_out;
            // Log.msg(FNAME + "inflate produced bytes: " + QString::number(have), Logger::Level::DEBUG);

            if (have > 0) {
                decompressedData.append((const char*)outChunk, have);
            }

            // Continue loop if inflate returned Z_OK (meaning more processing possible/needed)
            // Exit loop if Z_STREAM_END is reached or an error occurred
        } while (ret == Z_OK); // Loop while inflate indicates success and potentially more work

        // --- Final Check ---
        if (ret != Z_STREAM_END) {
            // This is the warning you were seeing! It means inflate finished processing
            // all input or filled output, but didn't find the end-of-stream marker.
            // This usually means the compressed data was truncated or corrupt.
            Log.msg(FNAME + "zlib inflate finished, but stream did not end properly (final ret code: "
                + QString::number(ret) + "). Data might be incomplete or corrupt.", Logger::Level::WARNING);
        }
        // --- End Final Check ---


        // Clean up the decompressor
        inflateEnd(&strm);

        Log.msg(FNAME + "Decompression finished. Input size: " + QString::number(compressedData.size())
            + ", Output size: " + QString::number(decompressedData.size()), Logger::Level::DEBUG);

        return decompressedData;
    }

} // namespace Compressor