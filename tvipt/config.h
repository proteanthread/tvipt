#ifndef _CONFIG_H
#define _CONFIG_H

// The wifi network joined automatically at boot.

#define DEFAULT_WIFI_SSID           NULL
#define DEFAULT_WIFI_PASSWORD       NULL
#define DEFAULT_WIFI_JOIN_TIMEOUT   5000

// The host to connect to using the tvipt protocol automatically at boot.

#define DEFAULT_HOST                NULL
#define DEFAULT_PORT                3333

// The Chacha20 secret key for tvipt protocol (change this to a random 32-byte sequence).

#define SECRET_KEY                  { 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00, 0x11, 0x00 }

#endif
