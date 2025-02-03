#!/bin/bash

echo "Czyszczenie starych zasobów IPC..."
# Usuwamy kolejki i pamięć dzieloną, jeśli istnieją
TICKET_KEY=1234
BOARDING_KEY=5678
DEPART_KEY=9012
SHM_KEY=2345

TICKET_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $TICKET_KEY) '$1==key {print $2}')
if [ ! -z "$TICKET_Q" ]; then
    ipcrm -q $TICKET_Q
fi

BOARDING_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $BOARDING_KEY) '$1==key {print $2}')
if [ ! -z "$BOARDING_Q" ]; then
    ipcrm -q $BOARDING_Q
fi

DEPART_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $DEPART_KEY) '$1==key {print $2}')
if [ ! -z "$DEPART_Q" ]; then
    ipcrm -q $DEPART_Q
fi

SHM_ID=$(ipcs -m | awk -v key=$(printf "0x%x" $SHM_KEY) '$1==key {print $2}')
if [ ! -z "$SHM_ID" ]; then
    ipcrm -m $SHM_ID
fi

echo "Kompilacja programów..."
gcc -o pasazer pasazer.c -lrt -lpthread
gcc -o kasjer kasjer.c -lrt -lpthread
gcc -o pomost pomost.c -lrt -lpthread
gcc -o statek statek.c -lrt -lpthread
gcc -o policja policja.c -lrt -lpthread

echo "Inicjalizacja IPC i uruchamianie procesów..."

./kasjer &
KASJER_PID=$!
./pomost &
POMOST_PID=$!
./sternik &   # (sternik.c – wersja niezmieniona z wcześniejszych odpowiedzi)
STERNIK_PID=$!
./statek 1 &
STATEK1_PID=$!
./statek 2 &
STATEK2_PID=$!
./policja $STATEK1_PID $STATEK2_PID &
POLICJA_PID=$!

echo "Uruchomiono procesy:"
echo "kasjer: $KASJER_PID"
echo "pomost: $POMOST_PID"
echo "sternik: $STERNIK_PID"
echo "statek 1: $STATEK1_PID"
echo "statek 2: $STATEK2_PID"
echo "policja: $POLICJA_PID"

echo "Generowanie pasażerów (naciśnij Ctrl+C, aby zatrzymać)..."
while true; do
    ./pasazer &
    sleep 2
done
