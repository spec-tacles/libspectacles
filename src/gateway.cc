#include <chrono>
#include <thread>

#include <cstring>
#include <cstdlib>

#include <uWS/uWS.h>

#include "../include/gateway.h"

#include "../include/etf/etf.h"

namespace spectacles {

namespace gateway {

void Connection::onError(std::function<void()> handler) {
  errorHandler = handler;
}

void Connection::onConnection(std::function<void()> handler) {
  connectionHandler = handler;
}

void Connection::onDisconnection(std::function<void(int, std::string)> handler) {
  disconnectionHandler = handler;
}

void Connection::onMessage(std::function<void(Packet)> handler) {
  messageHandler = handler;
}

void Connection::send(char *data, size_t length) {
  ws->send(data, length, uWS::OpCode::BINARY);
}

void Connection::send(etf::Data d) {
  etf::Encoder encoder;
  encoder.pack(d);

  etf::out_buf buf = encoder.release();

  send(buf.buf, buf.length);
}

void Connection::send(Packet p) {
  send(p.raw, p.length);
}

void Connection::identify() {
  seq = -1;
  session = "";

  etf::Data identify = etf::Data::Object();
  identify["op"] = 2;
  identify["d"] = etf::Data::Object();
  identify["d"]["token"] = options.token;
  identify["d"]["compress"] = true;
  identify["d"]["large_threshold"] = options.large_threshold;
  identify["d"]["properties"] = etf::Data::Object();
  identify["d"]["properties"]["$os"] = "linux";
  identify["d"]["properties"]["$browser"] = "spectacles";
  identify["d"]["properties"]["$device"] = "spectacles";
  identify["d"]["presence"] = etf::Data::Object();
  identify["d"]["shard"] = etf::Data::Array(2);
  identify["d"]["shard"][0] = options.shard_id;
  identify["d"]["shard"][1] = options.shard_count;
  identify["d"]["presence"] = options.presence;

  send(identify);
}

void Connection::resume() {
  etf::Data resume = etf::Data::Object();
  resume["op"] = 6;
  resume["d"] = etf::Data::Object();
  resume["d"]["token"] = options.token;
  resume["d"]["session_id"] = session;
  resume["d"]["seq"] = seq;

  send(resume);
}

void Connection::heartbeat() {
  etf::Data heartbeat = etf::Data::Object();
  heartbeat["op"] = 1;

  if (seq == -1) {
    heartbeat["d"] = nullptr;
  } else {
    heartbeat["d"] = seq;
  }

  send(heartbeat);
}

void Connection::connect(Options options) {
  uWS::Hub hub;

  this->options = options;

  hub.onError([this](void *user) {
    open = false;

    if (errorHandler) {
      errorHandler();
      reconnect();
    }
  });

  hub.onConnection([this](uWS::WebSocket<uWS::CLIENT> *ws, uWS::HttpRequest req) {
    this->ws = ws;
    open = true;

    if (connectionHandler) {
      connectionHandler();
    }
  });

  hub.onDisconnection([this](uWS::WebSocket<uWS::CLIENT> *ws, int code, char *message, size_t length) {
    open = false;

    if (disconnectionHandler) {
      disconnectionHandler(code, std::string(message, length));
    }

    if (tries == 5) {
      destroy();
    } else {
      switch (code) {
        case 4004:
        case 4010:
        case 4011:
          destroy();
          return;

        case 4003:
        case 4007:
        case 4009:
          seq = -1;
          session = "";

        default:
          reconnect();
          break;
      }
    }
  });

  hub.onMessage([this, options](uWS::WebSocket<uWS::CLIENT> *ws, char *raw, size_t length, uWS::OpCode opCode) {
    etf::Decoder decoder(reinterpret_cast<uint8_t *>(raw), length);
    etf::Data d = decoder.unpack();

    int op = d["op"];

    if (op == 10) {
      int heartbeatInterval = d["d"]["heartbeat_interval"];

      if (session.size() > 0) {
        resume();
      } else {
        identify();
      }

      acked = true;

      if (!heartbeatStarted) {
        heartbeatStarted = true;
        std::thread([ws, this, heartbeatInterval]() {
          while (heartbeatOpen) {
            std::this_thread::sleep_for(std::chrono::milliseconds(heartbeatInterval));

            if (!acked) {
              reconnect(4009);
              std::terminate();
            }

            if (open) {
              acked = false;
              heartbeat();
            }
          }
        }).detach();
      } else {
        heartbeat();
      }
    } else if (op == 0) {
      seq = d["s"];

      std::string t = d["t"];
      if (t == "READY") {
        std::string session = d["d"]["session_id"];
        this->session = session;
        tries = 0;
      }
    } else if (op == 11) {
      acked = true;
    } else if (op == 7) {
      reconnect();
    } else if (op == 9) {
      bool d = d["d"];
      if (d) {
        resume();
      } else {
        reconnect();
      }
    } else if (op == 1) {
      heartbeat();
    }

    if (messageHandler) {
      Packet p;
      p.op = d["op"];
      p.d = d["d"];
      p.length = length;

      p.raw = static_cast<char *>(malloc(length * sizeof(char)));
      memcpy(p.raw, raw, length);

      if (op == 0) {
        std::string t = d["t"];
        p.t = t;
        p.s = d["s"];
      }

      messageHandler(p);
    }
  });

  hub.connect("wss://gateway.discord.gg/?v=6&encoding=etf");

  hub.run();
}

void Connection::disconnect(int code) {
  if (open) {
    ws->close(code);
  }
}

void Connection::reconnect(int code) {
  disconnect(code);
  std::this_thread::sleep_for(std::chrono::milliseconds(5500 + (rand() % 10 + 1)));
  connect(options);
}

void Connection::destroy() {
  disconnect();
  heartbeatOpen = false;
}

}  // namespace gateway

}  // namespace spectacles
