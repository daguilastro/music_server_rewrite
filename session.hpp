#pragma once
#include "command.hpp"
#include <array>
#include <boost/asio.hpp>
#include <cstring>
#include <memory>
#include <print>
#include <vector>

using boost::asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
  tcp::socket socket_; // Público para envío de datos desde comandos

  Session(tcp::socket socket, CommandDispatcher &dispatcher)
      : socket_(std::move(socket)), dispatcher_(dispatcher) {}

  void start() { read_header(); }

private:
  CommandDispatcher &dispatcher_;

  // Estado de lectura
  std::array<char, 5> header_; // 1 byte tipo + 4 bytes longitud
  std::vector<uint8_t> payload_;
  uint32_t expected_payload_size_ = 0;

  static uint32_t read_u32_be(const char *data) {
    return (static_cast<uint32_t>(static_cast<uint8_t>(data[0])) << 24) |
           (static_cast<uint32_t>(static_cast<uint8_t>(data[1])) << 16) |
           (static_cast<uint32_t>(static_cast<uint8_t>(data[2])) << 8) |
           static_cast<uint32_t>(static_cast<uint8_t>(data[3]));
  }

  static uint16_t read_u16_be(const uint8_t *data) {
    return (static_cast<uint16_t>(data[0]) << 8) |
           static_cast<uint16_t>(data[1]);
  }

  bool parse_payload_into_args(Command &cmd) {
    size_t offset = 0;
    cmd.args.clear();

    while (offset < payload_.size()) {
      // Cada argumento empieza con 2 bytes de longitud
      if (payload_.size() - offset < 2) {
        return false;
      }

      const uint16_t arg_size = read_u16_be(payload_.data() + offset);
      offset += 2;

      if (payload_.size() - offset < arg_size) {
        return false;
      }

      Argument arg;
      arg.bytes.assign(payload_.begin() + offset,
                       payload_.begin() + offset + arg_size);
      cmd.args.push_back(std::move(arg));
      offset += arg_size;
    }

    return true;
  }

  void read_header() {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_, boost::asio::buffer(header_),
        [this, self](const boost::system::error_code &ec, std::size_t) {
          if (ec) {
            return;
          }

          const uint8_t type = static_cast<uint8_t>(header_[0]);
          expected_payload_size_ = read_u32_be(header_.data() + 1);

          if (expected_payload_size_ == 0) {
            Command cmd{.type = type, .total_length = 0, .args = {}};
            dispatcher_.execute(cmd, this);
            read_header();
            return;
          }

          payload_.assign(expected_payload_size_, 0);
          read_payload(type);
        });
  }

  void read_payload(uint8_t type) {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_, boost::asio::buffer(payload_),
        [this, self, type](const boost::system::error_code &ec, std::size_t) {
          if (ec) {
            return;
          }

          Command cmd{
              .type = type, .total_length = expected_payload_size_, .args = {}};

          if (!parse_payload_into_args(cmd)) {
            std::print("Error: payload mal formado para comando tipo {}\n",
                       type);
            read_header();
            return;
          }

          dispatcher_.execute(cmd, this);
          read_header();
        });
  }
};