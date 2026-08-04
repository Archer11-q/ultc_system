/**
 * @file    platform.c
 * @brief   跨平台兼容层实现 — 密码输入、控制台颜色
 * @details Windows 使用 _getch() + conio.h，
 *          Linux 使用 termios 关闭回显 + ICANON。
 */

#include "platform.h"

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include <stdio.h>

/* ============================================================
 * 密码输入（回显为 '*'）
 * ============================================================ */

int get_password(char *buf, int maxlen) {
    if (!buf || maxlen <= 0)
        return -1;

    int i = 0;

#ifdef _WIN32
    while (i < maxlen - 1) {
        int ch = _getch();
        if (ch == '\r' || ch == '\n')
            break;
        if (ch == '\b' || ch == 127) {
            if (i > 0) {
                i--;
                printf("\b \b");
            }
            continue;
        }
        if (ch == 3) { /* Ctrl+C */
            buf[0] = '\0';
            return -1;
        }
        buf[i++] = (char)ch;
        putchar('*');
    }
#else
    struct termios old_t, new_t;
    tcgetattr(STDIN_FILENO, &old_t);
    new_t = old_t;
    new_t.c_lflag &= (tcflag_t)(~(ECHO | ICANON));
    new_t.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_t);

    while (i < maxlen - 1) {
        int ch = getchar();
        if (ch == '\n' || ch == '\r')
            break;
        if (ch == 127 || ch == '\b') {
            if (i > 0) {
                i--;
                printf("\b \b");
            }
            continue;
        }
        if (ch == 3) {
            tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
            buf[0] = '\0';
            return -1;
        }
        buf[i++] = (char)ch;
        putchar('*');
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &old_t);
#endif

    buf[i] = '\0';
    printf("\n");
    return i;
}

/* ============================================================
 * 控制台颜色
 * ============================================================ */

void set_color_red(void) {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 12);
#else
    printf("\033[31m");
#endif
}

void set_color_green(void) {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 10);
#else
    printf("\033[32m");
#endif
}

void set_color_yellow(void) {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 14);
#else
    printf("\033[33m");
#endif
}

void reset_color(void) {
#ifdef _WIN32
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
#else
    printf("\033[0m");
#endif
}
