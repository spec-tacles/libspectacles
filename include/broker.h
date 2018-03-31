#ifndef SPECTACLES_INCLUDE_BROKER_H_
#define SPECTACLES_INCLUDE_BROKER_H_

#include <string>
#include <set>
#include <vector>

#include <amqp.h>
#include <amqp_tcp_socket.h>

#include "gateway.h"

/// \brief The spectacles namespace.
///
/// Everything public facing is under this namespace.
namespace spectacles {

/// \brief The brokers namespace.
///
/// Everything broker related is under this namespace.
namespace brokers {

/// The type of error.
enum error_type {
  BROKER_OK,
  BROKER_AMQP_STATUS_ERROR,
  BROKER_RPC_ERROR,
  BROKER_TCP_SOCKET_ERROR,
};

/// A broker error.
struct Error {
  error_type type = BROKER_OK;
  amqp_status_enum amqpStatus = AMQP_STATUS_OK;
  amqp_rpc_reply_t reply;
  std::string context;
};

/// A connection used solely to publish messages.
class Publisher {
 private:
  amqp_connection_state_t conn;
  std::string group;
  std::set<std::string> events;

 public:
  /// Creates a new publisher.
  Publisher() { }

  /// Closes the connection.
  ~Publisher();

  /// \brief Connects to the broker.
  ///
  /// \param[in] hostname - The server hostname.
  /// \param[in] port     - The server port.
  /// \param[in] exchange - The exchange to publish to.
  /// \param[in] events   - The events to send out. Any events published to this that are not in this set will be discarded. If left as an empty set, the publisher will let everything through.
  /// \returns 0 if successful.
  Error connect(std::string hostname, int port, std::string exchange = "direct", std::set<std::string> events = std::set<std::string>());

  /// \brief Publishes a message.
  ///
  /// \param[in] packet - The packet to send.
  /// \returns 0 if successful.
  Error publish(gateway::Packet packet);
};

/// \brief A connection used solely to consume messages.
///
/// \note The consumer spawns it's own thread.
class Consumer {
 private:
  bool open = true;
  std::function<void(Error)> errorHandler;
  std::function<void(std::string, gateway::Packet)> messageHandler;

  void handleMessage(amqp_bytes_t, amqp_message_t);

 public:
  /// Creates a new consumer.
  Consumer() { }

  /// Stops the consumer thread and closes the connection.
  ~Consumer();

  /// \brief Connects to the broker.
  ///
  /// \param[in] hostname - The server hostname.
  /// \param[in] port     - The server port.
  /// \param[in] exchange - The exchange to publish to.
  /// \param[in] events   - The events to subscribe to.
  /// \returns 0 if successful.
  Error connect(std::string hostname, int port, std::string exchange = "direct", std::vector<std::string> events = std::vector<std::string>(0));

  /// \brief Called when a new message is received.
  ///
  /// \param[in] handler - The event handler.
  void onMessage(std::function<void(std::string, gateway::Packet)> handler);

  /// \brief Called when there is an error;
  //
  /// \param[in] handler - The event handler.
  void onError(std::function<void(Error)> handler);
};

}  // namespace brokers

}  // namespace spectacles

#endif  // SPECTACLES_INCLUDE_BROKER_H_
