#!/bin/bash

# Funkcja zatrzymywania procesu
definitive_stop_process() {
    local process_name=$1
    local pid_file=$2

    if [ -f "$pid_file" ]; then
        PID=$(cat "$pid_file")
        if kill -0 "$PID" 2>/dev/null; then
            echo "Zatrzymuję $process_name (PID: $PID)..."
            kill -15 "$PID" 2>/dev/null
            sleep 2
            if kill -0 "$PID" 2>/dev/null; then
                echo "$process_name nie zakończył się po SIGTERM, wymuszam SIGKILL..."
                kill -9 "$PID" 2>/dev/null
            fi
        else
            echo "Proces $process_name już nie działa."
        fi
        rm -f "$pid_file"
    else
        echo "Brak pliku PID dla procesu $process_name."
    fi
}

# Zatrzymanie procesów
stop_all_processes() {
    echo "Zatrzymuję procesy i usuwam zasoby IPC..."

    definitive_stop_process "nowyczas" "nowyczas.pid"
    definitive_stop_process "kasjer" "kasjer.pid"
    definitive_stop_process "pomost" "pomost.pid"
    definitive_stop_process "statek" "statek.pid"

    # Zatrzymanie procesów pasażerów
    echo "Zatrzymuję procesy pasażerów..."
    pkill -f ./pasazer 2>/dev/null
    pkill -f ./pasazerowie 2>/dev/null

    # Usuwanie segmentów pamięci współdzielonej
    echo "Usuwam segmenty pamięci współdzielonej..."
    ipcs -m | grep $(whoami) | awk '{print $2}' | while read shmid; do
        ipcrm -m "$shmid" 2>/dev/null
    done

    # Usuwanie kolejek komunikatów
    echo "Usuwam kolejki komunikatów..."
    ipcs -q | grep $(whoami) | awk '{print $2}' | while read msqid; do
        ipcrm -q "$msqid" 2>/dev/null
    done

    # Usuwanie plików FIFO (jeśli istnieją)
    echo "Usuwam pliki FIFO..."
    rm -f fifo_boat1 fifo_boat2

    # Usuwanie wszystkich zasobów IPC bez informacji zwrotnej
    echo "Usuwam pozostałe zasoby IPC..."
    ipcrm -a 2>/dev/null

    # Potwierdzenie zakończenia
    echo "Wszystkie procesy i zasoby IPC zostały usunięte."
}

stop_all_processes
