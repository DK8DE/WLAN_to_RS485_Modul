#pragma once

#include <stdbool.h>

void network_bridge_begin();
void network_bridge_start_tasks();

// TCP: Socket verbunden; UDP: Socket aktiv (Server gebunden / Client bereit)
bool network_bridge_link_up();
bool network_bridge_tcp_connected(); // Alias für Link (Abwärtskompatibilität)

const char* network_bridge_mode_name();
