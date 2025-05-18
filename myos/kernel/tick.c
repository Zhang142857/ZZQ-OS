
unsigned int get_tick() {
    static unsigned int ticks = 0;
    return ticks++;
}
