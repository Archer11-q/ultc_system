/**
 * @file    material.c
 * @brief   耗材档案管理模块实现
 * @details 耗材链表 CRUD、报废记录管理、分页展示、文件持久化。
 *          文件路径：data/material.dat / data/scrap.dat
 */

#include "material.h"
#include "audit.h"
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

#define MATERIAL_FILE "data/material.dat"
#define SCRAP_FILE "data/scrap.dat"

/* ============================================================
 * 模块内部状态
 * ============================================================ */

static Material *g_mat_list = NULL;      /**< 耗材链表头      */
static ScrapRecord *g_scrap_list = NULL; /**< 报废记录链表头  */

/* ============================================================
 * 内部辅助 — 链表操作
 * ============================================================ */

/** 按编号查找耗材节点 */
static Material *find_mat(const char *id) {
    Material *p = g_mat_list;
    while (p) {
        if (strcmp(p->id, id) == 0)
            return p;
        p = p->next;
    }
    return NULL;
}

/** 追加耗材节点到链表尾 */
static void append_mat(Material *node) {
    node->next = NULL;
    if (!g_mat_list) {
        g_mat_list = node;
        return;
    }
    Material *p = g_mat_list;
    while (p->next)
        p = p->next;
    p->next = node;
}

/** 追加报废记录到链表尾 */
static void append_scrap(ScrapRecord *node) {
    node->next = NULL;
    if (!g_scrap_list) {
        g_scrap_list = node;
        return;
    }
    ScrapRecord *p = g_scrap_list;
    while (p->next)
        p = p->next;
    p->next = node;
}

/** 生成报废单号 SCRAP-YYYYMMDD-NNN */
static void gen_scrap_id(char *buf, size_t bufsz) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char date[16];
    strftime(date, sizeof(date), "%Y%m%d", tm);

    /* 统计今日已有的报废单数 */
    int today_count = 0;
    ScrapRecord *p = g_scrap_list;
    while (p) {
        char p_date[16];
        strftime(p_date, sizeof(p_date), "%Y%m%d", localtime(&p->scrap_time));
        if (strcmp(p_date, date) == 0)
            today_count++;
        p = p->next;
    }

    snprintf(buf, bufsz, "SCRAP-%s-%03d", date, today_count + 1);
}

/* ============================================================
 * 内部辅助 — 持久化
 * ============================================================ */

/** 链表 → 数组 → 写入文件 */
static int save_materials(void) {
    int count = 0;
    Material *p = g_mat_list;
    while (p) {
        count++;
        p = p->next;
    }

    if (count == 0) {
        remove(MATERIAL_FILE);
        return 0;
    }

    Material *arr = (Material *)malloc(sizeof(Material) * count);
    if (!arr)
        return -1;

    p = g_mat_list;
    for (int i = 0; i < count; i++) {
        arr[i] = *p;
        arr[i].next = NULL;
        p = p->next;
    }
    int ret = file_write_all(MATERIAL_FILE, arr, sizeof(Material), count);
    free(arr);
    return ret;
}

static int save_scraps(void) {
    int count = 0;
    ScrapRecord *p = g_scrap_list;
    while (p) {
        count++;
        p = p->next;
    }

    if (count == 0) {
        remove(SCRAP_FILE);
        return 0;
    }

    ScrapRecord *arr = (ScrapRecord *)malloc(sizeof(ScrapRecord) * count);
    if (!arr)
        return -1;

    p = g_scrap_list;
    for (int i = 0; i < count; i++) {
        arr[i] = *p;
        arr[i].next = NULL;
        p = p->next;
    }
    int ret = file_write_all(SCRAP_FILE, arr, sizeof(ScrapRecord), count);
    free(arr);
    return ret;
}

/** 从文件加载链表（通用） */
static void *load_list(const char *filename, size_t elem_size, int *out_count) {
    void *data = file_read_all(filename, elem_size, out_count);
    return data;
}

/* ============================================================
 * 模块生命周期
 * ============================================================ */

int material_init(void) {
    /* 加载耗材 */
    int count = 0;
    Material *mats = (Material *)load_list(MATERIAL_FILE, sizeof(Material), &count);
    if (count == -1) {
        fprintf(stderr, "[警告] 耗材数据文件损坏，将创建空库\n");
        remove(MATERIAL_FILE);
    } else if (mats && count > 0) {
        for (int i = 0; i < count; i++) {
            Material *node = (Material *)malloc(sizeof(Material));
            if (!node) {
                free(mats);
                return -1;
            }
            *node = mats[i];
            append_mat(node);
        }
        free(mats);
    }

    /* 加载报废记录 */
    count = 0;
    ScrapRecord *scraps = (ScrapRecord *)load_list(SCRAP_FILE, sizeof(ScrapRecord), &count);
    if (count == -1) {
        fprintf(stderr, "[警告] 报废记录文件损坏，将创建空记录\n");
        remove(SCRAP_FILE);
    } else if (scraps && count > 0) {
        for (int i = 0; i < count; i++) {
            ScrapRecord *node = (ScrapRecord *)malloc(sizeof(ScrapRecord));
            if (!node) {
                free(scraps);
                return -1;
            }
            *node = scraps[i];
            append_scrap(node);
        }
        free(scraps);
    }

    return 0;
}

void material_shutdown(void) {
    /* 释放耗材链表 */
    Material *mp = g_mat_list;
    while (mp) {
        Material *n = mp->next;
        free(mp);
        mp = n;
    }
    g_mat_list = NULL;

    /* 释放报废记录链表 */
    ScrapRecord *sp = g_scrap_list;
    while (sp) {
        ScrapRecord *n = sp->next;
        free(sp);
        sp = n;
    }
    g_scrap_list = NULL;
}

/* ============================================================
 * 耗材 CRUD
 * ============================================================ */

int material_add(const Material *mat) {
    if (!mat || mat->id[0] == '\0')
        return -2;

    /* 编号唯一校验 */
    if (find_mat(mat->id))
        return -1;

    Material *node = (Material *)malloc(sizeof(Material));
    if (!node)
        return -2;
    *node = *mat;
    node->next = NULL;

    append_mat(node);
    save_materials();
    audit_log(AUDIT_MAT_ADD, mat->id, mat->name, NULL);
    return 0;
}

int material_update(const Material *mat) {
    if (!mat)
        return -1;
    Material *target = find_mat(mat->id);
    if (!target)
        return -1;

    /* 覆盖除 id 和 next 之外的所有字段 */
    strncpy(target->name, mat->name, sizeof(target->name) - 1);
    target->category = mat->category;
    target->attr = mat->attr;
    target->unit_price = mat->unit_price;
    target->total_stock = mat->total_stock;
    target->min_stock = mat->min_stock;
    strncpy(target->cabinet, mat->cabinet, sizeof(target->cabinet) - 1);
    target->purchase_date = mat->purchase_date;

    save_materials();
    audit_log(AUDIT_MAT_EDIT, mat->id, mat->name, NULL);
    return 0;
}

int material_delete(const char *id) {
    if (!id)
        return -1;

    Material *prev = NULL;
    Material *curr = g_mat_list;
    while (curr) {
        if (strcmp(curr->id, id) == 0) {
            if (prev)
                prev->next = curr->next;
            else
                g_mat_list = curr->next;
            free(curr);
            save_materials();
            audit_log(AUDIT_MAT_DELETE, id, "删除耗材", NULL);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;
}

const Material *material_find_by_id(const char *id) { return find_mat(id); }

/* ============================================================
 * 库存操作
 * ============================================================ */

int material_reduce_stock(const char *id, int quantity) {
    if (!id || quantity <= 0)
        return -2;
    Material *mat = find_mat(id);
    if (!mat)
        return -2;
    if (mat->total_stock < quantity)
        return -1;

    mat->total_stock -= quantity;
    save_materials();
    return 0;
}

int material_increase_stock(const char *id, int quantity) {
    if (!id || quantity <= 0)
        return -1;
    Material *mat = find_mat(id);
    if (!mat)
        return -1;

    mat->total_stock += quantity;
    save_materials();
    return 0;
}

/* ============================================================
 * 报废管理
 * ============================================================ */

int material_scrap(const char *material_id, int quantity, const char *reason,
                   const char *operator_name) {
    if (!material_id || quantity <= 0 || !reason || !operator_name)
        return -2;

    Material *mat = find_mat(material_id);
    if (!mat)
        return -2;
    if (mat->total_stock < quantity)
        return -1;

    /* 扣减库存 */
    mat->total_stock -= quantity;

    /* 生成报废记录 */
    ScrapRecord *sr = (ScrapRecord *)calloc(1, sizeof(ScrapRecord));
    if (!sr)
        return -1;

    gen_scrap_id(sr->scrap_id, sizeof(sr->scrap_id));
    strncpy(sr->material_id, material_id, sizeof(sr->material_id) - 1);
    strncpy(sr->material_name, mat->name, sizeof(sr->material_name) - 1);
    sr->scrap_time = time(NULL);
    strncpy(sr->reason, reason, sizeof(sr->reason) - 1);
    sr->quantity = quantity;
    strncpy(sr->operator_name, operator_name, sizeof(sr->operator_name) - 1);

    append_scrap(sr);

    /* 持久化 */
    save_materials();
    save_scraps();
    audit_log(AUDIT_MAT_SCRAP, material_id, reason, operator_name);
    return 0;
}

ScrapRecord *material_scrap_get_all(int *out_count) {
    *out_count = 0;
    int total = 0;
    ScrapRecord *p = g_scrap_list;
    while (p) {
        total++;
        p = p->next;
    }
    if (total == 0)
        return NULL;

    ScrapRecord *arr = (ScrapRecord *)malloc(sizeof(ScrapRecord) * total);
    if (!arr)
        return NULL;

    p = g_scrap_list;
    for (int i = 0; i < total; i++) {
        arr[i] = *p;
        arr[i].next = NULL;
        p = p->next;
    }
    *out_count = total;
    return arr;
}

void material_scrap_list(int page, int *total_pages) {
    if (page < 1)
        page = 1;

    /* 统计总数 */
    int total = 0;
    ScrapRecord *p = g_scrap_list;
    while (p) {
        total++;
        p = p->next;
    }

    *total_pages = (total == 0) ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (page > *total_pages)
        page = *total_pages;

    if (total == 0) {
        printf("\n  [提示] 暂无报废记录。\n");
        return;
    }

    /* 打印表头 */
    printf("\n");
    printf("  %-22s %-12s %-18s %s\n", "报废单号", "耗材编号", "耗材名称", "数量");
    print_line();

    /* 跳转到当前页起始 */
    int start = (page - 1) * PAGE_SIZE;
    int idx = 0;
    int shown = 0;
    p = g_scrap_list;
    while (p && idx < start) {
        idx++;
        p = p->next;
    }
    while (p && shown < PAGE_SIZE) {
        printf("  %-22s %-12s %-18s %d\n", p->scrap_id, p->material_id, p->material_name,
               p->quantity);
        shown++;
        p = p->next;
    }

    print_line();
    printf("  第 %d/%d 页（共 %d 条）\n", page, *total_pages, total);
}

/* ============================================================
 * 查询与展示
 * ============================================================ */

int material_count(void) {
    int count = 0;
    Material *p = g_mat_list;
    while (p) {
        count++;
        p = p->next;
    }
    return count;
}

void material_list_page(int page, int *total_pages) {
    if (page < 1)
        page = 1;

    int total = material_count();
    *total_pages = (total == 0) ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (page > *total_pages)
        page = *total_pages;

    if (total == 0) {
        printf("\n  [提示] 暂无耗材记录。\n");
        return;
    }

    /* 打印表头 */
    printf("\n");
    printf("  %-12s %-20s %-10s %-6s %8s %6s %5s %-8s %s\n", "编号", "名称", "分类", "属性", "单价",
           "库存", "预警", "柜号", "采购日期");
    print_line();

    /* 跳转到当前页起始 */
    int start = (page - 1) * PAGE_SIZE;
    int idx = 0;
    int shown = 0;
    Material *p = g_mat_list;
    while (p && idx < start) {
        idx++;
        p = p->next;
    }
    while (p && shown < PAGE_SIZE) {
        /* 采购日期格式化为 YYYY-MM-DD */
        char date_buf[16];
        struct tm *tm = localtime(&p->purchase_date);
        strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", tm);

        /* 库存预警高亮 */
        int low_stock = (p->total_stock < p->min_stock);

        printf("  %-12s %-20s %-10s %-6s %8.2f ", p->id, p->name,
               material_category_name(p->category), material_attr_name(p->attr), p->unit_price);

        if (low_stock) {
            set_color_red();
            printf("%6d", p->total_stock);
            reset_color();
            printf(" %5d", p->min_stock);
        } else {
            printf("%6d %5d", p->total_stock, p->min_stock);
        }

        printf(" %-8s %s\n", p->cabinet, date_buf);

        shown++;
        p = p->next;
    }

    print_line();
    printf("  第 %d/%d 页（共 %d 条）", page, *total_pages, total);

    /* 低库存提示 */
    int low_count = 0;
    Material *cp = g_mat_list;
    while (cp) {
        if (cp->total_stock < cp->min_stock)
            low_count++;
        cp = cp->next;
    }
    if (low_count > 0) {
        set_color_yellow();
        printf("  ★ %d 种耗材低于预警库存", low_count);
        reset_color();
    }
    printf("\n");
}

const char *material_category_name(int category) {
    switch (category) {
    case CAT_ELECTRONIC:
        return "电子元器件";
    case CAT_TOOL:
        return "电工工具";
    case CAT_DEV_BOARD:
        return "开发板";
    case CAT_CHEMICAL:
        return "化学耗材";
    case CAT_MECHANICAL:
        return "机械零件";
    default:
        return "未知";
    }
}

const char *material_attr_name(int attr) {
    switch (attr) {
    case ATTR_DISPOSABLE:
        return "一次性";
    case ATTR_REUSABLE:
        return "可循环";
    default:
        return "未知";
    }
}

/* ============================================================
 * 全部导出
 * ============================================================ */

Material *material_get_all(int *out_count) {
    *out_count = 0;
    int total = 0;
    Material *p = g_mat_list;
    while (p) {
        total++;
        p = p->next;
    }
    if (total == 0)
        return NULL;

    Material *arr = (Material *)malloc(sizeof(Material) * total);
    if (!arr)
        return NULL;
    p = g_mat_list;
    for (int i = 0; i < total; i++) {
        arr[i] = *p;
        arr[i].next = NULL;
        p = p->next;
    }
    *out_count = total;
    return arr;
}

/* ============================================================
 * 模糊搜索
 * ============================================================ */

Material *material_search_by_name(const char *keyword, int *out_count) {
    *out_count = 0;
    if (!keyword || keyword[0] == '\0')
        return NULL;

    /* 统计匹配数 */
    int total = 0;
    Material *p = g_mat_list;
    while (p) {
        if (strstr(p->name, keyword))
            total++;
        p = p->next;
    }
    if (total == 0)
        return NULL;

    Material *arr = (Material *)malloc(sizeof(Material) * total);
    if (!arr)
        return NULL;

    int idx = 0;
    p = g_mat_list;
    while (p && idx < total) {
        if (strstr(p->name, keyword)) {
            arr[idx++] = *p;
        }
        p = p->next;
    }
    *out_count = total;
    return arr;
}

/* ============================================================
 * 库存预警与采购清单
 * ============================================================ */

void material_alert_print(void) {
    Material *p = g_mat_list;
    int count = 0;

    /* 先统计 */
    while (p) {
        if (p->total_stock < p->min_stock)
            count++;
        p = p->next;
    }

    if (count == 0) {
        printf("\n  [提示] 所有耗材库存充足，无需预警。\n");
        return;
    }

    printf("\n");
    printf("  %-12s %-20s %6s %5s %s\n", "编号", "名称", "库存", "预警", "存放柜号");
    print_line();

    p = g_mat_list;
    while (p) {
        if (p->total_stock < p->min_stock) {
            set_color_red();
            printf("  %-12s %-20s %6d %5d %s\n", p->id, p->name, p->total_stock, p->min_stock,
                   p->cabinet);
            reset_color();
        }
        p = p->next;
    }
    print_line();
    printf("  共 %d 种耗材低于预警库存\n", count);
}

void material_purchase_list(void) {
    /* 先统计预警项 */
    int count = 0;
    double total_cost = 0.0;
    Material *p = g_mat_list;
    while (p) {
        if (p->total_stock < p->min_stock)
            count++;
        p = p->next;
    }

    if (count == 0) {
        printf("\n  [提示] 所有耗材库存充足，无需采购。\n");
        return;
    }

    printf("\n");
    print_separator();
    printf("                    采 购 清 单\n");
    print_separator();
    printf("  %-12s %-20s %6s %5s %6s %8s %s\n", "编号", "名称", "库存", "预警", "建议采购", "单价",
           "预估金额");
    print_line();

    p = g_mat_list;
    while (p) {
        if (p->total_stock < p->min_stock) {
            int suggest = p->min_stock * 2 - p->total_stock;
            double cost = suggest * p->unit_price;
            total_cost += cost;

            printf("  %-12s %-20s %6d %5d %6d %8.2f %8.2f\n", p->id, p->name, p->total_stock,
                   p->min_stock, suggest, p->unit_price, cost);
        }
        p = p->next;
    }
    print_line();
    printf("  %63s %8.2f\n", "采购总金额: ￥", total_cost);
    print_separator();
}
