#pragma once
#include <zmq.h>
#include <string>
#include <cstdio>

/**
 * @brief ZeroMQ telemetry publisher
 * 
 * Publishes telemetry data as JSON messages on "telemetry" topic
 * as specified in technical_details.txt
 * 
 * Message format:
 * {"t": <sec>, "pos": <double>, "intensity": <double>, "mag": <double>, "deadline_miss": <0|1>}
 */
struct TelemetryPub {
  void* ctx{nullptr};  ///< ZeroMQ context
  void* pub{nullptr};  ///< ZeroMQ PUB socket
  
  /**
   * @brief Constructor - creates and binds publisher socket
   */
  TelemetryPub() {
    ctx = zmq_ctx_new();
    pub = zmq_socket(ctx, ZMQ_PUB);
    int rc = zmq_bind(pub, "tcp://127.0.0.1:5556");
    (void)rc;
  }
  
  /**
   * @brief Destructor - cleanup ZeroMQ resources
   */
  ~TelemetryPub() { 
    zmq_close(pub); 
    zmq_ctx_term(ctx); 
  }
  
  /**
   * @brief Send telemetry message
   * @param s JSON string to send
   */
  void send(const std::string& s) {
    zmq_send(pub, "telemetry", 9, ZMQ_SNDMORE);
    zmq_send(pub, s.data(), s.size(), 0);
  }
};