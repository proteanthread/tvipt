// Functions to write to the TeleVideo Personal Terminal.

#ifndef _TERM_H
#define _TERM_H

#include <Print.h>
#include <Arduino.h>
#include <SPI.h>
#include <WiFi101.h> 

#define TERM_BREAK    0
// https://en.wikipedia.org/wiki/Software_flow_control
#define TERM_XOFF               0x13
#define TERM_XON                0x11
#define TERM_ESCAPE             0x1B

#define TERM_CLEAR_TO_SPACES    '+'
#define TERM_ENABLE_ALT_CHAR    'J'
#define TERM_DISABLE_ALT_CHAR   'K'
#define TERM_MOVE_TO_POS        '=' // r c

// Serial is the USB connection's virtual serial port (use the Arduino serial monitor
// or another communications program on the host to see debug output as tvipt commands
// run).
#define dbg_serial    Serial

enum readln_echo {
    READLN_ECHO,
    READLN_NO_ECHO,
    READLN_MASKED,
};

void term_init();

void term_loop();

bool term_available();

void term_clear();

void term_write(const char c);

void term_write(const uint8_t *buf, size_t size);

void term_write(const char *buf, size_t size);

void term_write(const char *val);

void term_writeln();

void term_writeln(const char *val);

void term_write_masked(const char *val);

void term_writeln_masked(const char *val);

void term_write(byte row, byte col, char *value, size_t width);

void term_write(byte row, byte col, char *value);

void term_print(long val, int base = DEC);

void term_println(long val, int base = DEC);

void term_print(const IPAddress & addr);

char term_read();

int term_readln(char *buf, int max, readln_echo echo);

void term_move(byte row, byte col);

#endif
