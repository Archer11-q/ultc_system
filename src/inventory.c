/**
 * @file    inventory.c
 * @brief   库存盘点模块实现
 * @details StocktakeLog 链表管理，盘点差异计算与修正，文件持久化。
 *          文件路径：data/stocktake.dat
 */

#include "inventory.h"
#include "material.h"
#include "audit.h"
#include "file_io.h"
#include "platform.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define STOCKTAKE_FILE "data/stocktake.dat"

static StocktakeLog* g_log_list = NULL;

/* ============================================================
 * 内部辅助
 * ============================================================ */

static void append_log(StocktakeLog* node) {
    node->next = NULL;
    if (!g_log_list) { g_log_list = node; return; }
    StocktakeLog* p = g_log_list;
    while (p->next) p = p->next;
    p->next = node;
}

static int save_logs(void) {
    int count = 0;
    StocktakeLog* p = g_log_list;
    while (p) { count++; p = p->next; }
    if (count == 0) { remove(STOCKTAKE_FILE); return 0; }

    StocktakeLog* arr = (StocktakeLog*)malloc(sizeof(StocktakeLog) * count);
    if (!arr) return -1;
    p = g_log_list;
    for (int i = 0; i < count; i++) {
        arr[i] = *p;
        arr[i].next = NULL;
        p = p->next;
    }
    int ret = file_write_all(STOCKTAKE_FILE, arr, sizeof(StocktakeLog), count);
    free(arr);
    return ret;
}

static void gen_log_id(char* buf, size_t bufsz) {
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    int count = inventory_log_count();
    strftime(buf, bufsz, "LOG-%Y%m%d-", tm);
    char num[16];
    snprintf(num, sizeof(num), "%03d", count + 1);
    strncat(buf, num, bufsz - strlen(buf) - 1);
}

/* ============================================================
 * 模块生命周期
 * ============================================================ */

int inventory_init(void) {
    int count = 0;
    StocktakeLog* arr = (StocktakeLog*)file_read_all(STOCKTAKE_FILE,
                                                      sizeof(StocktakeLog),
                                                      &count);
    if (count == -1) {
        fprintf(stderr, "[警告] 盘点日志文件损坏，将创建空记录\n");
        remove(STOCKTAKE_FILE);
        return 0;
    }
    if (!arr || count == 0) return 0;
    for (int i = 0; i < count; i++) {
        StocktakeLog* node = (StocktakeLog*)malloc(sizeof(StocktakeLog));
        if (!node) { free(arr); return -1; }
        *node = arr[i];
        append_log(node);
    }
    free(arr);
    return 0;
}

void inventory_shutdown(void) {
    save_logs();
    StocktakeLog* p = g_log_list;
    while (p) { StocktakeLog* n = p->next; free(p); p = n; }
    g_log_list = NULL;
}

/* ============================================================
 * 盘点操作
 * ============================================================ */

int inventory_stocktake_item(const char* material_id, int actual_stock,
                              const char* operator_name, int auto_correct) {
    if (!material_id || !operator_name || actual_stock < 0) return -999999;

    const Material* mat = material_find_by_id(material_id);
    if (!mat) return -999999;

    int book = mat->total_stock;
    int diff = actual_stock - book;

    /* 修正库存 */
    if (auto_correct && diff != 0) {
        if (diff > 0) {
            material_increase_stock(material_id, diff);
        } else {
            /* diff < 0: 账面比实际多，需要调减。
             * material 无直接 decrease 用 update 间接设定 */
            Material tmp = *mat;
            tmp.total_stock = actual_stock;
            material_update(&tmp);
        }
    }

    /* 生成盘点日志 */
    StocktakeLog* log = (StocktakeLog*)calloc(1, sizeof(StocktakeLog));
    if (!log) return diff;
    gen_log_id(log->log_id, sizeof(log->log_id));
    strncpy(log->material_id, material_id, sizeof(log->material_id) - 1);
    log->book_value   = book;
    log->actual_value = actual_stock;
    log->diff         = diff;
    strncpy(log->operator_name, operator_name, sizeof(log->operator_name) - 1);
    log->check_time   = time(NULL);
    append_log(log);
    save_logs();
    if (auto_correct && diff != 0) {
        audit_log(AUDIT_STOCKTAKE, material_id, "修正库存差异", operator_name);
    }
    return diff;
}

void inventory_log_page(int page, int* total_pages) {
    if (page < 1) page = 1;
    int total = inventory_log_count();
    *total_pages = (total == 0) ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (page > *total_pages) page = *total_pages;

    if (total == 0) {
        printf("\n  [提示] 暂无盘点日志。\n");
        return;
    }

    printf("\n");
    printf("  %-22s %-12s %6s %6s %6s %s\n",
           "日志编号", "耗材编号", "账面", "实际", "差异", "盘点时间");
    print_line();

    int start = (page - 1) * PAGE_SIZE;
    int idx = 0, shown = 0;
    StocktakeLog* p = g_log_list;
    while (p && idx < start) { idx++; p = p->next; }
    while (p && shown < PAGE_SIZE) {
        char time_buf[20];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M",
                 localtime(&p->check_time));
        printf("  %-22s %-12s %6d %6d ",
               p->log_id, p->material_id,
               p->book_value, p->actual_value);
        if (p->diff != 0) {
            set_color_red();
            printf("%+6d", p->diff);
            reset_color();
        } else {
            printf("%6d", p->diff);
        }
        printf(" %s\n", time_buf);
        shown++;
        p = p->next;
    }
    print_line();
    printf("  第 %d/%d 页（共 %d 条）\n", page, *total_pages, total);
}

int inventory_log_count(void) {
    int count = 0;
    StocktakeLog* p = g_log_list;
    while (p) { count++; p = p->next; }
    return count;
}
