#!/bin/bash

if [ -f nowyczas.pid ]; then
    kill $(cat nowyczas.pid)
    rm -f nowyczas.pid
fi

pkill -f kasjer
pkill -f pasażer
pkill -f statek
