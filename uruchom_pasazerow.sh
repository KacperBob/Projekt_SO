#!/bin/bash

# Liczba pasażerów do wygenerowania
LICZBA_PASAZEROW=10

# Uruchamianie pasażerów w losowych odstępach czasu
for ((i=1; i<=LICZBA_PASAZEROW; i++))
do
    ./pasazer &
    sleep $((RANDOM % 3 + 1)) # Losowy czas (1-3 sekundy) między pasażerami
done
