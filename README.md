# HTTP/1.1 Server in C

A multi-threaded HTTP/1.1 server built from scratch using raw POSIX sockets and pthreads. No frameworks, no libraries.

## Features
- Thread pool with 10 worker threads
- Producer-consumer queue (capacity 1000) with mutex + condition variables
- HTTP keep-alive (persistent connections)
- Static file serving with MIME type detection (HTML, CSS, JS, PNG, JPG, GIF)
- POST request handling with Content-Length parsing
- Directory traversal protection (blocks ../ paths)
- Apache-style access logging
- Per-connection socket timeout (5s)

## Build & Run
```bash
gcc -o server HTTP.c -lpthread
mkdir public
echo "<h1>Hello</h1>" > public/index.html
./server
```

Then open http://localhost:8080 in your browser.

## Architecture
- Main thread runs accept() loop on port 8080
- Connections are enqueued into a circular buffer
- Worker threads dequeue and handle each connection
- Each worker handles GET and POST requests with keep-alive support
