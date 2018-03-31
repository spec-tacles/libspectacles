#ifndef SPECTACLES_INCLUDE_GATEWAY_H_
#define SPECTACLES_INCLUDE_GATEWAY_H_

#include <uWS/uWS.h>

#include "etf/etf.h"

/// \brief The spectacles namespace.
///
/// Everything public facing is under this namespace.
namespace spectacles {

/// \brief The gateway namespace.
///
/// Everything gateway related is under this namespace.
namespace gateway {

/// %Options used when connecting to the Discord gateway.
struct Options {
  /// The token used to authenticate with the Discord gateway.
  std::string token;

  /// [Presence](https://discordapp.com/developers/docs/topics/gateway#update-status) structure for initial presence information.
  etf::Data presence = nullptr;

  /// The shard ID.
  int shard_id = 0;

  /// The total number of shards.
  int shard_count = 1;

  /// Value between 50 and 250, total number of members where the gateway will stop sending offline members in the guild member list.
  int large_threshold = 250;
};

/// A packet coming from a Connection or a brokers::Consumer.
class Packet {
 public:
  /// The payload opcode.
  int op;

  /// \brief The sequence number.
  ///
  /// This is only sent for opcode 0. It will be set to -1 if not sent by discord.
  int s = -1;

  /// \brief The event name.
  ///
  /// This is only sent for opcode 0. It will be set to an empty string if not sent by discord.
  std::string t = "";

  /// The event data.
  etf::Data d;

  /// The raw etf data.
  char *raw;

  /// The size of Packet#raw
  size_t length;

  Packet() { }

  Packet(const Packet &p) {
    op = p.op;
    s = p.s;
    t = p.t;
    d = p.d;
    length = p.length;

    raw = static_cast<char *>(malloc(length * sizeof(char)));
    memcpy(raw, p.raw, p.length);
  }

  Packet &operator=(const Packet &p) {
    op = p.op;
    s = p.s;
    t = p.t;
    d = p.d;
    length = p.length;

    raw = static_cast<char *>(malloc(length * sizeof(char)));
    memcpy(raw, p.raw, p.length);

    return *this;
  }

  /// Frees Packet#raw
  ~Packet() {
    free(raw);
  }
};

/// A connection to the Discord gateway.
class Connection {
 private:
  uWS::WebSocket<uWS::CLIENT> *ws;
  Options options;
  std::string session = "";
  int tries = 0;
  int seq = -1;
  bool open = false;
  bool acked = true;
  bool heartbeatStarted = false;
  bool heartbeatOpen = true;
  std::function<void()> errorHandler;
  std::function<void()> connectionHandler;
  std::function<void(int, std::string)> disconnectionHandler;
  std::function<void(Packet)> messageHandler;

 public:
  /// \brief Called when a WebSocket error occurs.
  //
  /// The client will not automatically reconnect when this is called.
  ///
  /// \param[in] handler - The event handler.
  void onError(std::function<void()> handler);

  /// \brief Called when the WebSocket connects.
  ///
  /// \param[in] handler - The event handler.
  void onConnection(std::function<void()> handler);

  /// \brief Called when the WebSocket disconnects.
  ///
  /// \param[in] handler - The event handler.
  void onDisconnection(std::function<void(int, std::string)> handler);

  /// \brief Called when a message is received from the WebSocket.
  ///
  /// \param[in] handler - The event handler.
  void onMessage(std::function<void(Packet)> handler);

  /// \brief Sends data via the WebSocket.
  //
  /// \param[in] data   - The data to send.
  /// \param[in] length - The length of data.
  void send(char *data, size_t length);

  /// \brief Sends data via the WebSocket.
  //
  /// \param[in] data - The data to send.
  void send(etf::Data data);

  /// \brief Sends data via the WebSocket.
  //
  /// \param[in] packet - The data to send.
  void send(Packet packet);

  /// Sends an identify packet.
  void identify();

  /// Sends a resume packet.
  void resume();

  /// Sends a heartbeat.
  void heartbeat();

  /// \brief Connects to the Discord gateway.
  ///
  /// \param[in] options - The options to use when connecting.
  void connect(Options options);

  /// \brief Closes the WebSocket connection.
  ///
  /// \param[in] code - The close code to disconnect with.
  void disconnect(int code = 1000);

  /// \brief Closes and then reconnects to the Discord gateway.
  ///
  /// \param[in] code - The close code to disconnect with.
  void reconnect(int code = 1000);

  /// \brief Destroys the connection
  ///
  /// This method will also stop the heartbeat thread.
  void destroy();
};

}  // namespace gateway

}  // namespace spectacles

#endif  // SPECTACLES_INCLUDE_GATEWAY_H_
