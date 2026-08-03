/* Mock termios.h for embedded platforms without terminal I/O
 * (ESP32/STM32). quickjs-libc.c / mquickjs readline include
 * <termios.h> for raw-mode console handling which is unused on
 * these platforms. */
#ifndef YUI_MOCK_TERMIOS_H
#define YUI_MOCK_TERMIOS_H

typedef unsigned int tcflag_t;
typedef unsigned char cc_t;
typedef unsigned int speed_t;

struct termios {
    tcflag_t c_iflag;
    tcflag_t c_oflag;
    tcflag_t c_cflag;
    tcflag_t c_lflag;
    cc_t c_cc[32];
    speed_t c_ispeed;
    speed_t c_ospeed;
};

/* input flags */
#define IGNBRK 0x0001
#define BRKINT 0x0002
#define PARMRK 0x0004
#define ISTRIP 0x0008
#define INLCR  0x0010
#define IGNCR  0x0020
#define ICRNL  0x0040
#define IXON   0x0080

/* output flags */
#define OPOST 0x0001

/* local flags */
#define ECHO   0x0008
#define ECHONL 0x0010
#define ICANON 0x0100
#define IEXTEN 0x1000

/* control flags */
#define CSIZE 0x0030
#define PARENB 0x0100
#define CS8    0x0030

/* c_cc indices */
#define VMIN  6
#define VTIME 5

#define TCSANOW   0
#define TCSADRAIN 1
#define TCSAFLUSH 2

static inline int tcgetattr(int fd, struct termios* t) {
    (void)fd;
    (void)t;
    return -1;
}

static inline int tcsetattr(int fd, int a, const struct termios* t) {
    (void)fd;
    (void)a;
    (void)t;
    return -1;
}

static inline void cfmakeraw(struct termios* t) {
    (void)t;
}

#endif /* YUI_MOCK_TERMIOS_H */
