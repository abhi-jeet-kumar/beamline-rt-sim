#pragma once
#include <zmq.h>
#include <string>

/**
 * @brief ZeroMQ control command responder
 * 
 * Receives control commands as JSON and sends responses
 * as specified in technical_details.txt
 * 
 * Supported commands:
 * - {"cmd":"set_pid","kp":0.2,"ki":0.01,"kd":0.0}
 * - {"cmd":"set_freq","hz":500}
 * - {"cmd":"set_setpoint","sp":0.0}
 * - {"cmd":"recommission"}
 */
struct ControlRep {
  void* ctx{nullptr};  ///< ZeroMQ context
  void* rep{nullptr};  ///< ZeroMQ REP socket
  
  /**
   * @brief Constructor - creates and binds responder socket
   */
  ControlRep() {
    ctx = zmq_ctx_new();
    rep = zmq_socket(ctx, ZMQ_REP);
    int rc = zmq_bind(rep, "tcp://127.0.0.1:5555");
    (void)rc;
  }
  
  /**
   * @brief Destructor - cleanup ZeroMQ resources
   */
  ~ControlRep() { 
    zmq_close(rep); 
    zmq_ctx_term(ctx); 
  }
  
  /**
   * @brief Receive command (blocking)
   * @return Received JSON string. Caller must reply.
   */
  std::string recv() {
    char buf[1024]; 
    int n = zmq_recv(rep, buf, sizeof(buf), 0);
    return std::string(buf, buf + (n > 0 ? n : 0));
  }
  
  /**
   * @brief Send reply to received command
   * @param s Response string (typically JSON)
   */
  void reply(const std::string& s) { 
    zmq_send(rep, s.data(), s.size(), 0); 
  }
};