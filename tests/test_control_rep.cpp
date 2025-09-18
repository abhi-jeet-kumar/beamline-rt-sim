#include "../src/ipc/control_rep.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>
#include <zmq.h>

/**
 * @brief Test ControlRep functionality
 * 
 * Tests the REQ/REP pattern by creating a test client that sends
 * commands and verifies responses are received correctly.
 */
int main() {
    std::cout << "Testing ControlRep functionality..." << std::endl;
    
    try {
        // Test basic construction and destruction
        {
            ControlRep rep;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::cout << "  ControlRep created and destroyed successfully" << std::endl;
        }
        
        // Test request/response in separate thread
        std::cout << "Testing request/response pattern..." << std::endl;
        
        bool server_ready = false;
        bool test_passed = false;
        
        // Server thread
        std::thread server_thread([&]() {
            try {
                ControlRep rep;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                server_ready = true;
                
                // Receive first command with timeout
                void* ctx = zmq_ctx_new();
                void* monitor = zmq_socket(ctx, ZMQ_PAIR);
                
                // Wait for command (with timeout)
                zmq_pollitem_t items[] = {{rep.rep, 0, ZMQ_POLLIN, 0}};
                int rc = zmq_poll(items, 1, 5000); // 5 second timeout
                
                if (rc > 0 && (items[0].revents & ZMQ_POLLIN)) {
                    std::string cmd = rep.recv();
                    std::cout << "  Server received: " << cmd << std::endl;
                    
                    // Send response
                    rep.reply(R"({"ok": true})");
                    std::cout << "  Server sent response" << std::endl;
                    test_passed = true;
                } else {
                    std::cout << "  Server timeout waiting for command" << std::endl;
                }
                
                zmq_close(monitor);
                zmq_ctx_term(ctx);
                
            } catch (const std::exception& e) {
                std::cerr << "Server exception: " << e.what() << std::endl;
            }
        });
        
        // Wait for server to be ready
        while (!server_ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Client thread
        std::thread client_thread([&]() {
            try {
                // Create ZeroMQ client
                void* ctx = zmq_ctx_new();
                void* req = zmq_socket(ctx, ZMQ_REQ);
                int rc = zmq_connect(req, "tcp://127.0.0.1:5555");
                
                if (rc == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    
                    // Send test command
                    std::string cmd = R"({"cmd":"set_pid","kp":0.5,"ki":0.1,"kd":0.0})";
                    zmq_send(req, cmd.data(), cmd.size(), 0);
                    std::cout << "  Client sent: " << cmd << std::endl;
                    
                    // Receive response
                    char buf[1024];
                    int n = zmq_recv(req, buf, sizeof(buf), 0);
                    if (n > 0) {
                        std::string response(buf, buf + n);
                        std::cout << "  Client received: " << response << std::endl;
                        
                        // Verify response
                        if (response.find("\"ok\"") != std::string::npos) {
                            std::cout << "  Response verification passed" << std::endl;
                        }
                    }
                }
                
                zmq_close(req);
                zmq_ctx_term(ctx);
                
            } catch (const std::exception& e) {
                std::cerr << "Client exception: " << e.what() << std::endl;
            }
        });
        
        // Wait for both threads to complete
        server_thread.join();
        client_thread.join();
        
        if (test_passed) {
            std::cout << "  Request/response test passed" << std::endl;
        } else {
            std::cout << "  Request/response test failed" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception occurred" << std::endl;
        return 1;
    }
    
    std::cout << "✅ ControlRep tests completed!" << std::endl;
    return 0;
}
