#!/bin/bash

# Wyświetlenie komunikatu o rozpoczęciu zatrzymywania procesów symulacji.
echo "Zatrzymywanie procesów..."

# Zabijanie (siłą, sygnałem SIGKILL) wszystkich procesów związanych z symulacją:
# - pasazer – procesy pasażerów
# - kasjer – proces kasjera
# - pomost – proces pomostu
# - statek – procesy statków (oba statki mają nazwę "statek")
# - sternik – proces sternika
# - policja – proces policji
# - manager_pasazerow – proces zarządzania pasażerami
# - start.sh – sam skrypt startowy
pkill -9 pasazer
pkill -9 kasjer
pkill -9 pomost
pkill -9 statek
pkill -9 sternik
pkill -9 policja
pkill -9 manager_pasazerow
pkill -9 start.sh

# Krótkie opóźnienie, aby procesy miały czas na zakończenie.
sleep 1

# Wyświetlenie komunikatu o rozpoczęciu usuwania zasobów IPC.
echo "Usuwanie zasobów IPC..."

# Ustawienie kluczy dla kolejki komunikatów i segmentu pamięci dzielonej.
TICKET_KEY=1234
BOARDING_KEY=5678
DEPART_KEY=9012
SHM_KEY=2345

# Próba zlokalizowania identyfikatora kolejki biletów.
# Używamy polecenia ipcs -q wraz z awk, aby znaleźć kolejkę o zadanym kluczu.
TICKET_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $TICKET_KEY) '$1==key {print $2}')
if [ ! -z "$TICKET_Q" ]; then
    # Jeśli kolejka została znaleziona, usuwamy ją przy użyciu ipcrm.
    ipcrm -q $TICKET_Q
    echo "Usunięto kolejkę ticket (id: $TICKET_Q)"
fi

# Próba zlokalizowania identyfikatora kolejki boarding.
BOARDING_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $BOARDING_KEY) '$1==key {print $2}')
if [ ! -z "$BOARDING_Q" ]; then
    ipcrm -q $BOARDING_Q
    echo "Usunięto kolejkę boarding (id: $BOARDING_Q)"
fi

# Próba zlokalizowania identyfikatora kolejki depart.
DEPART_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $DEPART_KEY) '$1==key {print $2}')
if [ ! -z "$DEPART_Q" ]; then
    ipcrm -q $DEPART_Q
    echo "Usunięto kolejkę depart (id: $DEPART_Q)"
fi

# Próba zlokalizowania identyfikatora segmentu pamięci dzielonej używanego przez statki.
SHM_ID=$(ipcs -m | awk -v key=$(printf "0x%x" $SHM_KEY) '$1==key {print $2}')
if [ ! -z "$SHM_ID" ]; then
    # Jeśli segment pamięci został znaleziony, usuwamy go przy użyciu ipcrm.
    ipcrm -m $SHM_ID
    echo "Usunięto pamięć dzieloną (id: $SHM_ID)"
fi

# Dodatkowo wykonujemy polecenie ipcrm -a, które usuwa wszystkie zasoby IPC.
# Wyjście przekierowujemy do /dev/null, aby nie wyświetlać komunikatów.
ipcrm -a > /dev/null 2>&1

# Wyświetlenie komunikatu o zakończeniu procesu zatrzymywania.
echo "Stop zakończony."
