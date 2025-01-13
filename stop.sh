#!/bin/bash

echo "Zatrzymuję procesy i usuwam zasoby IPC..."

# Zatrzymaj proces nowyczas
if [ -f nowyczas.pid ]; then
    PID_NOWYCZAS=$(cat nowyczas.pid)
    if kill -0 $PID_NOWYCZAS 2>/dev/null; then
        echo "Zatrzymuję nowyczas (PID: $PID_NOWYCZAS)..."
        kill -15 $PID_NOWYCZAS
        sleep 1
        kill -9 $PID_NOWYCZAS 2>/dev/null
    fi
    rm -f nowyczas.pid
fi

# Zatrzymaj proces kasjer
if [ -f kasjer.pid ]; then
    PID_KASJER=$(cat kasjer.pid)
    if kill -0 $PID_KASJER 2>/dev/null; then
        echo "Zatrzymuję kasjer (PID: $PID_KASJER)..."
        kill -15 $PID_KASJER
        sleep 1
        kill -9 $PID_KASJER 2>/dev/null
    fi
    rm -f kasjer.pid
fi

# Zatrzymaj proces pomost
if [ -f pomost.pid ]; then
    PID_POMOST=$(cat pomost.pid)
    if kill -0 $PID_POMOST 2>/dev/null; then
        echo "Zatrzymuję pomost (PID: $PID_POMOST)..."
        kill -15 $PID_POMOST
        sleep 1
        kill -9 $PID_POMOST 2>/dev/null
    fi
    rm -f pomost.pid
fi

# Zatrzymaj proces statek
if [ -f statek.pid ]; then
    PID_STATEK=$(cat statek.pid)
    if kill -0 $PID_STATEK 2>/dev/null; then
        echo "Zatrzymuję statek (PID: $PID_STATEK)..."
        kill -15 $PID_STATEK
        sleep 1
        kill -9 $PID_STATEK 2>/dev/null
    fi
    rm -f statek.pid
fi

# Zatrzymaj procesy pasażerów
echo "Zatrzymuję procesy pasażerów..."
pkill -f ./pasazer 2>/dev/null

# Zatrzymaj procesy pasażerowie
echo "Zatrzymuję procesy pasażerowie..."
pkill -f ./pasazerowie 2>/dev/null

# Usuwanie segmentów pamięci współdzielonej
echo "Usuwam segmenty pamięci współdzielonej..."
ipcs -m | grep $(whoami) | awk '{print $2}' | while read shmid; do
    ipcrm -m $shmid 2>/dev/null
done

# Usuwanie kolejek komunikatów
echo "Usuwam kolejki komunikatów..."
ipcs -q | grep $(whoami) | awk '{print $2}' | while read msqid; do
    ipcrm -q $msqid 2>/dev/null
done

# Usuwanie plików FIFO
echo "Usuwam pliki FIFO..."
rm -f fifo_boat1 fifo_boat2

# Usunięcie wszystkich pozostałych zasobów IPC bez komunikatów
ipcrm -a 2>/dev/null

# Potwierdzenie zakończenia
echo "Wszystkie procesy i zasoby IPC zostały usunięte."
