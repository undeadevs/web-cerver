# Web Cerver

A simple (static) Web Server in C.

Made for learning and not for production use.

## How to Start
```bash
# build the program
./build.sh

# serve static files in current working directory
./main.out

# serve static files in `serve-dir` (path relative to current working directory)
./main.out serve-dir

# serve static files in `./serve-dir` (path relative to current working directory)
./main.out ./serve-dir

# serve static files in `/serve-dir` (absolute path)
./main.out /serve-dir
```

## Supported Platforms
For now, this program is only supported for Linux.
