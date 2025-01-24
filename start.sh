#!/bin/bash

FIFO_BOAT1="fifo_boat1"
FIFO_BOAT2="fifo_boat2"

cleanup() {
  echo "[start.sh] Otrzymano sygnał - zatrzymuję pętlę pasażerów..."
  exit 0
}

trap cleanup SIGINT SIGTERM

echo "[start.sh] Usuwam i tworzę FIFO..."
rm -f "$FIFO_BOAT1" "$FIFO_BOAT2"
mkfifo "$FIFO_BOAT1"
mkfifo "$FIFO_BOAT2"

echo "[start.sh] Uruchamiam nowyczas..."
./nowyczas &
echo $! > nowyczas.pid

echo "[start.sh] Uruchamiam kasjer..."
./kasjer &
echo $! > kasjer.pid

echo "[start.sh] Uruchamiam pomost..."
./pomost &
echo $! > pomost.pid

echo "[start.sh] Uruchamiam statek..."
./statek &
echo $! > statek.pid

echo "[start.sh] FIFO dla boat1 i boat2 gotowe. Rozpoczynam uruchamianie pasażerów..."

while true
do
  ./pasazer &
  sleep 3
done
