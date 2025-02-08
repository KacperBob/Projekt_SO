#!/bin/bash

echo "Czyszczenie starych zasobów IPC..."
# Usuwamy kolejki i segmenty pamięci, jeśli istnieją
TICKET_KEY=1234
BOARDING_KEY=5678
DEPART_KEY=9012
SHM_KEY=2345


# Próba uzyskania identyfikatora kolejki komunikatów dla biletów.
# Jeśli kolejka istnieje, pobieramy jej id.
TICKET_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $TICKET_KEY) '$1==key {print $2}')
if [ ! -z "$TICKET_Q" ]; then
  # Usuwamy kolejkę biletową, jeśli została znaleziona.   
 ipcrm -q $TICKET_Q
fi

# Próba uzyskania identyfikatora kolejki komunikatów dla boardingu.
BOARDING_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $BOARDING_KEY) '$1==key {print $2}')
if [ ! -z "$BOARDING_Q" ]; then
 # Usuwamy kolejkę boardingową, jeśli została znaleziona.    
 ipcrm -q $BOARDING_Q
fi

# Próba uzyskania identyfikatora kolejki komunikatów dla depart.
DEPART_Q=$(ipcs -q | awk -v key=$(printf "0x%x" $DEPART_KEY) '$1==key {print $2}')
if [ ! -z "$DEPART_Q" ]; then
  # Usuwamy kolejkę depart, jeśli została znaleziona.  
  ipcrm -q $DEPART_Q
fi

# Próba uzyskania identyfikatora segmentu pamięci dzielonej dla stanu statków.
SHM_ID=$(ipcs -m | awk -v key=$(printf "0x%x" $SHM_KEY) '$1==key {print $2}')
if [ ! -z "$SHM_ID" ]; then
 # Usuwamy segment pamięci dzielonej, jeśli został znaleziony.   
 ipcrm -m $SHM_ID
fi

# Wyświetlenie komunikatu o kompilacji programów.
echo "Kompilacja programów..."
# Kompilacja poszczególnych modułów projektu przy użyciu gcc, z dołączonymi bibliotekami rt i pthread.
gcc -o pasazer pasazer.c -lrt -lpthread
gcc -o kasjer kasjer.c -lrt -lpthread
gcc -o pomost pomost.c -lrt -lpthread
gcc -o statek statek.c -lrt -lpthread
gcc -o sternik sternik.c -lrt -lpthread
gcc -o policja policja.c -lrt -lpthread
gcc -o manager_pasazerow manager_pasazerow.c -lrt -lpthread


# Wyświetlenie komunikatu o inicjalizacji IPC i uruchamianiu procesów.
echo "Inicjalizacja IPC i uruchamianie procesów..."

# Uruchomienie procesów w tle i zapisanie ich PID'ów.
./kasjer &
KASJER_PID=$!
./pomost &
POMOST_PID=$!
./sternik 1 &
STERNIK_PID=$!
./sternik 2 &
STERNIK_PID=$!
./statek 1 &
STATEK1_PID=$!
./statek 2 &
STATEK2_PID=$!
./policja $STATEK1_PID $STATEK2_PID &
POLICIA_PID=$!
./manager_pasazerow &
MANAGER_PID=$!

# Wyświetlenie komunikatów potwierdzających uruchomienie procesów wraz z ich PID
echo "Uruchomiono procesy:"
echo "kasjer: $KASJER_PID"
echo "pomost: $POMOST_PID"
echo "sternik: $STERNIK_PID"
echo "statek 1: $STATEK1_PID"
echo "statek 2: $STATEK2_PID"
echo "policja: $POLICIA_PID"
echo "manager_pasazerow: $MANAGER_PID"

# Wyświetlenie komunikatu o rozpoczęciu generowania pasażerów.
echo "Generowanie pasażerów (naciśnij Ctrl+C, aby zatrzymać)..."
while true; do
    ./pasazer &
    sleep 2
done
