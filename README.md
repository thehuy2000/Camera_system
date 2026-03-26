# Embedded Linux Camera System (ELCS)

## Overview
ELCS (Embedded Linux Camera System) is a high-performance camera system designed for Embedded Linux environments. It supports capturing images and video streams from Webcams using the V4L2 (Video4Linux2) API. The system is built with a strong focus on strict memory management, thread safety, and efficiency to ensure reliable real-time operation.

## Key Features
* **Hardware Integration (V4L2)**: Captures images and continuous video from `/dev/video0`.
* **Modes of Operation**:
  * **Snapshot Mode**: Captures single frames and saves them as images (e.g., customized names like `snap_YYYYMMDD_HHMMSS.png`).
  * **Record Mode**: Captures a continuous stream of frames for video recording.
  * **Encode Mode**: Captures a continuous stream of frames for video recording and encoding to H.264 format.
  * **Stream Mode**: Encodes video to H.264 in real-time and streams it over the network via RTSP/RTP for live viewing (e.g. using VLC or ffplay).
* **Concurrency**: Implements a robust Producer-Consumer threading model using POSIX Threads (`pthreads`).
* **Memory Management**: Employs a custom **Memory Pool Allocator** to explicitly pre-allocate fixed-size frame buffers, preventing fragmentation and avoiding `malloc`/`free` operations within the real-time processing loop.
* **Thread-safe Ring Buffer**: Utilizes a customized Ring Buffer protected by mutexes and condition variables to safely transfer captured frames from the producer (camera thread) to the consumer (processing/saving thread).
* **Logging Framework**: Includes a built-in, thread-safe mini logging system supporting multiple severity levels (`DEBUG`, `INFO`, `WARN`, `ERROR`).

## Building the Project

The project is built using a standard `Makefile`. The following dependencies are required:
* `GCC` / `G++`
* `pthread` library
* `libx264` (for H.264 encoding)
* `live555` (for RTSP/RTP streaming)

To build the main executable:
```bash
make clean
make
```

To build and run unit tests for the core modules:
```bash
make test
```

## Running the Application

Once built, the main executable will be available in the `output/` directory:
```bash
./output/elcs_cam [mode] [device]

Examples: 
        ./output/elcs_cam snapshot /dev/video0
        ./output/elcs_cam record /dev/video0
        ./output/elcs_cam encode /dev/video0
        ./output/elcs_cam stream /dev/video0
```

To view the RTSP stream (when running in `stream` mode), open VLC or `ffplay`:
```bash
ffplay rtsp://127.0.0.1:8554/live
```
