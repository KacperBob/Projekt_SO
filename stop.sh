#!/bin/bash

echo "Zatrzymywanie procesów..."
pkill -9 pasazer
pkill -9 kasjer
pkill -9 pomost
pkill -9 statek
pkill -9 sternik
pkill -9 policja
pkill -9 manager_pasazerow
pkill -9 start.sh

sleep 1

echo "Usuwanie zasobów IPC..."

TICKET_KEY=1234
BOARDING_KEY=5678
DEPART_KEY=9012
SHM_KEY=2345

TICKET_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $TICKET_KEY) '$1==key {print $2}')
if [ ! -z "$TICKET_Q" ]; then
    ipcrm -q $TICKET_Q
    echo "Usunięto kolejkę ticket (id: $TICKET_Q)"
fi

BOARDING_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $BOARDING_KEY) '$1==key {print $2}')
if [ ! -z "$BOARDING_Q" ]; then
    ipcrm -q $BOARDING_Q
    echo "Usunięto kolejkę boarding (id: $BOARDING_Q)"
fi

DEPART_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $DEPART_KEY) '$1==key {print $2}')
if [ ! -z "$DEPART_Q" ]; then
    ipcrm -q $DEPART_Q
    echo "Usunięto kolejkę depart (id: $DEPART_Q)"
fi

SHM_ID=$(ipcs -m | awk -v key=$(printf "0x%x" $SHM_KEY) '$1==key {print $2}')
if [ ! -z "$SHM_ID" ]; then
    ipcrm -m $SHM_ID
    echo "Usunięto pamięć dzieloną (id: $SHM_ID)"
fi

# Dodatkowo wykonaj ipcrm -a bez komunikatów
ipcrm -a > /dev/null 2>&1

echo "Stop zakończony."
