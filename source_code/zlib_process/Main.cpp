#include <iostream>
#include <fstream>
#include <vector>

#include "zlib.h"

#define CHUNKSIZE 1024

// --- Compress Functions ---

bool compressVector(std::vector<char>& input_vector, std::vector<char>& output_vector, int compressionlevel) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (deflateInit(&zs, compressionlevel) != Z_OK) { return false; }
    zs.next_in = reinterpret_cast<Bytef*>(input_vector.data());
    zs.avail_in = (uInt)(input_vector.size());
    int ret;
    char outbuffer[CHUNKSIZE];
    int num_written_bytes = 0;
    int num_bytes_to_write = 0;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = deflate(&zs, Z_FINISH);
        num_bytes_to_write = zs.total_out - num_written_bytes;
        for (int i = 0; i < num_bytes_to_write; i++) { output_vector.push_back(outbuffer[i]); }
        num_written_bytes = zs.total_out;
    } while (ret == Z_OK);
    deflateEnd(&zs);
    if (ret != Z_STREAM_END) { return false; }
    return true;
}

// --- Decompress Functions ---

bool decompressVector(std::vector<char>& input_vector, std::vector<char>& output_vector) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK) { return false; }
    zs.next_in = reinterpret_cast<Bytef*>(input_vector.data());
    zs.avail_in = (uInt)(input_vector.size());
    int ret;
    char outbuffer[CHUNKSIZE];
    int num_written_bytes = 0;
    int num_bytes_to_write = 0;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = inflate(&zs, 0);
        num_bytes_to_write = zs.total_out - num_written_bytes;
        for (int i = 0; i < num_bytes_to_write; i++) { output_vector.push_back(outbuffer[i]); }
        num_written_bytes = zs.total_out;
    } while (ret == Z_OK);
    inflateEnd(&zs);
    if (ret != Z_STREAM_END) { return false; }
    return true;
}

bool decompressArray(uint64_t compressed_size, char* compressed_data, uint64_t decompressed_size, char*& decompressed_data) {
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK) { return false; }
    zs.next_in = reinterpret_cast<Bytef*>(compressed_data);
    zs.avail_in = (uInt)(compressed_size);
    int ret;
    char outbuffer[CHUNKSIZE];
    int num_written_bytes = 0;
    int num_bytes_to_write = 0;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        ret = inflate(&zs, 0);
        num_bytes_to_write = zs.total_out - num_written_bytes;
        for (int i = 0; i < num_bytes_to_write; i++) { decompressed_data[num_written_bytes + i] = outbuffer[i]; }
        num_written_bytes = zs.total_out;
    } while (ret == Z_OK);
    inflateEnd(&zs);
    if (ret != Z_STREAM_END) { return false; }
    return true;
}

// --- Read Functions ---

std::vector<char> readFileBinary(std::string filename) {
    // Open input file
    std::ifstream inputFile(filename, std::ios_base::binary);
    // Get length of file
    inputFile.seekg(0, std::ios_base::end);
    uint64_t length = (uint64_t)(inputFile.tellg());
    inputFile.seekg(0, std::ios_base::beg);
    // Allocate buffer
    std::vector<char> buffer(length);
    // Read buffer data
    inputFile.read(reinterpret_cast<char*>(buffer.data()), length);
    inputFile.close();
    return buffer;
}

std::vector<char> readCompressedFileBinary(std::string filename, uint64_t& decompressed_size) {
    // Open input file
    std::ifstream inputFile(filename, std::ios_base::binary);
    // Get length of compressed data
    inputFile.seekg(0, std::ios_base::end);
    uint64_t length = ((uint64_t)inputFile.tellg()) - sizeof(uint64_t);
    inputFile.seekg(0, std::ios_base::beg);
    // Allocate buffer
    std::vector<char> buffer(length);
    // Get decompressed size
    inputFile.read(reinterpret_cast<char*>(&decompressed_size), sizeof(uint64_t));
    // Read buffer data
    inputFile.read(reinterpret_cast<char*>(buffer.data()), length);
    inputFile.close();
    return buffer;
}

// --- Write Functions ---

void writeFileBinaryVector(std::string filename, std::vector<char>& buffer) {
    // Open output file
    std::ofstream outputFile(filename, std::ios_base::binary);
    // Write data
    outputFile.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
    outputFile.close();
}

void writeFileBinaryArray(std::string filename, uint64_t size, char* buffer) {
    // Open output file
    std::ofstream outputFile(filename, std::ios_base::binary);
    // Write data
    outputFile.write(reinterpret_cast<char*>(buffer), size);
    outputFile.close();
}

void writeCompressedFileBinary(std::string filename, std::vector<char>& buffer, uint64_t decompressed_size) {
    // Open output file
    std::ofstream outputFile(filename, std::ios_base::binary);
    // Write decompressed size
    outputFile.write(reinterpret_cast<char*>(&decompressed_size), sizeof(uint64_t));
    // Write data
    outputFile.write(reinterpret_cast<char*>(buffer.data()), buffer.size());
    outputFile.close();
}

// --- global variables ---

std::string g_input_file_name;
std::string g_output_file_name;
bool g_compress = false;
bool g_decompress = false;

// --- parameters ---

void printhelp() {
    std::cout << "Usage:" << std::endl;
    std::cout << "./compress [PARAMETERS]" << std::endl;
    std::cout << "-help: prints this text" << std::endl;
    std::cout << "-i [FILE NAME]: Input File Name" << std::endl;
    std::cout << "-o [FILE NAME]: Output File Name" << std::endl;
    std::cout << "-c: Compress file" << std::endl;
    std::cout << "-d: Decompress file" << std::endl;
}

bool processInputParamters(int argc, char* argv[]) {
    bool iMissing = true, oMissing = true;
    bool compress = false;
    bool decompress = false;
    std::string i, o;
    for (int currArg = 1; currArg < argc ; currArg++) {
        std::string str(argv[currArg]);
        if (!str.compare("-help")) { printhelp(); return false; }
        else if (!str.compare("-i")) { i = std::string(argv[currArg + 1]); iMissing = false; }
        else if (!str.compare("-o")) { o = std::string(argv[currArg + 1]); oMissing = false; }
        else if (!str.compare("-c")) { compress = true; }
        else if (!str.compare("-d")) { decompress = true; }
    }
    bool modeMissing = (compress == decompress);
    if (iMissing || oMissing || modeMissing) {
        if (iMissing) { std::cout << "(!) Input file missing" << std::endl; }
        if (oMissing) { std::cout << "(!) Output file missing" << std::endl; }
        if (modeMissing) { std::cout << "(!) Set one mode: Compress (-c) or Decompress (-d)" << std::endl; }
        printhelp();
        return false;
    }
    g_input_file_name = i;
    g_output_file_name = o;
    g_compress = compress;
    g_decompress = decompress;
    std::cout << "Input File: " << g_input_file_name << std::endl;
    std::cout << "Output File: " << g_output_file_name << std::endl;
    if (g_compress) {
        std::cout << "Compress file" << std::endl;
    }
    else if(g_decompress) {
        std::cout << "Decompress file" << std::endl;
    }
    return true;
}

// --- Main ---

int main(int argc, char* argv[]) {
    // Process Input Parameters
    if (!processInputParamters(argc, argv)) { return EXIT_FAILURE; }

    if (g_compress) {
        std::string original_file_name = g_input_file_name;
        std::string compressed_file_name = g_output_file_name;
        // Read original data
        std::vector<char> original_bytes = readFileBinary(original_file_name);
        std::cout << "original_bytes length: " << original_bytes.size() << std::endl;
        // Compress data
        std::vector<char> compressed_bytes;
        if (!compressVector(original_bytes, compressed_bytes, Z_BEST_COMPRESSION)) {
            std::cout << "Error at compression" << std::endl;
            return 0;
        }
        std::cout << "compressed_bytes length: " << compressed_bytes.size() << std::endl;
        // Save compressed file
        writeCompressedFileBinary(compressed_file_name, compressed_bytes, (uint64_t)original_bytes.size());
        // Compression ratio
        std::cout << "Compression ratio: " << ((double)100 * compressed_bytes.size()) / ((double)original_bytes.size()) << " %" << std::endl;
    }
    else if(g_decompress) {
        std::string compressed_file_name = g_input_file_name;
        std::string decompressed_file_name = g_output_file_name;
        // Read compressed data
        uint64_t decompressed_size;
        std::vector<char> compressed_bytes = readCompressedFileBinary(compressed_file_name, decompressed_size);
        std::cout << "decompressed_size = " << decompressed_size << std::endl;
        // Decompress data
        char* decompressed_data = new char[decompressed_size];
        if (!decompressArray((uint64_t)(compressed_bytes.size()), compressed_bytes.data(), decompressed_size, decompressed_data)) {
            std::cout << "Error at decompression" << std::endl;
            return 0;
        }
        // Write decompressed data
        writeFileBinaryArray(decompressed_file_name, decompressed_size, decompressed_data);
    }
    return 0;
}