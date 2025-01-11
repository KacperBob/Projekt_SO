k#!/bin/bash

echo "Zatrzymuję procesy i usuwam zasoby IPC..."

# Zatrzymaj proces nowyczas
if [ -f nowyczas.pid ]; then
    PID_NOWYCZAS=$(cat nowyczas.pid)
    if kill -0 $PID_NOWYCZAS 2>/dev/null; then
        echo "Zatrzymuję nowyczas (PID: $PID_NOWYCZAS)..."
        kill -15 $PID_NOWYCZAS
        sleep 1
        kill -9 $PID_NOWYCZAS 2>/dev/null
    else
        echo "Proces nowyczas już nie działa."
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
    else
        echo "Proces kasjer już nie działa."
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
    else
        echo "Proces pomost już nie działa."
    fi
    rm -f pomost.pid
fi

# Zatrzymaj procesy łodzi
pkill -f "./statek" 2>/dev/null

# Usuń wszystkie segmenty pamięci współdzielonej należące do użytkownika
echo "Usuwam segmenty pamięci współdzielonej..."
ipcs -m | grep $(whoami) | awk '{print $2}' | while read shmid; do
    echo "Usuwam segment pamięci SHMID: $shmid"
    ipcrm -m $shmid 2>/dev/null
done

# Usuń wszystkie semafory należące do użytkownika
echo "Usuwam semafory..."
ipcs -s | grep $(whoami) | awk '{print $2}' | while read semid; do
    echo "Usuwam semafor SEMID: $semid"
    ipcrm -s $semid 2>/dev/null
done

# Usuń wszystkie kolejki komunikatów należące do użytkownika
echo "Usuwam kolejki komunikatów..."
ipcs -q | grep $(whoami) | awk '{print $2}' | while read msqid; do
    echo "Usuwam kolejkę komunikatów MSQID: $msqid"
    ipcrm -q $msqid 2>/dev/null
done

# Usuń pliki FIFO
if [ -e ./fifo_boat1 ]; then
    echo "Usuwam plik FIFO: ./fifo_boat1"
    rm -f ./fifo_boat1
fi
if [ -e ./fifo_boat2 ]; then
    echo "Usuwam plik FIFO: ./fifo_boat2"
    rm -f ./fifo_boat2
fi

echo "Wszystkie procesy i zasoby IPC zostały usunięte."

