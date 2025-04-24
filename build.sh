#! /usr/bin/env bash
gcc -I/opt/homebrew/include -L/opt/homebrew/lib -lSDL3 main.c minigb_apu.c -o main
