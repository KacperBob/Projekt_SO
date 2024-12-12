#!/bin/bash

rm -f raport_kasjera.txt raport_pasazera.txt

./przeskalowany_czas & echo $! > przeskalowany_czas.pid
./kasjer & echo $! > kasjer.pid

while true; do
    ./pasazer & echo $! >> pasazer.pid
    sleep $((RANDOM % 5 + 1))
done
