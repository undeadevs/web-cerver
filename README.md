# Web Cerver

A simple (static) Web Server in C.

Made for learning and not for production use.

By default, `/` will respond with `index.html`. There's also a `/healthcheck` route.

## Defaults

Host = `0.0.0.0` (unchangeable)

Port = `3000`

Serve Directory = `.` (current working directory)


## Usage

```bash
# build the program
./build.sh

# serve static files in current working directory
./bin/web-cerver

# serve static files in `public` (relative to current working directory)
./bin/web-cerver public

# serve static files in `./public` (relative to current working directory)
./bin/web-cerver ./public

# serve static files in `/public` (absolute path)
./bin/web-cerver /public

# serve static files in `./public` to port 8080 
./bin/web-cerver 8080 ./public
```

## Supported Platforms

For now, this program is only supported for Linux.
