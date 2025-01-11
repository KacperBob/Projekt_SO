#!/bin/bash

echo "Zatrzymuję procesy i usuwam zasoby IPC..."

terminate_process() {
    local pid_file=$1
    local process_name=$2

    if [ -f "$pid_file" ]; then
        PID=$(cat "$pid_file")
        if kill -0 "$PID" 2>/dev/null; then
            echo "Zatrzymuję $process_name (PID: $PID)..."
            kill -15 "$PID" 2>/dev/null
            sleep 1
            kill -9 "$PID" 2>/dev/null
        else
            echo "Proces $process_name już nie działa."
        fi
        rm -f "$pid_file"
    else
        echo "Nie znaleziono pliku PID dla procesu $process_name. Sprawdzam nazwę procesu..."
        PID=$(pgrep -f "./$process_name")
        if [ -n "$PID" ]; then
            echo "Zatrzymuję $process_name (PID: $PID)..."
            kill -15 "$PID" 2>/dev/null
            sleep 1
            kill -9 "$PID" 2>/dev/null
        else
            echo "Nie znaleziono procesu $process_name."
        fi
    fi
}

# Zatrzymywanie głównych procesów
terminate_process "nowyczas.pid" "nowyczas"
terminate_process "kasjer.pid" "kasjer"
terminate_process "pomost.pid" "pomost"
terminate_process "statek.pid" "statek"
terminate_process "sternik.pid" "sternik"

# Zabij wszystkie procesy pasażerów
echo "Zatrzymuję procesy pasażerów..."
pkill -f ./pasazer 2>/dev/null

# Usuwanie segmentów pamięci współdzielonej
echo "Usuwam segmenty pamięci współdzielonej..."
ipcs -m | grep $(whoami) | awk '{print $2}' | while read shmid; do
    nattch=$(ipcs -m -i "$shmid" | grep nattch | awk '{print $3}')
    if [ "$nattch" -eq 0 ]; then
        echo "Usuwam segment pamięci SHMID: $shmid"
        ipcrm -m "$shmid" 2>/dev/null || echo "Nie udało się usunąć segmentu pamięci SHMID: $shmid"
    else
        echo "Segment pamięci SHMID: $shmid jest nadal używany przez $nattch procesów, nie można usunąć."
    fi
done

# Usuwanie kolejek komunikatów
echo "Usuwam kolejki komunikatów..."
ipcs -q | grep $(whoami) | awk '{print $2}' | while read msqid; do
    echo "Usuwam kolejkę komunikatów MSQID: $msqid"
    ipcrm -q "$msqid" 2>/dev/null || echo "Nie udało się usunąć kolejki komunikatów MSQID: $msqid"
done

# Usuwanie plików FIFO
echo "Usuwam pliki FIFO..."
rm -f fifo_boat1 fifo_boat2

# Potwierdzenie zakończenia
echo "Wszystkie procesy i zasoby IPC zostały usunięte."
