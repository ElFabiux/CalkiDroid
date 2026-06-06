# CalkiDroid

Interactive drawing robot system based on a SCARA-style robotic arm. The project allows users to draw on a web canvas and reproduce those strokes physically on a whiteboard using an ESP32, three servos, and a Python Flask serial bridge.

## Overview

CalkiDroid is a hardware-software project that connects a browser-based drawing interface with a physical robotic arm. Users can draw from a computer or mobile device, and the system translates the digital strokes into movement commands that are executed by the ESP32-controlled robot.

The project demonstrates the integration of web development, backend communication, embedded programming, serial communication, servo control, and inverse kinematics.

## Key Features

* Web-based drawing interface using an HTML5 canvas
* Mouse and touch input support
* Python Flask server used as a bridge between the browser and the ESP32
* Serial communication between the backend and the microcontroller
* ESP32 firmware for controlling three servos
* SCARA-style robotic arm movement
* Inverse kinematics for coordinate-based drawing
* Drawing pipeline from digital canvas to physical pen movement
* Basic operating modes for calibration, clock mode, and drawing mode
* Mobile-friendly local network control

## Tech Stack

* HTML5
* CSS3
* JavaScript
* Python
* Flask
* C++
* Arduino
* ESP32
* ESP32Servo
* Serial Communication

## Architecture

The system is divided into three main layers:

### Web Interface

The frontend provides a browser-based drawing surface where the user can create strokes using mouse or touch input. The interface captures the drawing coordinates, converts them into a format that matches the robot workspace, and sends the data to the backend.

### Python Serial Bridge

The Flask backend receives the drawing data from the web interface and converts it into a sequence of commands for the robot. It manages the drawing flow, including erase, lift, move, draw, and park actions.

The backend communicates with the ESP32 through a serial connection and waits for confirmation before sending the next command, helping keep the physical movement synchronized.

### ESP32 Firmware

The firmware receives serial commands and controls the servos that move the robotic arm and pen. It uses inverse kinematics to calculate the servo positions needed to reach specific coordinates on the drawing surface.

## Hardware

The project uses:

* ESP32 DevKit or compatible board
* Three micro servos
* Whiteboard or drawing surface
* External 5V power supply for the servos
* USB connection between the ESP32 and the computer

## How It Works

1. The user draws on the web canvas.
2. The frontend captures and serializes the stroke coordinates.
3. The drawing data is sent to the Flask backend.
4. The backend converts the strokes into serial commands.
5. The ESP32 receives each command and controls the servos.
6. The robotic arm reproduces the drawing physically on the board.

## Operating Modes

The firmware supports different operating modes:

* **Calibration Mode:** Used to test mechanical limits and servo movement.
* **Time Mode:** Allows the robot to work as a standalone clock.
* **Draw Mode:** Receives commands from the web interface and draws user-created strokes.

## Getting Started

Install the Python dependencies:

```bash
pip install flask pyserial
```

Configure the serial port in the Python application to match your ESP32 port:

```python
PUERTO_SERIAL = "COM6"
```

Run the Flask server:

```bash
python app.py
```

Open the web interface locally:

```bash
http://localhost:5000
```

For mobile access, connect your phone to the same Wi-Fi network and open the local IP address of the computer running the server.

## Project Purpose

This project was created to explore the connection between software and physical hardware. It demonstrates how a web interface, a Python backend, and embedded firmware can work together to control a real robotic drawing system.
