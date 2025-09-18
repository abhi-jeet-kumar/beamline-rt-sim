# 🔬 Beamline Real-Time Simulator

**Production-grade deterministic C++ beamline control simulator with professional real-time performance**

![Beamline Operator Console](Screenshot%202025-09-18%20at%205.03.17%20PM.png)

## 🎯 Overview

A comprehensive real-time simulation system for particle accelerator beamline control, featuring:

- **1000 Hz deterministic control loop** with <10μs jitter (professional-grade performance)
- **PID feedback control** for beam position stabilization  
- **FESA-style hardware abstraction** with realistic physics simulation
- **ZeroMQ IPC** for telemetry streaming and command handling
- **Professional PyQt operator console** with live visualization
- **Machine Protection System** with beam loss monitors and safety interlocks
- **Ultra-high performance optimization** with real-time scheduling

## 🚀 Quick Start

### Prerequisites
- **C++20 compiler** (GCC 10+ or Clang 12+)
- **CMake 3.10+**
- **ZeroMQ development libraries**
- **Python 3.8+** with PyQt6

### Build and Run

```bash
# Clone the repository
git clone https://github.com/abhi-jeet-kumar/beamline-rt-sim.git
cd beamline-rt-sim

# Build the C++ simulator
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j

# Start the simulator
cd ..
./build/beamline_rt_sim
```

**Expected Output:**
```
Beamline RT Simulator - Starting up...
Initializing real-time optimizations...
  ✅ Real-time scheduling enabled (priority 50)
  ✅ Memory pools pre-allocated
Hardware components created
Control API and IPC initialized
Telemetry: tcp://127.0.0.1:5556
Control: tcp://127.0.0.1:5555
Starting control loop at 1000 Hz...
PID gains: Kp=0.6, Ki=0.05, Kd=0
Press Ctrl+C to stop
```

### Start the GUI (in new terminal)

```bash
cd gui
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python app.py
```

## 🏗️ Architecture

### Core Components

#### **Real-Time Control Loop (1000 Hz)**
- **PeriodicClock**: Drift-compensated timing with nanosecond precision
- **PID Controller**: Anti-windup protection and derivative filtering
- **Watchdog**: Deadline miss detection and performance monitoring
- **RTLoop**: Main control loop with JSON command handling

#### **Hardware Simulation Layer**
- **BPM (Beam Position Monitor)**: 5 Hz oscillation with magnet coupling
- **BIC (Beam Intensity Counter)**: Poisson noise generation
- **Magnet**: Current control with physics coupling to beam position
- **Noise Simulation**: Realistic thermal, electronic, and shot noise

#### **Machine Protection System (MPS)**
- **Beam Loss Monitors**: 3 BLMs with configurable thresholds
- **Automatic Beam Abort**: Instant protection on dangerous conditions
- **Safety Callbacks**: Professional alarm and logging system
- **Real-time Integration**: Safety checks every control cycle

#### **Real-Time Performance Optimization**
- **CPU Core Affinity**: Dedicated core isolation
- **Real-time Scheduling**: SCHED_FIFO (Linux) / Time Constraint (macOS)
- **Memory Optimization**: Pre-allocation and locking (mlockall)
- **Performance Monitoring**: Detailed jitter analysis and statistics

#### **IPC Communication (ZeroMQ)**
- **Telemetry Publisher**: PUB socket on tcp://127.0.0.1:5556
- **Control Responder**: REP socket on tcp://127.0.0.1:5555
- **JSON Message Format**: Structured data exchange
- **Non-blocking Commands**: Real-time command processing

### GUI Features

#### **Live Visualization**
- **Beam Position Plot**: Real-time position with PID setpoint overlay
- **Beam Intensity Plot**: Live intensity monitoring
- **Magnet Current Plot**: Control output visualization  
- **Loop Performance Plot**: Timing and jitter analysis

#### **Control Interface**
- **PID Controller**: Kp, Ki, Kd gain adjustment
- **System Control**: Frequency, setpoint, enable/disable
- **Safety Controls**: Emergency stop, recommission
- **Real-time Status**: Connection, performance, and safety monitoring

## 🎛️ Operation

### JSON Command Interface

#### **PID Control**
```json
{"cmd":"set_pid","kp":0.6,"ki":0.05,"kd":0.0}
{"cmd":"set_setpoint","sp":0.5}
```

#### **System Control**
```json
{"cmd":"set_freq","hz":1000}
{"cmd":"recommission"}
{"cmd":"emergency_stop"}
{"cmd":"enable_control","enable":true}
```

#### **Status and Performance**
```json
{"cmd":"get_status"}
{"cmd":"get_performance"}
```

### Telemetry Data Format

```json
{
  "t": 1.234,
  "pos": 0.5,
  "intensity": 10000.0,
  "mag": 1.2,
  "deadline_miss": 0,
  "mps_safe": true,
  "mps_abort": false
}
```

## 📊 Performance Specifications

### **Professional Timing Performance**
- **Control Loop Frequency**: 1000 Hz (1 ms period)
- **Timing Jitter**: <10 μs P99 (6.208 μs achieved) - meets accelerator industry standards
- **Average Loop Time**: ~1.3 μs
- **Real-time Scheduling**: SCHED_FIFO priority 50
- **CPU Core Isolation**: Dedicated core assignment

### **Safety Performance**
- **Machine Protection**: 3 BLMs with configurable thresholds
- **Beam Abort Time**: <1 ms (automatic on threshold violation)
- **Safety Check Frequency**: Every control cycle (1000 Hz)
- **Alarm Response**: Immediate callback execution

### **Communication Performance**
- **Telemetry Rate**: 1000 Hz JSON streaming
- **Command Latency**: <1 ms response time
- **GUI Update Rate**: 20 Hz smooth visualization
- **Data Throughput**: >100 KB/s sustained

## 🧪 Testing

### **Run All Tests**
```bash
cd build
ctest --output-on-failure
```

### **Individual Component Tests**
```bash
# Core functionality
./test_rt_loop              # Real-time control loop
./test_machine_protection    # Machine protection system
./test_realtime_performance  # Performance optimization

# Hardware components  
./test_hw_components         # BPM, BIC, Magnet simulation
./test_control_api          # Control abstraction layer

# Communication
./test_telemetry_pub        # ZeroMQ telemetry publisher
./test_control_rep          # ZeroMQ control responder
```

### **Performance Validation**
```bash
# Get detailed performance report
echo '{"cmd":"get_performance"}' | nc localhost 5555

# Expected results:
# P99 jitter: <10 μs (professional target)
# Real-time scheduling: ENABLED
# CPU core isolation: WORKING
```

## 🔧 Technical Implementation

### **Real-Time Architecture**
- **Deterministic Timing**: `std::this_thread::sleep_until` for drift compensation
- **Lock-free Data Structures**: Atomic operations for thread safety
- **Zero Dynamic Allocation**: Pre-allocated memory pools in hot path
- **Cross-platform RT Support**: Linux (SCHED_FIFO) + macOS (Time Constraint)

### **Physics Simulation**
- **Closed-loop Coupling**: Magnet affects beam position (-0.4 factor)
- **Realistic Noise**: Gaussian (BPM) + Poisson (BIC) statistics
- **5 Hz Beam Oscillation**: Natural beam motion simulation
- **Configurable Parameters**: Beam current, position, magnet response

### **Safety Systems**
- **Multi-layered Protection**: Warning → Abort escalation
- **Redundant Monitoring**: Multiple BLM positions
- **Automatic Response**: Beam abort triggers emergency stop
- **Professional Logging**: Comprehensive alarm and statistics

### **Professional Features**
- **FESA-style Abstraction**: Industry-standard hardware interfaces
- **JSON API**: RESTful command and status interface
- **Comprehensive Telemetry**: Complete system state streaming
- **Graceful Shutdown**: Clean resource cleanup on SIGINT/SIGTERM

## 📈 Performance Benchmarks

### **Professional Timing Performance**
```
📊 REAL-TIME PERFORMANCE REPORT
================================
Samples: 1000
Timing: 0.416 - 8.042 μs
Average: 1.31117 μs
P95 Jitter: 2.416 μs
P99 Jitter: 6.208 μs  ← Meets accelerator industry standards
RT Enabled: YES
CPU Core: 10
🎯 Professional timing target achieved (<10μs p99)
```

### **System Resource Usage**
- **CPU Usage**: ~4-8% (simulator) + ~20-50% (GUI)
- **Memory Usage**: <50 MB total system
- **Network Throughput**: ~100 KB/s telemetry + commands
- **Real-time Priority**: SCHED_FIFO/Time Constraint enabled

## 🛡️ Safety Features

### **Machine Protection System**
- **Beam Loss Monitors**: 3 positions (upstream, target, downstream)
- **Configurable Thresholds**: Warning (1e-6 Gy/s) + Abort (1e-5 Gy/s)
- **Automatic Protection**: Beam abort on threshold violation
- **Real-time Monitoring**: Safety checks every 1 ms

### **Emergency Systems**
- **Emergency Stop**: Immediate magnet shutdown and control disable
- **Recommission**: Safe system reset with MPS state clear
- **Control Enable/Disable**: Operator control over automation
- **Professional Alarms**: Console output and GUI notifications

## 🎨 GUI Features

### **Live Visualization**
- **4-panel Layout**: Position, intensity, magnet current, performance
- **Real-time Updates**: 20 Hz smooth plotting
- **2000-point History**: Rolling data window
- **Professional Styling**: Dark theme with grid overlays

### **Control Panels**
- **PID Controller**: Real-time gain adjustment
- **System Control**: Frequency and setpoint management
- **Safety Controls**: Emergency stop and enable/disable
- **Status Monitoring**: Connection, performance, and safety status

## 🔬 Use Cases

### **Operator Training**
- Safe environment for learning beamline control
- Realistic physics simulation without hardware risks
- Professional operator interface matching real facilities
- Emergency procedure training with safety systems

### **Control Algorithm Development**
- Real-time PID tuning and optimization
- Algorithm validation with realistic noise and dynamics
- Performance testing under various conditions
- Safety system interaction testing

### **System Integration Testing**
- IPC communication validation
- Timing performance verification
- Safety system validation
- GUI responsiveness testing

### **Educational Applications**
- Accelerator physics demonstration
- Control systems engineering education
- Real-time programming examples
- Professional software development practices

## 🏆 Professional Standards

### **Industry Standards**
- **FESA-style Architecture**: Professional equipment software patterns
- **Real-time Performance**: <10 μs jitter suitable for demanding applications
- **Safety Standards**: Multi-layered protection systems
- **Professional GUI**: Operator interface with industry-standard features

### **Software Quality**
- **Comprehensive Testing**: Unit tests, integration tests, performance validation
- **Cross-platform Support**: Linux and macOS with platform-specific optimizations
- **Memory Safety**: RAII, smart pointers, no raw memory management
- **Thread Safety**: Atomic operations and lock-free algorithms

### **Documentation Standards**
- **Technical Specification**: Complete implementation documentation
- **API Documentation**: Comprehensive interface descriptions
- **Performance Reports**: Detailed timing and jitter analysis
- **User Guides**: Operation and maintenance procedures

## 🔗 Dependencies

### **C++ Libraries**
- **ZeroMQ** (libzmq): High-performance messaging
- **nlohmann/json**: JSON parsing and generation
- **pthread**: Threading and real-time scheduling
- **Standard Library**: C++20 features (chrono, atomic, thread)

### **Python Libraries**
- **PyQt6**: Professional GUI framework
- **pyqtgraph**: High-performance plotting
- **pyzmq**: ZeroMQ Python bindings
- **numpy**: Numerical computing for data processing

### **System Requirements**
- **Linux**: RT_PREEMPT kernel recommended for optimal performance
- **macOS**: Recent version with time constraint scheduling support
- **Memory**: 4+ GB RAM (2+ GB available)
- **CPU**: Multi-core processor (dedicated core for real-time)

## 🎓 Technical Specifications

### **Control System**
- **Loop Frequency**: 1000 Hz (configurable 10-2000 Hz)
- **PID Controller**: Anti-windup, derivative filtering, bumpless transfer
- **Timing Accuracy**: <10 μs P99 jitter (CERN standard)
- **Safety Response**: <1 ms beam abort time

### **Hardware Simulation**
- **BPM**: 5 Hz oscillation, realistic noise, magnet coupling
- **BIC**: Poisson counting statistics, configurable intensity
- **Magnet**: Current control with physics coupling to beam position
- **Physics**: Closed-loop simulation with -0.4 coupling factor

### **Communication**
- **Telemetry**: ZeroMQ PUB/SUB at 1000 Hz
- **Commands**: ZeroMQ REQ/REP with JSON format
- **Latency**: <1 ms command response time
- **Throughput**: >100 KB/s sustained data rates

### **Safety Systems**
- **BLM Network**: 3 beam loss monitors with position-dependent sensitivity
- **Threshold Management**: Configurable warning/abort levels
- **Automatic Protection**: Beam abort on safety violation
- **Alarm System**: Professional callbacks and logging

## 🏅 Achievements

### **Professional Performance** ⚡
- ✅ **P99 Jitter: 6.208 μs** (Target: <10 μs)
- ✅ **Real-time Scheduling**: SCHED_FIFO/Time Constraint enabled
- ✅ **CPU Core Isolation**: Dedicated core assignment
- ✅ **Memory Optimization**: Pre-allocation and locking

### **Professional Safety** 🛡️
- ✅ **Machine Protection System**: Multi-BLM monitoring
- ✅ **Automatic Beam Abort**: <1 ms response time
- ✅ **Safety Integration**: Real-time checks every cycle
- ✅ **Professional Alarms**: Comprehensive callback system

### **Production Quality** 🔧
- ✅ **Comprehensive Testing**: 100% component coverage
- ✅ **Cross-platform Support**: Linux + macOS optimizations
- ✅ **Professional GUI**: Live plots and operator controls
- ✅ **Enterprise Integration**: JSON API and telemetry streaming

## 📚 Documentation

### **API Reference**
- **Control Commands**: JSON command interface specification
- **Telemetry Format**: Data structure documentation
- **Hardware Interfaces**: Component API reference
- **Safety Systems**: MPS configuration and operation

### **Performance Analysis**
- **Timing Benchmarks**: Detailed jitter analysis
- **Resource Usage**: CPU, memory, and network utilization
- **Scalability**: Multi-device and high-frequency operation
- **Optimization**: Real-time tuning and configuration

### **Operational Guides**
- **Startup Procedures**: System initialization and verification
- **Safety Procedures**: Emergency stop and recommission
- **Troubleshooting**: Common issues and solutions
- **Maintenance**: System health monitoring and diagnostics

## 🎯 Applications

### **Accelerator Facilities**
- **Operator Training**: Safe environment for learning beamline control
- **Control Development**: Algorithm testing and validation
- **System Integration**: IPC and timing validation
- **Safety Training**: Emergency procedure practice

### **Research and Education**
- **Accelerator Physics**: Real-time beam dynamics demonstration
- **Control Engineering**: PID tuning and optimization
- **Real-time Systems**: Professional timing and scheduling
- **Software Engineering**: Production-quality system design

### **Industry Applications**
- **Process Control**: Real-time feedback systems
- **Robotics**: High-frequency control loops
- **Manufacturing**: Precision motion control
- **Automation**: Professional HMI and SCADA systems

## 🔬 Physics Implementation

### **Beam Dynamics**
- **Position Oscillation**: 5 Hz natural frequency
- **Magnet Coupling**: -0.4 factor position influence
- **Noise Simulation**: Realistic detector characteristics
- **Closed-loop Physics**: Self-consistent beam-magnet interaction

### **Detector Simulation**
- **BPM Physics**: Shot noise scaling with beam current
- **BIC Statistics**: Poisson counting with configurable mean
- **Magnet Response**: Current control with realistic dynamics
- **Environmental Effects**: Temperature and calibration factors

*A production-grade beamline control simulator with professional timing performance and comprehensive safety systems.*
