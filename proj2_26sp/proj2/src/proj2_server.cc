// Copyright 2026 Hardik Marlapudi. All Rights Reserved.

#include "proj2/lib/domain_socket.h"
#include "proj2/lib/file_reader.h"
#include "proj2/lib/sha_solver.h"

#include <iostream>
#include <thread>
#include <vector>
#include <cstring>

using namespace proj2;

/**
 * @struct ClientRequest
 * @brief Represents a parsed request from a client.
 * 
 * The request contains:
 * - reply socket : path of the socket where the server sends the response
 * - files        : list of file paths requested by the client
 * - rows         : number of rows to hash for each file
 */

struct ClientRequest {
    std::string reply_socket;
    std::vector<std::string> files;
    std::vector<uint32_t> rows;
};

/**
 * @brief Reads a 32-bit unsigned integer from the message buffer.
 * 
 * The pointer is advanced after the value is read.
 * 
 * @param ptr Pointer to the current position in the message buffer
 * @return uint32_t value read from the buffer
 */

uint32_t read_uint32(const char* &ptr) {
    uint32_t v;
    std::memcpy(&v, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    return v;
}

/**
 * @brief Reads a length-prefixed string from the message buffer.
 * 
 * Format
 * [uint32 length][string characters]
 * 
 * @param ptr Pointer to the current position in the message buffer
 * @return std::string extracted string
 */

std::string read_string(const char* &ptr) {
    uint32_t len = read_uint32(ptr);
    std::string s(ptr, len);
    ptr += len;
    return s;
}

/**
 * @brief Parses a raw message from the client into a ClientRequest
 * 
 * Message format:
 * reply_socket
 * file_count
 * file_path
 * rows
 * file_path
 * rows
 * 
 * @param msg Raw message received from the client
 * @return ClientRequest parsed request object
 */

ClientRequest parse_request(const std::string &msg) {

    const char* ptr = msg.data();

    ClientRequest req;

    req.reply_socket = read_string(ptr);

    uint32_t file_count = read_uint32(ptr);

    for (uint32_t i=0; i < file_count; i++) {
        std::string path = read_string(ptr);
        uint32_t rows = read_uint32(ptr);

        req.files.push_back(path);
        req.rows.push_back(rows);
    }

    return req;
}

/**
 * @brief Handles a single client request.
 * 
 * Steps that are performed:
 * 1. Parse the request message
 * 2. Determine total rows required
 * 3. Checkout solver and reader resources
 * 4. Process file hashing
 * 5. Send hash results back to the client
 * 6. Return resources to the pools
 * 
 * @param msg Raw message received from the client
 */

void process_request(const std::string &msg) {

    ClientRequest req = parse_request(msg);

    uint32_t total_rows = 0;
    
    for(auto r : req.rows)
        total_rows += r;

    auto solver = ShaSolvers::Checkout(total_rows);
    auto reader = FileReaders::Checkout(req.files.size(), &solver);

    std::vector<std::vector<ReaderHandle::HashType>> hashes(req.files.size());

    reader.Process(req.files, req.rows, &hashes);

    std::vector<char> result;
    result.reserve(total_rows * 64);

    for(auto &file_hashes : hashes)
        for(auto &h : file_hashes)
            result.insert(result.end(), h.begin(), h.end());

    UnixDomainStreamClient client(req.reply_socket);
    client.Init();

    if (!result.empty()) {
        client.Write(result.data(), result.size());
    }

    FileReaders::Checkin(std::move(reader));
    ShaSolvers::Checkin(std::move(solver));
}

/**
 * @brief Entry point of the concurrent SHA256 server.
 * 
 * Usage:
 * proj2-server <socket_path> <reader_threads> <solver_threads>
 * 
 * Example:
 * ./proj2-server /tmp/proj2.sock 4 32
 * 
 * @param argc number of command line arguments 
 * @param argv command line argument values
 * @return int program exit status
 */

int main(int argc, char** argv) {

    if(argc != 4) {
        std::cerr << "Usage: proj2-server <socket> <readers> <solvers>\n";
        return 1;
    }

    std::string socket_path = argv[1];
    uint32_t reader_threads = std::stoul(argv[2]);
    uint32_t solver_threads = std::stoul(argv[3]);

    FileReaders::Init(reader_threads);
    ShaSolvers::Init(solver_threads);

    UnixDomainDatagramEndpoint server(socket_path);
    server.Init();

    while(true) {

        std::string peer;
        std::string msg = server.RecvFrom(&peer, 65536);

        std::thread worker([msg]() {
            process_request(msg);
        });

        worker.detach();
    }

    return 0;
}
