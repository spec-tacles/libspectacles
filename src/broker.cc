#include <string>
#include <thread>

#include <amqp.h>
#include <amqp_tcp_socket.h>

#include <set>
#include <vector>

#include "../include/broker.h"
#include "../include/gateway.h"
#include "../include/utils.h"

#include "../include/etf/etf.h"

namespace spectacles {

namespace brokers {

Error Publisher::connect(std::string hostname, int port, std::string g, std::set<std::string> s) {
  Error e;
  group = g;
  events = s;

  amqp_socket_t *socket = nullptr;
  amqp_rpc_reply_t reply;

  conn = amqp_new_connection();

  socket = amqp_tcp_socket_new(conn);
  if (!socket) {
    e.type = BROKER_TCP_SOCKET_ERROR;
    return e;
  }

  int status = amqp_socket_open(socket, hostname.c_str(), port);
  if (status) {
    e.context = "opening TCP socket";
    e.type = BROKER_AMQP_STATUS_ERROR;
    e.amqpStatus = static_cast<amqp_status_enum>(status);
    return e;
  }

  reply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
  if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
    e.context = "Logging in";
    e.type = BROKER_RPC_ERROR;
    e.reply = reply;
    return e;
  }

  amqp_channel_open(conn, 1);
  reply = amqp_get_rpc_reply(conn);
  if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
    e.context = "Opening channel";
    e.type = BROKER_RPC_ERROR;
    e.reply = reply;
    return e;
  }

  amqp_exchange_declare(conn, 1, amqp_cstring_bytes(group.c_str()), amqp_cstring_bytes("direct"), 0, 1, 0, 0, amqp_empty_table);
  reply = amqp_get_rpc_reply(conn);
  if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
    e.context = "Declaring exchange";
    e.type = BROKER_RPC_ERROR;
    e.reply = reply;
    return e;
  }

  return e;
}

Publisher::~Publisher() {
  amqp_rpc_reply_t reply;

  reply = amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
  if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
    return;
  }

  reply = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
  if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
    return;
  }

  amqp_destroy_connection(conn);
}

Error Publisher::publish(gateway::Packet p) {
  Error err;
  if (p.op != 0) {
    return err;
  }

  if (events.size() != 0 && events.count(p.t) == 0) {
    return err;
  }

  amqp_bytes_t message_bytes;

  etf::Encoder e;
  e.pack(p.d);

  etf::out_buf buf = e.release();

  message_bytes.len = buf.length;
  message_bytes.bytes = buf.buf;


  int status = amqp_basic_publish(conn, 1, amqp_cstring_bytes(group.c_str()), amqp_cstring_bytes(p.t.c_str()), 0, 0, nullptr, message_bytes);
  if (status != AMQP_STATUS_OK) {
    err.context = "Publishing";
    err.type = BROKER_AMQP_STATUS_ERROR;
    err.amqpStatus = static_cast<amqp_status_enum>(status);
  }

  return err;
}

Error Consumer::connect(std::string hostname, int port, std::string group, std::vector<std::string> events) {
  Error e;
  std::thread([hostname, port, group, events, this]() -> void {
    Error e;

    amqp_socket_t *socket = nullptr;
    amqp_connection_state_t conn;
    amqp_bytes_t queuename;
    amqp_rpc_reply_t reply;

    conn = amqp_new_connection();

    socket = amqp_tcp_socket_new(conn);
    if (!socket) {
      e.context = "creating TCP socket";
      e.type = BROKER_TCP_SOCKET_ERROR;

      if (errorHandler) {
        errorHandler(e);
      }

      return;
    }

    int status = amqp_socket_open(socket, hostname.c_str(), port);
    if (status) {
      e.context = "opening TCP socket";
      e.type = BROKER_AMQP_STATUS_ERROR;
      e.amqpStatus = static_cast<amqp_status_enum>(status);

      if (errorHandler) {
        errorHandler(e);
      }

      return;
    }

    reply = amqp_login(conn, "/", 0, 131072, 0, AMQP_SASL_METHOD_PLAIN, "guest", "guest");
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
      e.context = "Logging in";
      e.type = BROKER_RPC_ERROR;
      e.reply = reply;

      if (errorHandler) {
        errorHandler(e);
      }

      return;
    }

    amqp_channel_open(conn, 1);
    reply = amqp_get_rpc_reply(conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
      e.context = "Opening channel";
      e.type = BROKER_RPC_ERROR;
      e.reply = reply;

      if (errorHandler) {
        errorHandler(e);
      }

      return;
    }

    amqp_exchange_declare(conn, 1, amqp_cstring_bytes(group.c_str()),
                          amqp_cstring_bytes("direct"), 0, 1, 0, 0,
                          amqp_empty_table);
    reply = amqp_get_rpc_reply(conn);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
      e.context = "Declaring exchange";
      e.type = BROKER_RPC_ERROR;
      e.reply = reply;

      if (errorHandler) {
        errorHandler(e);
      }

      return;
    }

    {
      amqp_queue_declare_ok_t *r = amqp_queue_declare(conn, 1, amqp_empty_bytes, 0, 0, 0, 1, amqp_empty_table);
      reply = amqp_get_rpc_reply(conn);
      if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        e.context = "Declaring queue";
        e.type = BROKER_RPC_ERROR;
        e.reply = reply;

        if (errorHandler) {
          errorHandler(e);
        }

        return;
      }

      queuename = amqp_bytes_malloc_dup(r->queue);
      if (queuename.bytes == nullptr) {
        fprintf(stderr, "Out of memory while copying queue name");
        return;
      }
    }

    for (size_t i = 0; i < events.size(); ++i) {
      amqp_queue_bind(conn, 1, queuename, amqp_cstring_bytes(group.c_str()), amqp_cstring_bytes(events[i].c_str()), amqp_empty_table);
      reply = amqp_get_rpc_reply(conn);
      if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        e.context = "Binding queue";
        e.type = BROKER_RPC_ERROR;
        e.reply = reply;

        if (errorHandler) {
          errorHandler(e);
        }

        return;
      }

      amqp_basic_consume(conn, 1, queuename, amqp_empty_bytes, 0, 1, 0, amqp_empty_table);
      reply = amqp_get_rpc_reply(conn);
      if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        e.context = "Consuming";
        e.type = BROKER_RPC_ERROR;
        e.reply = reply;

        if (errorHandler) {
          errorHandler(e);
        }

        return;
      }
    }

    amqp_frame_t frame;

    while (open) {
      amqp_envelope_t envelope;

      amqp_maybe_release_buffers(conn);
      amqp_rpc_reply_t ret = amqp_consume_message(conn, &envelope, nullptr, 0);

      if (AMQP_RESPONSE_NORMAL != ret.reply_type) {
        if (AMQP_RESPONSE_LIBRARY_EXCEPTION == ret.reply_type &&
            AMQP_STATUS_UNEXPECTED_STATE == ret.library_error) {
          if (AMQP_STATUS_OK != amqp_simple_wait_frame(conn, &frame)) {
            break;
          }

          if (AMQP_FRAME_METHOD == frame.frame_type) {
            if (frame.payload.method.id == AMQP_BASIC_ACK_METHOD) {
              /* if we've turned publisher confirms on, and we've published a
                * message here is a message being confirmed.
                */
            } else if (frame.payload.method.id == AMQP_BASIC_RETURN_METHOD) {
              /* if a published message couldn't be routed and the mandatory
                * flag was set this is what would be returned. The message then
                * needs to be read.
                */
              amqp_message_t message;
              ret = amqp_read_message(conn, frame.channel, &message, 0);
              if (AMQP_RESPONSE_NORMAL != ret.reply_type) {
                break;
              }

              handleMessage(envelope.routing_key, message);

              amqp_destroy_message(&message);
            } else if (frame.payload.method.id == AMQP_CHANNEL_CLOSE_METHOD) {
              /* a channel.close method happens when a channel exception occurs,
                * this can happen by publishing to an exchange that doesn't exist
                * for example.
                *
                * In this case you would need to open another channel redeclare
                * any queues that were declared auto-delete, and restart any
                * consumers that were attached to the previous channel.
                */
              break;
            } else if (frame.payload.method.id == AMQP_CONNECTION_CLOSE_METHOD) {
              /* a connection.close method happens when a connection exception
                * occurs, this can happen by trying to use a channel that isn't
                * open for example.
                *
                * In this case the whole connection must be restarted.
                */
              break;
            } else {
              fprintf(stderr, "An unexpected method was received %u\n",
                      frame.payload.method.id);
              break;
            }
          }
        }

      } else {
        handleMessage(envelope.routing_key, envelope.message);
      }
    }

    reply = amqp_channel_close(conn, 1, AMQP_REPLY_SUCCESS);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
      e.context = "Closing channel";
      e.type = BROKER_RPC_ERROR;
      e.reply = reply;

      if (errorHandler) {
        errorHandler(e);
      }

      return;
    }

    reply = amqp_connection_close(conn, AMQP_REPLY_SUCCESS);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
      e.context = "Closing connection";
      e.type = BROKER_RPC_ERROR;
      e.reply = reply;

      if (errorHandler) {
        errorHandler(e);
      }

      return;
    }

    status = amqp_destroy_connection(conn);
    if (status) {
      e.context = "Ending connection";
      e.type = BROKER_AMQP_STATUS_ERROR;
      e.amqpStatus = static_cast<amqp_status_enum>(status);

      if (errorHandler) {
        errorHandler(e);
      }

      return;
    }

    return;
  }).detach();

  return e;
}

Consumer::~Consumer() {
  open = false;
}

void Consumer::onMessage(std::function<void(std::string, gateway::Packet)> handler) {
  messageHandler = handler;
}

void Consumer::onError(std::function<void(Error)> handler) {
  errorHandler = handler;
}

void Consumer::handleMessage(amqp_bytes_t routing_key, amqp_message_t message) {
  if (messageHandler) {
    etf::Decoder decoder(static_cast<uint8_t *>(message.body.bytes), message.body.len);
    etf::Data d = decoder.unpack();

    gateway::Packet p;
    p.op = d["op"];
    p.d = d["d"];
    p.length = message.body.len;

    p.raw = static_cast<char *>(malloc(message.body.len * sizeof(char)));
    memcpy(p.raw, message.body.bytes, message.body.len);

    int op = d["op"];
    if (op == 0) {
      std::string t = d["t"];
      p.t = t;
      p.s = d["s"];
    }

    messageHandler(std::string(static_cast<char *>(routing_key.bytes), routing_key.len), p);
  }
}

}  // namespace brokers

}  // namespace spectacles
