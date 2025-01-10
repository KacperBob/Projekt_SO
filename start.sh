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

    # Zatrzymaj wszystkie procesy pasażerów
    echo "Zatrzymuję procesy pasażerów..."
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
echo "Uruchamiam pomost..."
./pomost & echo $! > pomost.pid
echo "Uruchamiam statek..."
./statek & echo $! > statek.pid

# Uruchamianie pasażerów w nieskończonej pętli
echo "Uruchamiam pasażerów..."
while true; do
    ./pasazer &
    sleep $((RANDOM % 3 + 1)) # Losowy czas między uruchomieniem pasażerów
done
