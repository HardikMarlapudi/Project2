#ifndef PROJ2_SERVER_H
#define PROJ2_SERVER_H

#include <string>
#include <vector>
#include <cstdint>

struct Request {
    std::string reply_socket;
    std::vector<std::string> paths;
    std::vector<std::uint32_t> rows;
};

Request ParseRequest(const std::string& data);
void HandleRequest(const std::string& datagram);
void handle_signal(int);

#endif
