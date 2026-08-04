/**
 * @file    borrow.c
 * @brief   学生领用 / 归还 / 逾期管理模块实现
 * @details BorrowRecord 链表管理、文件持久化、多条件筛选。
 *          文件路径：data/borrow.dat
 */

#include "borrow.h"
#include "file_io.h"
#include "platform.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 内部常量
 * ============================================================ */

#define BORROW_FILE "data/borrow.dat"

/* ============================================================
 * 模块内部状态
 * ============================================================ */

static BorrowRecord* g_borrow_list = NULL;

/* ============================================================
 * 内部辅助
 * ============================================================ */

static void append_borrow(BorrowRecord* node) {
    node->next = NULL;
    if (!g_borrow_list) { g_borrow_list = node; return; }
    BorrowRecord* p = g_borrow_list;
    while (p->next) p = p->next;
    p->next = node;
}

static int save_borrows(void) {
    int count = 0;
    BorrowRecord* p = g_borrow_list;
    while (p) { count++; p = p->next; }

    if (count == 0) { remove(BORROW_FILE); return 0; }

    BorrowRecord* arr = (BorrowRecord*)malloc(sizeof(BorrowRecord) * count);
    if (!arr) return -1;

    p = g_borrow_list;
    for (int i = 0; i < count; i++) {
        arr[i] = *p;
        arr[i].next = NULL;
        p = p->next;
    }
    int ret = file_write_all(BORROW_FILE, arr, sizeof(BorrowRecord), count);
    free(arr);
    return ret;
}

/**
 * @brief 将过滤结果收集到动态数组
 * @param predicate 过滤函数，返回 1 表示匹配
 * @param userdata  传给 predicate 的额外参数
 * @param out_count 输出参数
 * @return 匹配记录数组（调用方 free），无匹配返回 NULL
 */
typedef int (*borrow_predicate)(const BorrowRecord*, const void* userdata);

static BorrowRecord* collect_filtered(borrow_predicate pred,
                                       const void* userdata, int* out_count) {
    *out_count = 0;

    /* 第一遍：计数 */
    int total = 0;
    BorrowRecord* p = g_borrow_list;
    while (p) {
        if (pred(p, userdata)) total++;
        p = p->next;
    }

    if (total == 0) return NULL;

    /* 第二遍：收集 */
    BorrowRecord* arr = (BorrowRecord*)malloc(sizeof(BorrowRecord) * total);
    if (!arr) return NULL;

    int idx = 0;
    p = g_borrow_list;
    while (p && idx < total) {
        if (pred(p, userdata)) {
            arr[idx++] = *p;
        }
        p = p->next;
    }
    *out_count = total;
    return arr;
}

/* ---- 谓词函数 ---- */

static int pred_by_record_id(const BorrowRecord* r, const void* userdata) {
    return strcmp(r->record_id, (const char*)userdata) == 0 ? 1 : 0;
}

typedef struct {
    const char* student_id;
    int         exclude_returned;  /* 1 = 排除已归还 */
} StudentFilter;

static int pred_by_student(const BorrowRecord* r, const void* userdata) {
    const StudentFilter* f = (const StudentFilter*)userdata;
    if (strcmp(r->student_id, f->student_id) != 0) return 0;
    if (f->exclude_returned && r->status == BORROW_RETURNED) return 0;
    return 1;
}

static int pred_overdue(const BorrowRecord* r, const void* userdata) {
    (void)userdata;
    if (r->status != BORROW_ACTIVE) return 0;
    time_t now = time(NULL);
    double diff = difftime(now, r->borrow_time);
    return (diff > (double)(OVERDUE_DAYS * 86400)) ? 1 : 0;
}

typedef struct {
    const char* class_name;
    const char* student_id;
    const char* project_id;
} SearchFilter;

static int pred_search(const BorrowRecord* r, const void* userdata) {
    const SearchFilter* f = (const SearchFilter*)userdata;
    if (f->class_name[0] && strcmp(r->class_name, f->class_name) != 0) return 0;
    if (f->student_id[0] && strcmp(r->student_id, f->student_id) != 0) return 0;
    if (f->project_id[0] && strcmp(r->project_id, f->project_id) != 0) return 0;
    return 1;
}

/* ============================================================
 * 模块生命周期
 * ============================================================ */

int borrow_init(void) {
    int count = 0;
    BorrowRecord* arr = (BorrowRecord*)file_read_all(BORROW_FILE,
                                                      sizeof(BorrowRecord),
                                                      &count);
    if (count == -1) {
        fprintf(stderr, "[警告] 领用记录文件损坏，将创建空记录\n");
        remove(BORROW_FILE);
        return 0;
    }
    if (!arr || count == 0) return 0;

    for (int i = 0; i < count; i++) {
        BorrowRecord* node = (BorrowRecord*)malloc(sizeof(BorrowRecord));
        if (!node) { free(arr); return -1; }
        *node = arr[i];
        append_borrow(node);
    }
    free(arr);
    return 0;
}

void borrow_shutdown(void) {
    save_borrows();
    BorrowRecord* p = g_borrow_list;
    while (p) { BorrowRecord* n = p->next; free(p); p = n; }
    g_borrow_list = NULL;
}

/* ============================================================
 * 领用操作
 * ============================================================ */

void borrow_gen_id(char* buf, size_t bufsz) {
    time_t now = time(NULL);
    struct tm* tm = localtime(&now);
    char date[16];
    strftime(date, sizeof(date), "%Y%m%d", tm);

    /* 统计今日已有领用单 */
    int today = 0;
    BorrowRecord* p = g_borrow_list;
    while (p) {
        /* 从 record_id 中提取日期部分：BORROW-YYYYMMDD */
        if (strncmp(p->record_id + 7, date, 8) == 0) {
            today++;
            /* 同一领用单可能有多条记录，但单号相同只计一次
             * 简单方案：只统计单号前缀匹配的条数，除以预估每单条数取上限
             * 更精确的做法：跳过已见过的单号，这里用简单方案 */
        }
        p = p->next;
    }
    /* 简化：以总记录数估算，确保单号唯一 */
    int count = borrow_count();
    snprintf(buf, bufsz, "BORROW-%s-%03d", date, count + 1);
}

int borrow_create(const BorrowRecord* rec) {
    if (!rec || rec->record_id[0] == '\0') return -1;

    BorrowRecord* node = (BorrowRecord*)malloc(sizeof(BorrowRecord));
    if (!node) return -1;
    *node = *rec;
    node->next = NULL;

    append_borrow(node);
    save_borrows();
    return 0;
}

BorrowRecord* borrow_get_by_record_id(const char* record_id, int* out_count) {
    return collect_filtered(pred_by_record_id, record_id, out_count);
}

/* ============================================================
 * 归还操作
 * ============================================================ */

BorrowRecord* borrow_get_unreturned_by_student(const char* student_id,
                                                int* out_count) {
    StudentFilter f = { student_id, 1 };
    return collect_filtered(pred_by_student, &f, out_count);
}

int borrow_return(const char* record_id, const char* damage_note) {
    if (!record_id) return -1;

    BorrowRecord* p = g_borrow_list;
    while (p) {
        if (strcmp(p->record_id, record_id) == 0) {
            if (p->status == BORROW_RETURNED) return -2;

            /* 如果有损坏说明 → 转入报废流程 */
            if (damage_note && damage_note[0] != '\0') {
                p->status = BORROW_SCRAPPED;
                strncpy(p->damage_note, damage_note,
                        sizeof(p->damage_note) - 1);
            } else {
                p->status = BORROW_RETURNED;
            }
            p->return_time = time(NULL);
            save_borrows();
            return 0;
        }
        p = p->next;
    }
    return -1;
}

int borrow_return_session(const char* record_id, const char* damage_note) {
    if (!record_id) return -1;

    int returned = 0;
    int found_any = 0;
    BorrowRecord* p = g_borrow_list;

    while (p) {
        if (strcmp(p->record_id, record_id) == 0) {
            found_any = 1;
            if (p->status != BORROW_RETURNED) {
                if (damage_note && damage_note[0] != '\0') {
                    p->status = BORROW_SCRAPPED;
                    strncpy(p->damage_note, damage_note,
                            sizeof(p->damage_note) - 1);
                } else {
                    p->status = BORROW_RETURNED;
                }
                p->return_time = time(NULL);
                returned++;
            }
        }
        p = p->next;
    }

    if (!found_any) return -1;
    if (returned > 0) save_borrows();
    return returned;
}

BorrowRecord* borrow_get_overdue_list(int* out_count) {
    return collect_filtered(pred_overdue, NULL, out_count);
}

/* ============================================================
 * 查询
 * ============================================================ */

BorrowRecord* borrow_search(const char* class_name, const char* student_id,
                             const char* project_id, int* out_count) {
    SearchFilter f = { class_name ? class_name : "",
                       student_id ? student_id : "",
                       project_id ? project_id : "" };
    return collect_filtered(pred_search, &f, out_count);
}

int borrow_count(void) {
    int count = 0;
    BorrowRecord* p = g_borrow_list;
    while (p) { count++; p = p->next; }
    return count;
}

void borrow_list_page(int page, int* total_pages) {
    if (page < 1) page = 1;

    int total = borrow_count();
    *total_pages = (total == 0) ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (page > *total_pages) page = *total_pages;

    if (total == 0) {
        printf("\n  [提示] 暂无领用记录。\n");
        return;
    }

    printf("\n");
    printf("  %-22s %-10s %-10s %-12s %s %s\n",
           "领用单号", "学号", "姓名", "耗材编号", "数量", "状态");
    print_line();

    int start = (page - 1) * PAGE_SIZE;
    int idx = 0, shown = 0;
    BorrowRecord* p = g_borrow_list;
    while (p && idx < start) { idx++; p = p->next; }
    while (p && shown < PAGE_SIZE) {
        const char* status_str;
        switch (p->status) {
        case BORROW_ACTIVE:   status_str = "领用中"; break;
        case BORROW_RETURNED: status_str = "已归还"; break;
        case BORROW_OVERDUE:  status_str = "逾期";   break;
        case BORROW_SCRAPPED: status_str = "已报废"; break;
        default:              status_str = "未知";   break;
        }

        printf("  %-22s %-10s %-10s %-12s %d %s\n",
               p->record_id, p->student_id,
               p->student_name, p->material_id,
               p->quantity, status_str);
        shown++;
        p = p->next;
    }
    print_line();
    printf("  第 %d/%d 页（共 %d 条）\n", page, *total_pages, total);
}
