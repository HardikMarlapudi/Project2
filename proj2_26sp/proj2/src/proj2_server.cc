// Copyright Hardik Marlapudi

#include "proj2/lib/domain_socket.h"
#include "proj2/lib/file_reader.h"
#include "proj2/lib/sha_solver.h"

#include <iostream>
#include <thread>
#include <vector>
#include <cstring>

using namespace proj2;

struct ClientRequest {
    std::string reply_socket;
    std::vector<std::string> files;
    std::vector<uint32_t> rows;
};

uint32_t read_uint32(const char* &ptr) {
    uint32_t v;
    std::memcpy(&v, ptr, sizeof(uint32_t));
    ptr += sizeof(uint32_t);
    return v;
}

std::string read_string(const char* &ptr) {
    uint32_t len = read_uint32(ptr);
    std::string s(ptr, len);
    ptr += len;
    return s;
}

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
