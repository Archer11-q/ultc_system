/**
 * @file    platform.h
 * @brief   跨平台兼容层 — 封装 Windows / Linux 系统差异
 * @details 所有平台相关代码集中在此头文件及 platform.c 中。
 *          业务模块不直接调用系统 API，统一通过本层提供的接口。
 *          当前支持的平台差异：清屏、密码输入回显屏蔽、颜色输出。
 */

#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 字节序转换（主机序 ↔ 大端序）
 *
 * 手写实现，不依赖 winsock2.h / arpa/inet.h。
 * 16/32 位的 ntoh 与 hton 计算相同（按位对称翻转）。
 * ============================================================ */

/**
 * @brief 16 位整数转大端序
 */
static inline uint16_t host_to_be16(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }

/**
 * @brief 32 位整数转大端序
 */
static inline uint32_t host_to_be32(uint32_t v) {
    return ((v >> 24) & 0x000000FFu) | ((v >> 8) & 0x0000FF00u) | ((v << 8) & 0x00FF0000u) |
           ((v << 24) & 0xFF000000u);
}

/** 大端序 → 主机序（与 host→be 相同） */
#define be16_to_host(v) host_to_be16(v)
#define be32_to_host(v) host_to_be32(v)

/* ============================================================
 * 控制台操作
 * ============================================================ */

/** 清屏 */
static inline void clear_screen(void) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

/** 暂停等待用户按键 */
static inline void pause_screen(void) {
#ifdef _WIN32
    system("pause");
#else
    printf("按回车键继续...");
    /* 消费整行以避免管道输入时错位 */
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
#endif
}

/* ============================================================
 * 密码输入（跨平台实现见 platform.c）
 * ============================================================ */

/**
 * @brief 从控制台读取密码，回显为 '*'
 * @param buf    输出缓冲区
 * @param maxlen 缓冲区最大长度（含终止符）
 * @return 成功返回读入字符数，Ctrl+C 中断返回 -1
 */
int get_password(char *buf, int maxlen);

/* ============================================================
 * 控制台颜色输出（跨平台实现见 platform.c）
 * ============================================================ */

void set_color_red(void);
void set_color_green(void);
void set_color_yellow(void);
void reset_color(void);

#ifdef __cplusplus
}
#endif

#endif /* PLATFORM_H */
