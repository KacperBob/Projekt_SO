#!/bin/bash

# Funkcja czyszcząca procesy i pliki PID po zakończeniu skryptu
cleanup() {
    echo "Zatrzymuję procesy..."

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

    # Zatrzymaj wszystkie procesy pasażera
    echo "Zatrzymuję procesy pasażera..."
    pkill -f ./pasazer 2>/dev/null

    echo "Wszystkie procesy zostały zatrzymane."
    exit 0
}

# Rejestrowanie obsługi sygnałów (SIGINT i SIGTERM)
trap cleanup SIGINT SIGTERM

# Uruchamianie głównych procesów
echo "Uruchamiam nowyczas..."
./nowyczas & echo $! > nowyczas.pid

echo "Uruchamiam kasjer..."
./kasjer & echo $! > kasjer.pid

# Uruchamianie pasażerów w nieskończonej pętli
echo "Uruchamiam pasażerów..."
while true; do
    ./pasazer &
    sleep $((RANDOM % 3 + 1)) # Losowy czas między uruchomieniem pasażerów
done
