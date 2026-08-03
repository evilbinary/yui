/* Mock sys/ioctl.h for embedded platforms without terminal I/O
 * (ESP32/STM32). quickjs-libc.c includes <sys/ioctl.h> for TIOCGWINSZ
 * which is unused on these platforms. */
#ifndef YUI_MOCK_SYS_IOCTL_H
#define YUI_MOCK_SYS_IOCTL_H

#define TIOCGWINSZ 0x5413

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

static inline int ioctl(int fd, unsigned long request, ...) {
    (void)fd;
    (void)request;
    return -1;
}

#endif /* YUI_MOCK_SYS_IOCTL_H */
