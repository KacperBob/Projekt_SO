// shared.h
#ifndef SHARED_H
#define SHARED_H

#include <time.h> // Dodane dla typu time_t

#define SHM_KEY_TIME 5678
#define SHM_KEY_CAPACITY 8765
#define TIME_SIZE 20
#define MAX_SEATS_BOAT1 4 // Maksymalna liczba miejsc w łodzi 1
#define MAX_SEATS_BOAT2 4 // Maksymalna liczba miejsc w łodzi 2

struct boat_info {
    int passengers_on_bridge;
    int direction;       // 1 - wchodzenie, -1 - schodzenie
    int seats_available;
    time_t last_update;
};

#endif
