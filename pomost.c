void pobierzCzas(char *b){
    int id = shmget(SHM_KEY_TIME, TIME_SIZE, 0666);
    if(id < 0){ 
        snprintf(b, TIME_SIZE, "??:??:??"); 
        return; 
    }
    char *c = (char*)shmat(id, NULL, 0);
    if(c == (char*)-1){ 
        snprintf(b, TIME_SIZE, "??:??:??"); 
        return; 
    }
    // Skopiuj tylko pierwszą część do pierwszego spacji, aby uniknąć dodatkowych danych
    strncpy(b, c, TIME_SIZE - 1);
    b[TIME_SIZE - 1] = '\0'; // Upewnij się, że string jest zakończony
    // Znajdź pierwszą spację i zakończ string tam
    char *space = strchr(b, ' ');
    if(space != NULL){
        *space = '\0';
    }
    shmdt(c);
}
