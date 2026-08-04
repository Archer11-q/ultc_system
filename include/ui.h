/**
 * @file    ui.h
 * @brief   控制台界面工具 — 菜单、表格、输入封装
 * @details 提供分隔线、标题、格式化输入等通用 UI 函数。
 *          所有用户交互通过本模块完成，确保界面风格统一。
 */

#ifndef UI_H
#define UI_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 界面装饰
 * ============================================================ */

/** 打印双线分隔符 */
void print_separator(void);

/** 打印单线分隔符 */
void print_line(void);

/** 打印居中标题（带分隔线包围） */
void print_title(const char *title);

/** 打印子标题（无清屏） */
void print_subtitle(const char *subtitle);

/* ============================================================
 * 格式化输入
 * ============================================================ */

/**
 * @brief 读取整数
 * @param prompt 提示文本
 * @param min    最小值（含）
 * @param max    最大值（含）
 * @return 用户输入的有效整数
 */
int read_int(const char *prompt, int min, int max);

/**
 * @brief 读取浮点数
 * @param prompt 提示文本
 * @param min    最小值（含）
 * @param max    最大值（含）
 * @return 用户输入的有效浮点数
 */
double read_double(const char *prompt, double min, double max);

/**
 * @brief 读取字符串（去除尾部换行）
 * @param prompt 提示文本
 * @param buf    输出缓冲区
 * @param maxlen 缓冲区长度
 */
void read_string(const char *prompt, char *buf, int maxlen);

/**
 * @brief 确认操作（Y/N）
 * @param prompt 确认提示
 * @return 1=确认, 0=取消
 */
int confirm(const char *prompt);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
