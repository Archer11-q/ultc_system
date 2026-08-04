/**
 * @file    csv_io.c
 * @brief   CSV 导入导出实现
 * @details 导入：逐行解析 CSV → 字段校验 → 批量入库。
 *          导出：遍历链表 → 写 CSV（UTF-8 BOM + 逗号分隔+引号转义）。
 */

#include "csv_io.h"
#include "audit.h"
#include "borrow.h"
#include "material.h"
#include "platform.h"
#include "types.h"
#include "ui.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 内部辅助
 * ============================================================ */

/** 写 UTF-8 BOM */
static void write_bom(FILE *fp) {
    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    fwrite(bom, 1, 3, fp);
}

/** CSV 字段转义 */
static void csv_write_field(FILE *fp, const char *field) {
    int need_quote = 0;
    if (strchr(field, ',') || strchr(field, '"') || strchr(field, '\n') || strchr(field, '\r')) {
        need_quote = 1;
    }
    if (need_quote) {
        fputc('"', fp);
        for (const char *c = field; *c; c++) {
            if (*c == '"')
                fputc('"', fp);
            fputc(*c, fp);
        }
        fputc('"', fp);
    } else {
        fprintf(fp, "%s", field);
    }
}

static void csv_write_int(FILE *fp, int v) { fprintf(fp, "%d", v); }

static void csv_write_double(FILE *fp, double v) { fprintf(fp, "%.2f", v); }

/** 去除首尾空白 */
static char *trim(char *s) {
    while (isspace((unsigned char)*s))
        s++;
    if (*s == 0)
        return s;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end))
        end--;
    end[1] = '\0';
    return s;
}

static int parse_category(const char *s) {
    if (strcmp(s, "电子元器件") == 0 || strcmp(s, "0") == 0)
        return CAT_ELECTRONIC;
    if (strcmp(s, "电工工具") == 0 || strcmp(s, "1") == 0)
        return CAT_TOOL;
    if (strcmp(s, "开发板") == 0 || strcmp(s, "2") == 0)
        return CAT_DEV_BOARD;
    if (strcmp(s, "化学耗材") == 0 || strcmp(s, "3") == 0)
        return CAT_CHEMICAL;
    if (strcmp(s, "机械零件") == 0 || strcmp(s, "4") == 0)
        return CAT_MECHANICAL;
    return -1;
}

static int parse_attr(const char *s) {
    if (strcmp(s, "一次性") == 0 || strcmp(s, "0") == 0)
        return ATTR_DISPOSABLE;
    if (strcmp(s, "可循环") == 0 || strcmp(s, "1") == 0)
        return ATTR_REUSABLE;
    return -1;
}

/** 解析一行 CSV */
static int csv_split_line(char *line, char fields[][128], int max_fields) {
    int count = 0;
    char *p = line;
    while (*p && count < max_fields) {
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0' || *p == '\n' || *p == '\r')
            break;
        char *out = fields[count];
        if (*p == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '"' && *(p + 1) == '"') {
                    *out++ = '"';
                    p += 2;
                } else {
                    *out++ = *p++;
                }
            }
            if (*p == '"')
                p++;
            *out = '\0';
        } else {
            while (*p && *p != ',' && *p != '\n' && *p != '\r')
                *out++ = *p++;
            *out = '\0';
            while (out > fields[count] && isspace((unsigned char)*(out - 1)))
                *--out = '\0';
        }
        count++;
        if (*p == ',')
            p++;
    }
    return count;
}

/* ============================================================
 * 导入
 * ============================================================ */

int csv_import_materials(const char *filepath, const char *operator_name) {
    if (!filepath || !operator_name)
        return -1;

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        printf("\n  [错误] 无法打开文件: %s\n", filepath);
        return -1;
    }

    int bom_check = fgetc(fp);
    if (bom_check != 0xEF)
        ungetc(bom_check, fp);
    else {
        fgetc(fp);
        fgetc(fp);
    }

    char line[1024];
    int line_no = 0;
    int imported = 0, skipped = 0, errors = 0;

    printf("\n");
    print_line();
    printf("  CSV 导入: %s\n", filepath);
    print_line();

    while (fgets(line, sizeof(line), fp)) {
        line_no++;
        char *trimmed = trim(line);
        if (trimmed[0] == '\0' || trimmed[0] == '#')
            continue;
        if (line_no == 1 && strstr(trimmed, "编号") && strstr(trimmed, "名称")) {
            printf("  [提示] 检测到标题行，已跳过。\n");
            continue;
        }

        char fields[9][128] = {{0}};
        int n = csv_split_line(line, fields, 9);
        if (n < 9) {
            printf("  [警告] 第%d行: 字段不足(%d列)，已跳过。\n", line_no, n);
            errors++;
            continue;
        }

        char *id = trim(fields[0]), *name = trim(fields[1]);
        char *cs = trim(fields[2]), *as = trim(fields[3]);
        char *ps = trim(fields[4]), *ss = trim(fields[5]);
        char *ms = trim(fields[6]), *cb = trim(fields[7]);
        char *ds = trim(fields[8]);

        if (id[0] == '\0' || name[0] == '\0') {
            printf("  [警告] 第%d行: 编号或名称为空。\n", line_no);
            errors++;
            continue;
        }
        if (material_find_by_id(id)) {
            printf("  [提示] 第%d行: '%s' 重复，已跳过。\n", line_no, id);
            skipped++;
            continue;
        }
        int cat = parse_category(cs);
        if (cat < 0) {
            printf("  [警告] 第%d行: 分类'%s'无效。\n", line_no, cs);
            errors++;
            continue;
        }
        int attr = parse_attr(as);
        if (attr < 0) {
            printf("  [警告] 第%d行: 属性'%s'无效。\n", line_no, as);
            errors++;
            continue;
        }
        double price = atof(ps);
        int stock = atoi(ss), min_stock = atoi(ms);
        if (price < 0 || stock < 0 || min_stock < 0) {
            printf("  [警告] 第%d行: 数值异常。\n", line_no);
            errors++;
            continue;
        }
        int y = 0, m = 0, d = 0;
        if (sscanf(ds, "%d-%d-%d", &y, &m, &d) != 3) {
            printf("  [警告] 第%d行: 日期格式无效。\n", line_no);
            errors++;
            continue;
        }
        struct tm tm = {0};
        tm.tm_year = y - 1900;
        tm.tm_mon = m - 1;
        tm.tm_mday = d;
        tm.tm_isdst = -1;

        Material mat;
        memset(&mat, 0, sizeof(mat));
        strncpy(mat.id, id, sizeof(mat.id) - 1);
        strncpy(mat.name, name, sizeof(mat.name) - 1);
        mat.category = cat;
        mat.attr = attr;
        mat.unit_price = price;
        mat.total_stock = stock;
        mat.min_stock = min_stock;
        strncpy(mat.cabinet, cb, sizeof(mat.cabinet) - 1);
        mat.purchase_date = mktime(&tm);

        if (material_add(&mat) == 0)
            imported++;
        else {
            errors++;
        }
    }
    fclose(fp);

    print_line();
    printf("  导入完成: 成功 %d, 跳过 %d, 错误 %d\n", imported, skipped, errors);
    print_line();

    char detail[128];
    snprintf(detail, sizeof(detail), "从 %s 导入 %d 条耗材", filepath, imported);
    audit_log(AUDIT_IMPORT, filepath, detail, operator_name);
    return imported;
}

/* ============================================================
 * 导出
 * ============================================================ */

int csv_export_materials(const char *filepath) {
    if (!filepath)
        return -1;
    int count = 0;
    Material *mats = material_get_all(&count);
    if (!mats || count == 0) {
        printf("\n  [提示] 耗材库为空。\n");
        return -1;
    }

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        free(mats);
        return -1;
    }
    write_bom(fp);

    fprintf(fp, "编号,名称,分类,属性,单价,库存,预警,柜号,采购日期\n");
    for (int i = 0; i < count; i++) {
        csv_write_field(fp, mats[i].id);
        fputc(',', fp);
        csv_write_field(fp, mats[i].name);
        fputc(',', fp);
        csv_write_field(fp, material_category_name(mats[i].category));
        fputc(',', fp);
        csv_write_field(fp, material_attr_name(mats[i].attr));
        fputc(',', fp);
        csv_write_double(fp, mats[i].unit_price);
        fputc(',', fp);
        csv_write_int(fp, mats[i].total_stock);
        fputc(',', fp);
        csv_write_int(fp, mats[i].min_stock);
        fputc(',', fp);
        csv_write_field(fp, mats[i].cabinet);
        fputc(',', fp);
        char date[16];
        strftime(date, sizeof(date), "%Y-%m-%d", localtime(&mats[i].purchase_date));
        csv_write_field(fp, date);
        fputc('\n', fp);
    }
    fclose(fp);
    free(mats);
    printf("\n  [提示] 已导出 %d 条耗材至 %s\n", count, filepath);
    return 0;
}

int csv_export_purchase_list(const char *filepath) {
    if (!filepath)
        return -1;
    int count = 0;
    Material *mats = material_get_all(&count);
    if (!mats || count == 0) {
        free(mats);
        return -1;
    }

    /* 筛选预警项 */
    int alert_count = 0;
    for (int i = 0; i < count; i++) {
        if (mats[i].total_stock < mats[i].min_stock)
            alert_count++;
    }
    if (alert_count == 0) {
        printf("\n  [提示] 无预警耗材，无需导出采购清单。\n");
        free(mats);
        return -1;
    }

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        free(mats);
        return -1;
    }
    write_bom(fp);

    fprintf(fp, "编号,名称,分类,当前库存,预警值,建议采购量,单价,预估金额\n");
    double total = 0.0;
    for (int i = 0; i < count; i++) {
        if (mats[i].total_stock < mats[i].min_stock) {
            int suggest = mats[i].min_stock * 2 - mats[i].total_stock;
            double cost = suggest * mats[i].unit_price;
            total += cost;

            csv_write_field(fp, mats[i].id);
            fputc(',', fp);
            csv_write_field(fp, mats[i].name);
            fputc(',', fp);
            csv_write_field(fp, material_category_name(mats[i].category));
            fputc(',', fp);
            csv_write_int(fp, mats[i].total_stock);
            fputc(',', fp);
            csv_write_int(fp, mats[i].min_stock);
            fputc(',', fp);
            csv_write_int(fp, suggest);
            fputc(',', fp);
            csv_write_double(fp, mats[i].unit_price);
            fputc(',', fp);
            csv_write_double(fp, cost);
            fputc('\n', fp);
        }
    }
    /* 汇总行 */
    fprintf(fp, ",,,,,,采购总金额:,");
    csv_write_double(fp, total);
    fputc('\n', fp);
    fclose(fp);
    free(mats);
    printf("\n  [提示] 已导出采购清单（%d 项，总金额 %.2f）至 %s\n", alert_count, total, filepath);
    return 0;
}

int csv_export_borrow_records(const char *filepath) {
    if (!filepath)
        return -1;
    int count = 0;
    BorrowRecord *recs = borrow_search("", "", "", &count);
    if (!recs || count == 0) {
        printf("\n  [提示] 无领用记录。\n");
        return -1;
    }

    FILE *fp = fopen(filepath, "w");
    if (!fp) {
        free(recs);
        return -1;
    }
    write_bom(fp);

    fprintf(fp, "领用单号,学号,姓名,班级,项目编号,耗材编号,数量,状态,领用时间,归还时间\n");
    for (int i = 0; i < count; i++) {
        csv_write_field(fp, recs[i].record_id);
        fputc(',', fp);
        csv_write_field(fp, recs[i].student_id);
        fputc(',', fp);
        csv_write_field(fp, recs[i].student_name);
        fputc(',', fp);
        csv_write_field(fp, recs[i].class_name);
        fputc(',', fp);
        csv_write_field(fp, recs[i].project_id);
        fputc(',', fp);
        csv_write_field(fp, recs[i].material_id);
        fputc(',', fp);
        csv_write_int(fp, recs[i].quantity);
        fputc(',', fp);

        const char *st;
        switch (recs[i].status) {
        case BORROW_ACTIVE:
            st = "领用中";
            break;
        case BORROW_RETURNED:
            st = "已归还";
            break;
        case BORROW_OVERDUE:
            st = "逾期";
            break;
        case BORROW_SCRAPPED:
            st = "已报废";
            break;
        default:
            st = "未知";
        }
        csv_write_field(fp, st);
        fputc(',', fp);

        char tbuf[20];
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", localtime(&recs[i].borrow_time));
        csv_write_field(fp, tbuf);
        fputc(',', fp);

        if (recs[i].return_time > 0) {
            strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", localtime(&recs[i].return_time));
            csv_write_field(fp, tbuf);
        }
        fputc('\n', fp);
    }
    fclose(fp);
    free(recs);
    printf("\n  [提示] 已导出 %d 条领用记录至 %s\n", count, filepath);
    return 0;
}
