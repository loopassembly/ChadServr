# ChadServr - Video Chunk Processing Server

A robust, high-performance video chunk processing server written in C++. ChadServr provides a flexible API for uploading, processing, and managing video chunks, making it ideal for video streaming applications, transcoding services, and media processing pipelines.

## Features

- **HTTP API** for video chunk upload and management
- **Parallel processing** using a configurable thread pool
- **FFmpeg integration** for video transcoding and manipulation
- **Stateful chunk tracking** with detailed metadata
- **Configuration management** via JSON files
- **Comprehensive logging system**
- **Storage management** with automatic cleanup

## Requirements

- C++17 compatible compiler
- CMake 3.10+
- Boost libraries (system, thread)
- FFmpeg command-line tools
- nlohmann/json library (automatically downloaded by CMake)

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Running

```bash
# From the build directory
./bin/chadservr
```

The server will start on port 8080 by default, as specified in the configuration file.

## Configuration

Edit `config/server_config.json` to configure the server:

```json
{
  "server": {
    "port": 8080,
    "worker_threads": 4
  },
  "video_processing": {
    "thread_pool_size": 2,
    "storage_path": "storage/processed",
    "temp_path": "storage/temp",
    "max_chunks": 100
  }
}
```

## API Reference

### Upload Video Chunk

```
POST /api/upload?options={...}
Content-Type: video/*
```

Options parameter is a URL-encoded JSON string with processing parameters:

```json
{
  "resize": {"width": 1280, "height": 720},
  "bitrate": "2M",
  "codec": "libx264"
}
```

### List Chunks

```
GET /api/chunks
```

### Get Chunk Info

```
GET /api/chunks/info?id={chunk_id}
```

### Delete Chunk

```
DELETE /api/chunks?id={chunk_id}
```

### Server Status

```
GET /api/status
```

## Architecture

ChadServr is built with a modular architecture:

- **HttpServer**: Handles HTTP requests and routes
- **VideoProcessor**: Processes video chunks using FFmpeg
- **ThreadPool**: Manages parallel processing tasks
- **StorageManager**: Handles file storage and retrieval
- **Logger**: Provides application-wide logging
- **Config**: Manages configuration settings

## License

MIT License

## Contributing

1. Fork the repository
2. Create a feature branch
3. Submit a pull request