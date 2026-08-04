/**
 * @file    ui.c
 * @brief   控制台界面工具实现
 * @details 分隔线、标题、格式化输入、确认对话框。
 */

#include "ui.h"
#include "platform.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 界面装饰
 * ============================================================ */

void print_separator(void) {
    printf("══════════════════════════════════════════════════════════════\n");
}

void print_line(void) {
    printf("──────────────────────────────────────────────────────────────\n");
}

void print_title(const char *title) {
    clear_screen();
    print_separator();
    printf("  %s\n", title);
    print_separator();
}

void print_subtitle(const char *subtitle) {
    printf("\n");
    print_line();
    printf("  %s\n", subtitle);
    print_line();
}

/* ============================================================
 * 输入辅助：跳过当前行剩余字符
 * ============================================================ */

static void flush_stdin(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
        /* 消耗缓冲区直到换行 */
    }
}

/* ============================================================
 * 读取整数
 * ============================================================ */

int read_int(const char *prompt, int min, int max) {
    int val;
    char buf[64];

    while (1) {
        printf("%s", prompt);
        if (!fgets(buf, sizeof(buf), stdin)) {
            /* stdin EOF（管道关闭或 Ctrl+D），终止程序 */
            exit(0);
        }

        /* 如果输入过长（没读完一行），清掉剩余字符 */
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] != '\n') {
            flush_stdin();
        }

        char *end = NULL;
        val = (int)strtol(buf, &end, 10);
        if (end == buf) {
            printf("  [提示] 请输入整数 (%d ~ %d)\n", min, max);
            continue;
        }

        /* 跳过尾部空白 */
        while (*end && isspace((unsigned char)*end))
            end++;
        if (*end != '\0') {
            printf("  [提示] 请输入整数 (%d ~ %d)\n", min, max);
            continue;
        }

        if (val < min || val > max) {
            printf("  [提示] 输入超出范围 (%d ~ %d)\n", min, max);
            continue;
        }
        break;
    }
    return val;
}

/* ============================================================
 * 读取浮点数
 * ============================================================ */

double read_double(const char *prompt, double min, double max) {
    double val;
    char buf[64];

    while (1) {
        printf("%s", prompt);
        if (!fgets(buf, sizeof(buf), stdin))
            continue;

        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] != '\n')
            flush_stdin();

        char *end = NULL;
        val = strtod(buf, &end);
        if (end == buf) {
            printf("  [提示] 请输入数字\n");
            continue;
        }

        while (*end && isspace((unsigned char)*end))
            end++;
        if (*end != '\0') {
            printf("  [提示] 请输入有效数字\n");
            continue;
        }

        if (val < min || val > max) {
            printf("  [提示] 输入超出范围 (%.2f ~ %.2f)\n", min, max);
            continue;
        }
        break;
    }
    return val;
}

/* ============================================================
 * 读取字符串
 * ============================================================ */

void read_string(const char *prompt, char *buf, int maxlen) {
    while (1) {
        printf("%s", prompt);
        if (!fgets(buf, maxlen, stdin))
            continue;

        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] != '\n') {
            flush_stdin();
        }

        /* 去除尾部换行符 */
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[--len] = '\0';
        }

        if (len == 0) {
            printf("  [提示] 输入不能为空\n");
            continue;
        }
        break;
    }
}

/* ============================================================
 * 确认操作
 * ============================================================ */

int confirm(const char *prompt) {
    char buf[8];
    printf("%s (y/n): ", prompt);
    if (!fgets(buf, sizeof(buf), stdin))
        return 0;
    return (buf[0] == 'y' || buf[0] == 'Y');
}
