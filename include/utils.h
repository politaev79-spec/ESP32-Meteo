#pragma once
#include <Arduino.h>

// Корректное экранирование строки для JSON (кавычки, слеш, спецсимволы).
static String jsonEscape(const String &in) {
    String o;
    o.reserve(in.length() + 8);
    for (size_t i = 0; i < in.length(); i++) {
        char c = in[i];
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if ((unsigned char)c < 0x20) {
                    char b[8];
                    snprintf(b, sizeof(b), "\\u%04x", (unsigned)(unsigned char)c);
                    o += b;
                } else {
                    o += c;
                }
        }
    }
    return o;
}
