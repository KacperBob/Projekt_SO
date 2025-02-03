#!/bin/bash

echo "Zatrzymywanie procesów..."

# Używamy pkill -9 (SIGKILL) aby na pewno zabić procesy
pkill -9 pasazer
pkill -9 kasjer
pkill -9 pomost
pkill -9 statek
pkill -9 sternik
pkill -9 policja

sleep 1

echo "Usuwanie zasobów IPC..."

TICKET_KEY=1234
BOARDING_KEY=5678
DEPART_KEY=9012
SHM_KEY=2345

# Usuwanie kolejki ticket
TICKET_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $TICKET_KEY) '$1==key {print $2}')
if [ ! -z "$TICKET_Q" ]; then
    ipcrm -q $TICKET_Q
    echo "Usunięto kolejkę ticket (id: $TICKET_Q)"
fi

# Usuwanie kolejki boarding
BOARDING_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $BOARDING_KEY) '$1==key {print $2}')
if [ ! -z "$BOARDING_Q" ]; then
    ipcrm -q $BOARDING_Q
    echo "Usunięto kolejkę boarding (id: $BOARDING_Q)"
fi

# Usuwanie kolejki depart
DEPART_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $DEPART_KEY) '$1==key {print $2}')
if [ ! -z "$DEPART_Q" ]; then
    ipcrm -q $DEPART_Q
    echo "Usunięto kolejkę depart (id: $DEPART_Q)"
fi

# Usuwanie pamięci dzielonej
SHM_ID=$(ipcs -m | awk -v key=$(printf "0x%x" $SHM_KEY) '$1==key {print $2}')
if [ ! -z "$SHM_ID" ]; then
    ipcrm -m $SHM_ID
    echo "Usunięto pamięć dzieloną (id: $SHM_ID)"
fi

# Dodatkowo wykonaj polecenie ipcrm -a, bez wyświetlania komunikatów
ipcrm -a > /dev/null 2>&1

echo "Stop zakończony."
