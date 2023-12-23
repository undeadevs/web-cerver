# Web Cerver

A simple (static) Web Server in C.

Made for learning and not for production use.

## Defaults

Host = `0.0.0.0` (unchangeable)

Port = `3000`

Serve Directory = `.` (current working directory)


## Usage

```bash
# build the program
./build.sh

# serve static files in current working directory
./main.out

# serve static files in current working directory to port 8080
./main.out 8080

# serve static files in `public` (relative to current working directory)
./main.out public

# serve static files in `./public` (relative to current working directory)
./main.out ./public

# serve static files in `/public` (absolute path)
./main.out /public

# serve static files in `./public` to port 8080 
./main.out 8080 ./public
```

## Supported Platforms

For now, this program is only supported for Linux.
