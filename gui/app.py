#!/usr/bin/env python3
"""
Beamline Operator Console

Real-time monitoring and control interface for the beamline simulator.
Provides live plots of beam position and intensity, PID gain controls,
and system status monitoring.
"""

import json
import sys
import time
import threading
import traceback
from collections import deque
from typing import Dict, Any, Optional
import numpy as np

from PyQt6 import QtWidgets, QtCore, QtGui
import pyqtgraph as pg
import zmq


class TelemetryThread(QtCore.QThread):
    """Background thread for receiving telemetry data via ZeroMQ"""
    
    data_received = QtCore.pyqtSignal(dict)
    error_occurred = QtCore.pyqtSignal(str)
    
    def __init__(self, telemetry_address: str = "tcp://127.0.0.1:5556"):
        super().__init__()
        self.telemetry_address = telemetry_address
        self.running = True
        self.context = None
        self.subscriber = None
        
    def run(self):
        """Main thread loop for receiving telemetry"""
        try:
            self.context = zmq.Context()
            self.subscriber = self.context.socket(zmq.SUB)
            self.subscriber.connect(self.telemetry_address)
            self.subscriber.setsockopt(zmq.SUBSCRIBE, b"telemetry")
            self.subscriber.setsockopt(zmq.SUBSCRIBE, b"alarm")
            self.subscriber.setsockopt(zmq.SUBSCRIBE, b"error")
            self.subscriber.setsockopt(zmq.SUBSCRIBE, b"status")
            
            # Set receive timeout to allow clean shutdown
            self.subscriber.setsockopt(zmq.RCVTIMEO, 1000)  # 1 second timeout
            
            while self.running:
                try:
                    # Receive topic and payload
                    topic = self.subscriber.recv_string(zmq.DONTWAIT)
                    payload = self.subscriber.recv_string(zmq.DONTWAIT)
                    
                    # Parse JSON payload
                    data = json.loads(payload)
                    data['_topic'] = topic
                    
                    # Emit signal with data
                    self.data_received.emit(data)
                    
                except zmq.Again:
                    # Timeout - continue loop
                    continue
                except json.JSONDecodeError as e:
                    self.error_occurred.emit(f"JSON decode error: {e}")
                except Exception as e:
                    self.error_occurred.emit(f"Telemetry error: {e}")
                    
        except Exception as e:
            self.error_occurred.emit(f"Failed to setup telemetry: {e}")
        finally:
            self.cleanup()
    
    def stop(self):
        """Stop the telemetry thread"""
        self.running = False
        
    def cleanup(self):
        """Clean up ZeroMQ resources"""
        if self.subscriber:
            self.subscriber.close()
        if self.context:
            self.context.term()


class ControlClient:
    """ZeroMQ client for sending control commands"""
    
    def __init__(self, control_address: str = "tcp://127.0.0.1:5555"):
        self.control_address = control_address
        self.context = zmq.Context()
        self.requester = self.context.socket(zmq.REQ)
        self.requester.connect(control_address)
        self.requester.setsockopt(zmq.RCVTIMEO, 5000)  # 5 second timeout
        
    def send_command(self, command: Dict[str, Any]) -> Dict[str, Any]:
        """Send command and return response"""
        try:
            self.requester.send_string(json.dumps(command))
            response_str = self.requester.recv_string()
            return json.loads(response_str)
        except zmq.Again:
            return {"ok": False, "error": "Command timeout"}
        except Exception as e:
            return {"ok": False, "error": str(e)}
    
    def close(self):
        """Close the control client"""
        self.requester.close()
        self.context.term()


class BeamlineConsole(QtWidgets.QMainWindow):
    """Main application window for the beamline operator console"""
    
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Beamline Operator Console")
        self.setWindowIcon(QtGui.QIcon())
        
        # Data storage
        self.max_points = 2000
        self.time_data = deque(maxlen=self.max_points)
        self.position_data = deque(maxlen=self.max_points)
        self.intensity_data = deque(maxlen=self.max_points)
        self.magnet_data = deque(maxlen=self.max_points)
        self.loop_time_data = deque(maxlen=self.max_points)
        
        # Status tracking
        self.last_telemetry_time = 0
        self.total_deadline_misses = 0
        self.connection_status = "Disconnected"
        
        # Initialize components
        self.control_client = ControlClient()
        self.telemetry_thread = TelemetryThread()
        
        # Setup UI
        self.setup_ui()
        self.setup_plots()
        self.setup_telemetry()
        
        # Start telemetry reception
        self.telemetry_thread.start()
        
        # Setup periodic updates
        self.update_timer = QtCore.QTimer()
        self.update_timer.timeout.connect(self.update_plots)
        self.update_timer.start(50)  # 20 Hz update rate
        
        # Setup status update timer
        self.status_timer = QtCore.QTimer()
        self.status_timer.timeout.connect(self.update_status)
        self.status_timer.start(1000)  # 1 Hz status updates
        
    def setup_ui(self):
        """Setup the main user interface"""
        central_widget = QtWidgets.QWidget()
        self.setCentralWidget(central_widget)
        
        # Main layout
        layout = QtWidgets.QVBoxLayout(central_widget)
        
        # Status bar
        self.status_label = QtWidgets.QLabel("Status: Starting up...")
        self.status_label.setStyleSheet("background-color: #f0f0f0; padding: 5px; border: 1px solid gray;")
        layout.addWidget(self.status_label)
        
        # Plot area
        self.plot_widget = pg.GraphicsLayoutWidget()
        layout.addWidget(self.plot_widget, stretch=3)
        
        # Control panel
        control_frame = QtWidgets.QFrame()
        control_frame.setFrameStyle(QtWidgets.QFrame.Shape.StyledPanel)
        control_layout = QtWidgets.QVBoxLayout(control_frame)
        
        # PID Controls
        pid_group = QtWidgets.QGroupBox("PID Controller")
        pid_layout = QtWidgets.QGridLayout(pid_group)
        
        # PID gain controls
        self.kp_spin = QtWidgets.QDoubleSpinBox()
        self.kp_spin.setRange(0.0, 10.0)
        self.kp_spin.setValue(0.6)
        self.kp_spin.setSingleStep(0.1)
        self.kp_spin.setDecimals(3)
        
        self.ki_spin = QtWidgets.QDoubleSpinBox()
        self.ki_spin.setRange(0.0, 10.0)
        self.ki_spin.setValue(0.05)
        self.ki_spin.setSingleStep(0.01)
        self.ki_spin.setDecimals(3)
        
        self.kd_spin = QtWidgets.QDoubleSpinBox()
        self.kd_spin.setRange(0.0, 10.0)
        self.kd_spin.setValue(0.0)
        self.kd_spin.setSingleStep(0.01)
        self.kd_spin.setDecimals(3)
        
        self.setpoint_spin = QtWidgets.QDoubleSpinBox()
        self.setpoint_spin.setRange(-5.0, 5.0)
        self.setpoint_spin.setValue(0.0)
        self.setpoint_spin.setSingleStep(0.1)
        self.setpoint_spin.setDecimals(2)
        self.setpoint_spin.setSuffix(" mm")
        
        pid_layout.addWidget(QtWidgets.QLabel("Kp:"), 0, 0)
        pid_layout.addWidget(self.kp_spin, 0, 1)
        pid_layout.addWidget(QtWidgets.QLabel("Ki:"), 0, 2)
        pid_layout.addWidget(self.ki_spin, 0, 3)
        pid_layout.addWidget(QtWidgets.QLabel("Kd:"), 1, 0)
        pid_layout.addWidget(self.kd_spin, 1, 1)
        pid_layout.addWidget(QtWidgets.QLabel("Setpoint:"), 1, 2)
        pid_layout.addWidget(self.setpoint_spin, 1, 3)
        
        # System Controls
        system_group = QtWidgets.QGroupBox("System Control")
        system_layout = QtWidgets.QGridLayout(system_group)
        
        self.freq_spin = QtWidgets.QSpinBox()
        self.freq_spin.setRange(10, 2000)
        self.freq_spin.setValue(1000)
        self.freq_spin.setSuffix(" Hz")
        
        self.apply_btn = QtWidgets.QPushButton("Apply Settings")
        self.apply_btn.setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;")
        
        self.recommission_btn = QtWidgets.QPushButton("Recommission")
        self.recommission_btn.setStyleSheet("background-color: #FF9800; color: white; font-weight: bold;")
        
        self.emergency_stop_btn = QtWidgets.QPushButton("EMERGENCY STOP")
        self.emergency_stop_btn.setStyleSheet("background-color: #f44336; color: white; font-weight: bold;")
        
        self.enable_control_btn = QtWidgets.QPushButton("Enable Control")
        self.enable_control_btn.setCheckable(True)
        self.enable_control_btn.setChecked(True)
        self.enable_control_btn.setStyleSheet("background-color: #2196F3; color: white;")
        
        system_layout.addWidget(QtWidgets.QLabel("Frequency:"), 0, 0)
        system_layout.addWidget(self.freq_spin, 0, 1)
        system_layout.addWidget(self.apply_btn, 0, 2)
        system_layout.addWidget(self.recommission_btn, 1, 0)
        system_layout.addWidget(self.emergency_stop_btn, 1, 1)
        system_layout.addWidget(self.enable_control_btn, 1, 2)
        
        control_layout.addWidget(pid_group)
        control_layout.addWidget(system_group)
        layout.addWidget(control_frame, stretch=1)
        
        # Connect signals
        self.apply_btn.clicked.connect(self.apply_settings)
        self.recommission_btn.clicked.connect(self.recommission_system)
        self.emergency_stop_btn.clicked.connect(self.emergency_stop)
        self.enable_control_btn.clicked.connect(self.toggle_control)
        
        # Set window size
        self.resize(1200, 800)
        
    def setup_plots(self):
        """Setup the plot widgets"""
        # Position plot
        self.pos_plot = self.plot_widget.addPlot(title="Beam Position", row=0, col=0)
        self.pos_plot.setLabel('left', 'Position', 'mm')
        self.pos_plot.setLabel('bottom', 'Time', 's')
        self.pos_plot.showGrid(x=True, y=True)
        self.pos_curve = self.pos_plot.plot(pen=pg.mkPen(color='blue', width=2), symbol='o', symbolSize=3)
        self.setpoint_line = pg.InfiniteLine(pos=0, angle=0, pen=pg.mkPen(color='red', style=QtCore.Qt.PenStyle.DashLine))
        self.pos_plot.addItem(self.setpoint_line)
        
        # Intensity plot  
        self.intensity_plot = self.plot_widget.addPlot(title="Beam Intensity", row=0, col=1)
        self.intensity_plot.setLabel('left', 'Intensity', 'counts')
        self.intensity_plot.setLabel('bottom', 'Time', 's')
        self.intensity_plot.showGrid(x=True, y=True)
        self.intensity_curve = self.intensity_plot.plot(pen=pg.mkPen(color='green', width=2))
        
        # Magnet current plot
        self.magnet_plot = self.plot_widget.addPlot(title="Magnet Current", row=1, col=0)
        self.magnet_plot.setLabel('left', 'Current', 'A')
        self.magnet_plot.setLabel('bottom', 'Time', 's')
        self.magnet_plot.showGrid(x=True, y=True)
        self.magnet_curve = self.magnet_plot.plot(pen=pg.mkPen(color='orange', width=2))
        
        # Loop performance plot
        self.perf_plot = self.plot_widget.addPlot(title="Loop Performance", row=1, col=1)
        self.perf_plot.setLabel('left', 'Loop Time', 'ms')
        self.perf_plot.setLabel('bottom', 'Time', 's')
        self.perf_plot.showGrid(x=True, y=True)
        self.perf_curve = self.perf_plot.plot(pen=pg.mkPen(color='purple', width=2))
        
    def setup_telemetry(self):
        """Setup telemetry reception"""
        self.telemetry_thread.data_received.connect(self.handle_telemetry_data)
        self.telemetry_thread.error_occurred.connect(self.handle_telemetry_error)
        
    def handle_telemetry_data(self, data: Dict[str, Any]):
        """Handle incoming telemetry data"""
        try:
            topic = data.get('_topic', 'telemetry')
            
            if topic == 'telemetry':
                # Main telemetry data
                t = data.get('t', 0)
                pos = data.get('pos', 0)
                intensity = data.get('intensity', 0)
                mag_current = data.get('mag', 0)
                loop_time = data.get('loop_time_ms', 0)
                deadline_miss = data.get('deadline_miss', 0)
                
                # Store data
                self.time_data.append(t)
                self.position_data.append(pos)
                self.intensity_data.append(intensity)
                self.magnet_data.append(mag_current)
                self.loop_time_data.append(loop_time)
                
                # Update status
                self.last_telemetry_time = time.time()
                self.connection_status = "Connected"
                
                if deadline_miss:
                    self.total_deadline_misses += 1
                    
            elif topic == 'alarm':
                # Handle alarm messages
                alarm_type = data.get('type', 'unknown')
                self.show_alarm(f"ALARM: {alarm_type}", data)
                
            elif topic == 'error':
                # Handle error messages
                error_msg = data.get('error', 'Unknown error')
                self.show_error(f"ERROR: {error_msg}")
                
        except Exception as e:
            print(f"Error handling telemetry data: {e}")
            
    def handle_telemetry_error(self, error_msg: str):
        """Handle telemetry errors"""
        print(f"Telemetry error: {error_msg}")
        self.connection_status = "Error"
        
    def update_plots(self):
        """Update all plots with latest data"""
        if not self.time_data:
            return
            
        try:
            # Convert to numpy arrays for plotting
            times = np.array(self.time_data)
            positions = np.array(self.position_data)
            intensities = np.array(self.intensity_data)
            magnet_currents = np.array(self.magnet_data)
            loop_times = np.array(self.loop_time_data)
            
            # Update curves
            self.pos_curve.setData(times, positions)
            self.intensity_curve.setData(times, intensities)
            self.magnet_curve.setData(times, magnet_currents)
            self.perf_curve.setData(times, loop_times)
            
            # Update setpoint line
            self.setpoint_line.setPos(self.setpoint_spin.value())
            
        except Exception as e:
            print(f"Error updating plots: {e}")
            
    def update_status(self):
        """Update status bar"""
        try:
            # Check connection status
            time_since_last = time.time() - self.last_telemetry_time
            if time_since_last > 2.0:
                self.connection_status = "Disconnected"
                
            # Get system status
            status_response = self.control_client.send_command({"cmd": "get_status"})
            
            if status_response.get("ok", False):
                freq = status_response.get("loop_frequency", 0)
                loop_count = status_response.get("loop_count", 0)
                avg_time = status_response.get("avg_loop_time_ms", 0)
                control_enabled = status_response.get("control_enabled", False)
                emergency_stop = status_response.get("emergency_stop", False)
                
                status_text = f"Status: {self.connection_status} | "
                status_text += f"Freq: {freq:.0f} Hz | "
                status_text += f"Loops: {loop_count} | "
                status_text += f"Avg: {avg_time:.2f} ms | "
                status_text += f"Misses: {self.total_deadline_misses} | "
                status_text += f"Control: {'ON' if control_enabled else 'OFF'}"
                
                if emergency_stop:
                    status_text += " | EMERGENCY STOP ACTIVE"
                    
                # Update button states
                self.enable_control_btn.setChecked(control_enabled)
                if emergency_stop:
                    self.enable_control_btn.setEnabled(False)
                    self.emergency_stop_btn.setStyleSheet("background-color: #d32f2f; color: white; font-weight: bold;")
                else:
                    self.enable_control_btn.setEnabled(True)
                    self.emergency_stop_btn.setStyleSheet("background-color: #f44336; color: white; font-weight: bold;")
                    
            else:
                status_text = f"Status: {self.connection_status} | Control communication error"
                
            self.status_label.setText(status_text)
            
        except Exception as e:
            self.status_label.setText(f"Status: Error - {e}")
            
    def apply_settings(self):
        """Apply PID and frequency settings"""
        try:
            # Send PID gains
            pid_response = self.control_client.send_command({
                "cmd": "set_pid",
                "kp": self.kp_spin.value(),
                "ki": self.ki_spin.value(),
                "kd": self.kd_spin.value()
            })
            
            # Send frequency
            freq_response = self.control_client.send_command({
                "cmd": "set_freq",
                "hz": self.freq_spin.value()
            })
            
            # Send setpoint
            sp_response = self.control_client.send_command({
                "cmd": "set_setpoint",
                "sp": self.setpoint_spin.value()
            })
            
            if not all([pid_response.get("ok"), freq_response.get("ok"), sp_response.get("ok")]):
                self.show_error("Failed to apply some settings")
            else:
                self.show_info("Settings applied successfully")
                
        except Exception as e:
            self.show_error(f"Error applying settings: {e}")
            
    def recommission_system(self):
        """Recommission the control system"""
        try:
            response = self.control_client.send_command({"cmd": "recommission"})
            
            if response.get("ok", False):
                self.show_info("System recommissioned successfully")
                # Clear data
                self.time_data.clear()
                self.position_data.clear()
                self.intensity_data.clear()
                self.magnet_data.clear()
                self.loop_time_data.clear()
                self.total_deadline_misses = 0
            else:
                self.show_error(f"Recommission failed: {response.get('error', 'Unknown error')}")
                
        except Exception as e:
            self.show_error(f"Error during recommission: {e}")
            
    def emergency_stop(self):
        """Activate emergency stop"""
        try:
            response = self.control_client.send_command({"cmd": "emergency_stop"})
            
            if response.get("ok", False):
                self.show_alarm("EMERGENCY STOP ACTIVATED", {})
            else:
                self.show_error(f"Emergency stop failed: {response.get('error', 'Unknown error')}")
                
        except Exception as e:
            self.show_error(f"Error activating emergency stop: {e}")
            
    def toggle_control(self):
        """Toggle control enable/disable"""
        try:
            enable = self.enable_control_btn.isChecked()
            response = self.control_client.send_command({
                "cmd": "enable_control",
                "enable": enable
            })
            
            if not response.get("ok", False):
                self.show_error(f"Failed to toggle control: {response.get('error', 'Unknown error')}")
                # Revert button state
                self.enable_control_btn.setChecked(not enable)
                
        except Exception as e:
            self.show_error(f"Error toggling control: {e}")
            
    def show_info(self, message: str):
        """Show information message"""
        QtWidgets.QMessageBox.information(self, "Information", message)
        
    def show_error(self, message: str):
        """Show error message"""
        QtWidgets.QMessageBox.critical(self, "Error", message)
        
    def show_alarm(self, title: str, data: Dict[str, Any]):
        """Show alarm message"""
        msg = QtWidgets.QMessageBox(self)
        msg.setIcon(QtWidgets.QMessageBox.Icon.Warning)
        msg.setWindowTitle("System Alarm")
        msg.setText(title)
        msg.setDetailedText(json.dumps(data, indent=2))
        msg.exec()
        
    def closeEvent(self, event):
        """Handle application close event"""
        try:
            # Stop telemetry thread
            self.telemetry_thread.stop()
            self.telemetry_thread.wait(3000)  # Wait up to 3 seconds
            
            # Close control client
            self.control_client.close()
            
            event.accept()
            
        except Exception as e:
            print(f"Error during shutdown: {e}")
            event.accept()


def main():
    """Main application entry point"""
    app = QtWidgets.QApplication(sys.argv)
    app.setApplicationName("Beamline Operator Console")
    app.setApplicationVersion("1.0")
    
    # Set application style
    app.setStyle('Fusion')
    
    # Create and show main window
    console = BeamlineConsole()
    console.show()
    
    # Run application
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
