#include "proj2_server.h"

#include "proj2/lib/domain_socket.h"
#include "proj2/lib/file_reader.h"
#include "proj2/lib/sha_solver.h"

#include <iostream>
#include <atomic>
#include <csignal>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

using namespace proj2;

static std::atomic<bool> terminate_flag(false);

/* Signalling the  handler */
void handle_signal(int) {
    terminate_flag.store(true);
}

/* Parsing the binary request */
Request ParseRequest(const std::string& data) {
    Request req;

    const char* ptr = data.data();
    size_t size = data.size();
    size_t offset = 0;

    auto read_u32 = [&](uint32_t &value) {
        if (offset + 4 > size) {
            throw std::runtime_error("Malformed request");
        }
        std::memcpy(&value, ptr + offset, 4);
        offset += 4;
    };

    uint32_t reply_len;
    read_u32(reply_len);

    if (offset + reply_len > size) {
        throw std::runtime_error("Malformed request");
    }

    req.reply_socket.assign(ptr + offset, reply_len);
    offset += reply_len;

    uint32_t file_count;
    read_u32(file_count);

    for (uint32_t i = 0; i < file_count; i++) {

        uint32_t path_len;
        read_u32(path_len);

        if (offset + path_len > size) {
            throw std::runtime_error("Malformed request");
        }

        std::string path(ptr + offset, path_len);
        offset += path_len;

        uint32_t rows;
        read_u32(rows);

        req.paths.push_back(path);
        req.rows.push_back(rows);
    }

    return req;
}

/* Working the thread */
void HandleRequest(const std::string& datagram) {

    Request req = ParseRequest(datagram);

    std::uint32_t max_rows = 0;

    for (auto r : req.rows) {
         if (r > max_rows) {
            max_rows = r;
         }
    }

    /* Acquiring the solvers first */
    auto solver = ShaSolvers::Checkout(max_rows);

    /* Acquire the readers */
    auto reader = FileReaders::Checkout(req.paths.size(), &solver);

    std::vector<std::vector<ReaderHandle::HashType>> file_hashes;

    reader.Process(req.paths, req.rows, &file_hashes);

    FileReaders::Checkin(std::move(reader));

    std::vector<char> output;

    for (auto& file : file_hashes) {
        for (auto& hash : file) {
            output.insert(output.end(), hash.begin(), hash.end());
        }
    }

    ShaSolvers::Checkin(std::move(solver));

    UnixDomainStreamClient client(req.reply_socket);
    client.Init();

    client.Write(output.data(), output.size());
}

/* Main server functionality */
int main(int argc, char* argv[]) {

    if (argc != 4) {
        std::cerr << "Usage: proj2-server <socket_path> <readers> <solvers>\n";
        return 1;
    }

    std::string socket_path = argv[1];
    std::uint32_t reader_count = std::stoul(argv[2]);
    std::uint32_t solver_count = std::stoul(argv[3]);

    FileReaders::Init(reader_count);
    ShaSolvers::Init(solver_count);

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    UnixDomainDatagramEndpoint server(socket_path);
    server.Init();

    while (!terminate_flag.load()) {

        std::string peer;
        std::string datagram = server.RecvFrom(&peer, 65536);

        if (datagram.empty()) {
            continue;
        }

        std::thread worker(HandleRequest, datagram);
        worker.detach();
    }

    return 0;
}
