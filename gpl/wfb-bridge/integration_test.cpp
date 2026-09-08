// SPDX-License-Identifier: GPL-3.0-only

#include "fpv4mac/devourer_json.hpp"

#include "rx.hpp"

#include <sodium.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void require(const bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path write_ground_key(std::array<std::uint8_t, crypto_box_PUBLICKEYBYTES>& gs_public,
                                       std::array<std::uint8_t, crypto_box_SECRETKEYBYTES>& gs_secret,
                                       std::array<std::uint8_t, crypto_box_PUBLICKEYBYTES>& drone_public,
                                       std::array<std::uint8_t, crypto_box_SECRETKEYBYTES>& drone_secret) {
    require(crypto_box_keypair(gs_public.data(), gs_secret.data()) == 0,
            "generating ground keypair failed");
    require(crypto_box_keypair(drone_public.data(), drone_secret.data()) == 0,
            "generating drone keypair failed");
    const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      ("fpv4mac-wfb-test-" + std::to_string(suffix) + ".key");
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(gs_secret.data()), gs_secret.size());
    output.write(reinterpret_cast<const char*>(drone_public.data()), drone_public.size());
    require(output.good(), "writing test ground key failed");
    return path;
}

std::vector<std::uint8_t> make_session(
    const std::array<std::uint8_t, crypto_box_PUBLICKEYBYTES>& gs_public,
    const std::array<std::uint8_t, crypto_box_SECRETKEYBYTES>& drone_secret,
    const std::array<std::uint8_t, crypto_aead_chacha20poly1305_KEYBYTES>& session_key,
    const std::uint8_t k, const std::uint8_t n) {
    std::vector<std::uint8_t> packet(sizeof(wsession_hdr_t) + sizeof(wsession_data_t) +
                                     crypto_box_MACBYTES);
    auto* header = reinterpret_cast<wsession_hdr_t*>(packet.data());
    header->packet_type = WFB_PACKET_SESSION;
    randombytes_buf(header->session_nonce, sizeof(header->session_nonce));

    std::array<std::uint8_t, sizeof(wsession_data_t)> plaintext{};
    auto* data = reinterpret_cast<wsession_data_t*>(plaintext.data());
    data->epoch = htobe64(1);
    data->channel_id = htobe32(0);
    data->fec_type = WFB_FEC_VDM_RS;
    data->k = k;
    data->n = n;
    std::copy(session_key.begin(), session_key.end(), data->session_key);
    require(crypto_box_easy(packet.data() + sizeof(wsession_hdr_t), plaintext.data(),
                            plaintext.size(), header->session_nonce, gs_public.data(),
                            drone_secret.data()) == 0,
            "encrypting session failed");
    return packet;
}

std::vector<std::uint8_t> encrypt_fragment(
    const std::array<std::uint8_t, crypto_aead_chacha20poly1305_KEYBYTES>& session_key,
    const std::uint8_t* plaintext, const std::size_t plaintext_size,
    const std::uint8_t fragment_index) {
    std::vector<std::uint8_t> packet(sizeof(wblock_hdr_t) + plaintext_size +
                                     crypto_aead_chacha20poly1305_ABYTES);
    auto* block = reinterpret_cast<wblock_hdr_t*>(packet.data());
    block->packet_type = WFB_PACKET_DATA;
    block->data_nonce = htobe64(fragment_index);
    unsigned long long encrypted_size{};
    require(crypto_aead_chacha20poly1305_encrypt(
                packet.data() + sizeof(wblock_hdr_t), &encrypted_size, plaintext,
                plaintext_size, packet.data(), sizeof(wblock_hdr_t), nullptr,
                reinterpret_cast<const std::uint8_t*>(&block->data_nonce), session_key.data()) == 0,
            "encrypting data packet failed");
    packet.resize(sizeof(wblock_hdr_t) + encrypted_size);
    return packet;
}

void fill_primary(std::uint8_t* block, const std::size_t allocated_size,
                  const std::string& payload) {
    require(sizeof(wpacket_hdr_t) + payload.size() <= allocated_size,
            "test payload exceeds FEC block");
    std::memset(block, 0, allocated_size);
    auto* header = reinterpret_cast<wpacket_hdr_t*>(block);
    header->flags = 0;
    header->packet_size = htons(static_cast<std::uint16_t>(payload.size()));
    std::memcpy(block + sizeof(wpacket_hdr_t), payload.data(), payload.size());
}

std::string receive_datagram(const int socket_fd) {
    std::array<char, 128> received{};
    const auto size = recv(socket_fd, received.data(), received.size(), 0);
    require(size >= 0, "timed out waiting for recovered UDP payload");
    return {received.data(), static_cast<std::size_t>(size)};
}

} // namespace

int main() {
    require(sodium_init() >= 0, "libsodium initialization failed");
    std::array<std::uint8_t, crypto_box_PUBLICKEYBYTES> gs_public{};
    std::array<std::uint8_t, crypto_box_SECRETKEYBYTES> gs_secret{};
    std::array<std::uint8_t, crypto_box_PUBLICKEYBYTES> drone_public{};
    std::array<std::uint8_t, crypto_box_SECRETKEYBYTES> drone_secret{};
    const auto key_path = write_ground_key(gs_public, gs_secret, drone_public, drone_secret);

    const int receiver = socket(AF_INET, SOCK_DGRAM, 0);
    require(receiver >= 0, "opening UDP receiver failed");
    timeval timeout{.tv_sec = 1, .tv_usec = 0};
    require(setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0,
            "setting UDP timeout failed");
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    require(bind(receiver, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
            "binding UDP receiver failed");
    socklen_t address_size = sizeof(address);
    require(getsockname(receiver, reinterpret_cast<sockaddr*>(&address), &address_size) == 0,
            "reading UDP receiver address failed");

    AggregatorUDPv4 aggregator("127.0.0.1", ntohs(address.sin_port), key_path.string(), 0, 0, 0);
    std::array<std::uint8_t, crypto_aead_chacha20poly1305_KEYBYTES> session_key{};
    randombytes_buf(session_key.data(), session_key.size());
    constexpr std::uint8_t k = 2;
    constexpr std::uint8_t n = 3;
    const std::string first_payload = "fpv4mac-primary-0";
    const std::string recovered_payload = "fpv4mac-recovered-1";
    const auto plaintext_size = sizeof(wpacket_hdr_t) +
                                std::max(first_payload.size(), recovered_payload.size());
    const auto allocation_size = ZFEX_ROUND_UP_SIMD(plaintext_size);

    std::array<std::uint8_t*, n> blocks{};
    for (auto& block : blocks) {
        require(posix_memalign(reinterpret_cast<void**>(&block), ZFEX_SIMD_ALIGNMENT,
                               allocation_size) == 0,
                "allocating aligned FEC block failed");
        std::memset(block, 0, allocation_size);
    }
    fill_primary(blocks[0], allocation_size, first_payload);
    fill_primary(blocks[1], allocation_size, recovered_payload);

    fec_t* fec{};
    require(fec_new(k, n, &fec) == ZFEX_SC_OK && fec != nullptr,
            "creating FEC encoder failed");
    const std::array<const std::uint8_t*, k> primary_blocks{blocks[0], blocks[1]};
    const std::array<std::uint8_t*, n - k> parity_blocks{blocks[2]};
    require(fec_encode_simd(fec, primary_blocks.data(), parity_blocks.data(), allocation_size) ==
                ZFEX_SC_OK,
            "encoding parity fragment failed");
    require(fec_free(fec) == ZFEX_SC_OK, "freeing FEC encoder failed");

    auto session = make_session(gs_public, drone_secret, session_key, k, n);
    auto primary_zero = encrypt_fragment(session_key, blocks[0],
                                         sizeof(wpacket_hdr_t) + first_payload.size(), 0);
    auto parity_two = encrypt_fragment(session_key, blocks[2], plaintext_size, 2);
    std::array<std::uint8_t, RX_ANT_MAX> antennas{0, 1, 0xff, 0xff};
    std::array<std::int8_t, RX_ANT_MAX> rssi{-40, -42, -128, -128};
    std::array<std::int8_t, RX_ANT_MAX> noise{-90, -90, 127, 127};
    aggregator.process_packet(session.data(), session.size(), 0, antennas.data(), rssi.data(),
                              noise.data(), 5805, 0, 20, nullptr);
    aggregator.process_packet(primary_zero.data(), primary_zero.size(), 0, antennas.data(),
                              rssi.data(), noise.data(), 5805, 0, 20, nullptr);
    // Deliberately omit primary fragment 1 and deliver parity fragment 2. The receiver must
    // reconstruct fragment 1 before forwarding its payload.
    aggregator.process_packet(parity_two.data(), parity_two.size(), 0, antennas.data(), rssi.data(),
                              noise.data(), 5805, 0, 20, nullptr);

    require(receive_datagram(receiver) == first_payload, "primary UDP payload did not match");
    require(receive_datagram(receiver) == recovered_payload,
            "FEC-recovered UDP payload did not match");
    require(aggregator.count_p_fec_recovered == 1, "receiver did not report one FEC recovery");
    require(aggregator.count_p_outgoing == 2, "receiver did not forward both primary packets");
    for (auto* block : blocks) {
        std::free(block);
    }
    close(receiver);
    std::filesystem::remove(key_path);
    return 0;
}
