#!/bin/bash

./przeskalowany_czas &

echo $! > nowyczas.pid

./kasjer &
./pasażer &
./statek &

wait
