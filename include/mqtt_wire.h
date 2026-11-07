#pragma once
#include <stdbool.h>
#include <stddef.h>

/* Consume exactly one MQTT frame. Never assume TCP preserves packet boundaries. */
static inline int mqtt_read_frame(int fd, unsigned char *buffer, size_t capacity,
                                  bool (*read_all)(int, unsigned char *, size_t)) {
    if (capacity < 2 || !read_all(fd, buffer, 1)) return -1;
    size_t used = 1, remaining = 0, multiplier = 1;
    for (unsigned i = 0; i < 4; ++i) {
        if (used >= capacity || !read_all(fd, buffer + used, 1)) return -1;
        unsigned char digit = buffer[used++];
        remaining += (digit & 127) * multiplier;
        if (!(digit & 128)) {
            if (remaining > capacity - used) return -1;
            if (remaining && !read_all(fd, buffer + used, remaining)) return -1;
            return (int)(used + remaining);
        }
        multiplier *= 128;
    }
    return -1;
}
