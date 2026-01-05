#!/usr/bin/env bash

mkdir -p bin
gcc -Wall -Wextra -Werror -std=c99 -pedantic -o bin/web-cerver main.c
