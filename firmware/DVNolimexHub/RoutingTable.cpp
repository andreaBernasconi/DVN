#include "RoutingTable.h"

// Routing table containing all known boards.
// Each entry defines:
// - prefix: short name used in OSC addressing
// - mac: MAC address of the board
// - enabled: whether the board should be used by the router
// - lastAlive: timestamp of the last received /alive message

RouteEntry routingTable[] = {
   {"scheda1", {0x68, 0xB6, 0xB3, 0x27, 0x29, 0xB8}, true, 0},
    {"scheda2", {0x68, 0xB6, 0xB3, 0x27, 0xBE, 0x98}, true, 0},
    {"scheda3", {0x68, 0xB6, 0xB3, 0x27, 0x28, 0x78}, true, 0},
    {"scheda4", {0x68, 0xB6, 0xB3, 0x27, 0xBE, 0xA0}, true, 0},
    {"scheda5", {0x68, 0xB6, 0xB3, 0x27, 0x26, 0xAC}, true, 0},
    {"scheda6", {0x68, 0xB6, 0xB3, 0x27, 0xBE, 0xA8}, true, 0},
    {"scheda7", {0x68, 0xB6, 0xB3, 0x27, 0x29, 0x70}, true, 0},
    {"scheda8", {0x68, 0xB6, 0xB3, 0x27, 0xBE, 0xE0}, true, 0},
    {"scheda9", {0x68, 0xB6, 0xB3, 0x27, 0xBE, 0x94}, true, 0},
    {"scheda10", {0x68, 0xB6, 0xB3, 0x27, 0x26, 0xB0}, true, 0}}; 

const int routingTableSize = sizeof(routingTable) / sizeof(RouteEntry);

RouteEntry *findRoute(const char *prefix)
{
    for (int i = 0; i < routingTableSize; i++)
    {
        if (routingTable[i].enabled && strcmp(prefix, routingTable[i].prefix) == 0)
        {
            return &routingTable[i];
        }
    }
    return nullptr;
}

const char *findPrefixByMac(const uint8_t *mac)
{
    for (int i = 0; i < routingTableSize; i++)
    {
        if (memcmp(mac, routingTable[i].mac, 6) == 0)
        {
            return routingTable[i].prefix;
        }
    }
    return nullptr;
}

RouteEntry *findRouteByMac(const uint8_t *mac)
{
    for (int i = 0; i < routingTableSize; i++)
    {
        if (memcmp(mac, routingTable[i].mac, 6) == 0)
        {
            return &routingTable[i];
        }
    }
    return nullptr;
}

bool findRouteSafe(const char *prefix, RouteEntry *&route, int &index)
{
    if (!prefix || prefix[0] == '\0')
        return false;

    for (int i = 0; i < routingTableSize; i++)
    {
        if (!routingTable[i].enabled)
            continue;
        if (strcmp(routingTable[i].prefix, prefix) == 0)
        {
            route = &routingTable[i];
            index = i;
            return true;
        }
    }

    return false;
}