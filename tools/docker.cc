#include <iostream>
#include "../include/spectacles.h"

#include <cstdlib>
#include <chrono>
#include <set>
#include <string>
#include <sstream>
#include <thread>
#include <vector>

using namespace spectacles;

int main() {
  std::set<std::string> publisherEvents;

  std::stringstream ss(std::getenv("EVENTS"));

  while (ss.good()) {
    std::string substr;
    std::getline(ss, substr, ',');
    publisherEvents.insert(substr);
  }

  if (std::getenv("SHARDS")) {
    int shardCount = atoi(std::getenv("SHARDS"));
    std::vector<std::string> consumerEvents(shardCount);
    std::vector<gateway::Connection> shards(shardCount);

    for (int i = 0; i < shardCount; i++) {
      consumerEvents.push_back(std::to_string(i));
      std::thread([i, shardCount, &shards, &publisherEvents]() {
        gateway::Connection conn;
        shards[i] = conn;

        brokers::Publisher publisher;
        publisher.connect(std::getenv("HOST"), atoi(std::getenv("PORT")), std::getenv("PUBLISHER_GROUP"), publisherEvents);

        while (true) {
          brokers::Error e = publisher.connect(std::getenv("HOST"), atoi(std::getenv("PORT")), std::getenv("PUBLISHER_GROUP"), publisherEvents);
          if (e.type == brokers::BROKER_OK) {
            break;
          } else if (e.type == brokers::BROKER_AMQP_STATUS_ERROR && e.amqpStatus == AMQP_STATUS_SOCKET_ERROR ) {
            std::cout << "[SHARD: " << i << "] Failed to connect to TCP socket, retrying in 5 seconds..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
          } else {
            std::cerr << "[SHARD: " << i << "] Unexpected publisher connect error " << e.type << " while " << e.context << std::endl;
            exit(1);
          }
        }

        conn.onError([i]() {
          std::cout << "[SHARD: " << i << "] Error" << std::endl;
        });

        conn.onConnection([&conn, i]() {
          std::cout << "[SHARD: " << i << "] Connection" << std::endl;
        });

        conn.onDisconnection([&conn, i](int code, std::string message) {
          std::cout << "[SHARD: " << i << "] Disconnected: " << code << std::endl;
        });

        conn.onMessage([&publisher](gateway::Packet p) {
          publisher.publish(p);
        });

        gateway::Options opt;
        opt.token = std::getenv("TOKEN");
        opt.shard_id = i;
        opt.shard_count = shardCount;

        conn.connect(opt);
      }).detach();

      std::this_thread::sleep_for(std::chrono::seconds(6));
    }

    brokers::Consumer consumer;

    consumer.onError([&consumer, consumerEvents](brokers::Error e) {
      if (e.type == brokers::BROKER_OK) {
        return;
      } else if (e.type == brokers::BROKER_AMQP_STATUS_ERROR && e.amqpStatus == AMQP_STATUS_SOCKET_ERROR ) {
        std::cout << "Consumer failed to connect to TCP socket, retrying in 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        consumer.connect(std::getenv("HOST"), atoi(std::getenv("PORT")), std::getenv("CONSUMER_GROUP"), consumerEvents);
      } else {
        std::cerr << "Unexpected consumer error " << e.type << " while " << e.context << std::endl;
        exit(1);
      }
    });

    consumer.onMessage([&shards](std::string event, gateway::Packet p) {
      shards[atoi(event.c_str())].send(p.raw, p.length);
    });

    consumer.connect(std::getenv("HOST"), atoi(std::getenv("PORT")), std::getenv("CONSUMER_GROUP"), consumerEvents);

    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(10));
    }
  } else {
    gateway::Connection conn;

    brokers::Publisher publisher;

    while (true) {
      brokers::Error e = publisher.connect(std::getenv("HOST"), atoi(std::getenv("PORT")), std::getenv("PUBLISHER_GROUP"), publisherEvents);
      if (e.type == brokers::BROKER_OK) {
        break;
      } else if (e.type == brokers::BROKER_AMQP_STATUS_ERROR && e.amqpStatus == AMQP_STATUS_SOCKET_ERROR ) {
        std::cout << "Failed to connect to TCP socket, retrying in 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
      } else {
        std::cerr << "Unexpected publisher connect error " << e.type << " while " << e.context << std::endl;
        exit(1);
      }
    }

    std::vector<std::string> consumerEvents = {std::getenv("SHARD_ID")};
    brokers::Consumer consumer;

    consumer.onError([](brokers::Error e) {
      std::cerr << "Unexpected consumer error " << e.type << " while " << e.context << std::endl;
      exit(1);
    });

    consumer.onMessage([&conn](std::string event, gateway::Packet p) {
      conn.send(p);
    });

    consumer.connect(std::getenv("HOST"), atoi(std::getenv("PORT")), std::getenv("CONSUMER_GROUP"), consumerEvents);

    conn.onError([]() {
      std::cout << "Error" << std::endl;
    });

    conn.onConnection([&conn]() {
      std::cout << "Connection" << std::endl;
    });

    conn.onDisconnection([&conn](int code, std::string message) {
      std::cout << "Disconnected: " << code << std::endl;
    });

    conn.onMessage([&publisher](gateway::Packet p) {
      publisher.publish(p);
    });

    gateway::Options opt;
    opt.token = std::getenv("TOKEN");
    opt.shard_id = atoi(std::getenv("SHARD_ID"));
    opt.shard_count = atoi(std::getenv("SHARD_COUNT"));

    conn.connect(opt);
  }
}
