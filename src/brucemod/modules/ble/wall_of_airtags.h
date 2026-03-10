#ifndef __WALL_OF_AIRTAGS_H__
#define __WALL_OF_AIRTAGS_H__

#include <Arduino.h>
#include <vector>

struct AirTagItem {
    String mac;
    int rssi;
    unsigned long lastSeen;
    String name;
};

extern std::vector<AirTagItem> airTagItems;

void wall_of_airtags_setup();

#endif
