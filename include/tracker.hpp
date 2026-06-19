#pragma once

#include <string>
#include <asio.hpp>
class TrackerClient {
    public:

        static std::string announce(const std::string& announce_url,
        const std::string& info_hash,
        long long total_length);

    private:

        static bool parse_url(const std::string& url, std::string& host, std::string& port ,std::string& path);
};

inline std::string generate_peer_id() {
    std::string id = "-MT0001-";
    while (id.length() < 20) {
        id += std::to_string(rand() % 10);
    }
    return id;
}