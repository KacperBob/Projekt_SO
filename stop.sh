#!/bin/bash

if [ -f przeskalowany_czas.pid ]; then
    kill $(cat przeskalowany_czas.pid) 2>/dev/null
    rm -f przeskalowany_czas.pid
fi

if [ -f kasjer.pid ]; then
    kill $(cat kasjer.pid) 2>/dev/null
    rm -f kasjer.pid
fi

if [ -f pasazer.pid ]; then
    while read pid; do
        kill $pid 2>/dev/null
    done < pasazer.pid
    rm -f pasazer.pid
fi

ipcs -q | grep $(whoami) | awk '{print $2}' | xargs -n 1 ipcrm -q 2>/dev/null
ipcs -m | grep $(whoami) | awk '{print $2}' | xargs -n 1 ipcrm -m 2>/dev/null
ipcs -s | grep $(whoami) | awk '{print $2}' | xargs -n 1 ipcrm -s 2>/dev/null
