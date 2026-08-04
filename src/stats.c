/**
 * @file    stats.c
 * @brief   数据统计模块实现
 * @details 遍历 BorrowRecord 和 ScrapRecord 链表进行聚合计算。
 *          统计维度：月度消耗（金额/次数）、班级排行、逾期、报废成本。
 */

#include "stats.h"
#include "borrow.h"
#include "material.h"
#include "platform.h"
#include "types.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 月度消耗统计
 * ============================================================ */

static void stats_monthly(void) {
    /* 获取全部领用记录 */
    int total = 0;
    BorrowRecord *all = borrow_search("", "", "", &total);
    if (!all || total == 0) {
        printf("\n  [提示] 暂无领用记录。\n");
        return;
    }

    /* 按月份+分类聚合：
     * 用简单结构：month_key (YYYYMM) → category → {count, cost} */
    typedef struct {
        int year_month; /* 如 202608 */
        int category;
        int count;
        double cost;
    } MonthStat;

    MonthStat stats[200]; /* 预分配：最多 200 个 (月×类) 组合 */
    int stat_count = 0;

    for (int i = 0; i < total; i++) {
        struct tm *tm = localtime(&all[i].borrow_time);
        int ym = (tm->tm_year + 1900) * 100 + (tm->tm_mon + 1);

        const Material *mat = material_find_by_id(all[i].material_id);
        int cat = mat ? mat->category : -1;
        if (cat < 0)
            continue;

        double price = mat ? mat->unit_price : 0.0;

        /* 查找已有聚合项 */
        int found = -1;
        for (int j = 0; j < stat_count; j++) {
            if (stats[j].year_month == ym && stats[j].category == cat) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            stats[found].count++;
            stats[found].cost += price * all[i].quantity;
        } else if (stat_count < 200) {
            stats[stat_count].year_month = ym;
            stats[stat_count].category = cat;
            stats[stat_count].count = 1;
            stats[stat_count].cost = price * all[i].quantity;
            stat_count++;
        }
    }
    free(all);

    if (stat_count == 0) {
        printf("\n  [提示] 无有效统计数据。\n");
        return;
    }

    printf("\n");
    print_separator();
    printf("              月度耗材消耗统计\n");
    print_separator();
    printf("  %-8s %-12s %8s %12s\n", "月份", "分类", "次数", "金额(元)");
    print_line();

    double grand_total = 0.0;
    for (int i = 0; i < stat_count; i++) {
        int y = stats[i].year_month / 100;
        int m = stats[i].year_month % 100;
        printf("  %04d-%02d  %-12s %8d %12.2f\n", y, m, material_category_name(stats[i].category),
               stats[i].count, stats[i].cost);
        grand_total += stats[i].cost;
    }
    print_line();
    printf("  %-22s %8s %12.2f\n", "合计", "", grand_total);
    print_separator();
}

/* ============================================================
 * 班级用量排行
 * ============================================================ */

static void stats_class_ranking(void) {
    int total = 0;
    BorrowRecord *all = borrow_search("", "", "", &total);
    if (!all || total == 0) {
        printf("\n  [提示] 暂无领用记录。\n");
        return;
    }

    /* 按班级聚合 */
    typedef struct {
        char class_name[MAX_CLASS_NAME];
        int total_qty;
    } ClassStat;

    ClassStat stats[100];
    int stat_count = 0;

    for (int i = 0; i < total; i++) {
        /* 查找已有班级 */
        int found = -1;
        for (int j = 0; j < stat_count; j++) {
            if (strcmp(stats[j].class_name, all[i].class_name) == 0) {
                found = j;
                break;
            }
        }

        if (found >= 0) {
            stats[found].total_qty += all[i].quantity;
        } else if (stat_count < 100) {
            strncpy(stats[stat_count].class_name, all[i].class_name,
                    sizeof(stats[stat_count].class_name) - 1);
            stats[stat_count].total_qty = all[i].quantity;
            stat_count++;
        }
    }
    free(all);

    if (stat_count == 0) {
        printf("\n  [提示] 无有效统计数据。\n");
        return;
    }

    /* 按用量降序排序（冒泡） */
    for (int i = 0; i < stat_count - 1; i++) {
        for (int j = i + 1; j < stat_count; j++) {
            if (stats[j].total_qty > stats[i].total_qty) {
                ClassStat tmp = stats[i];
                stats[i] = stats[j];
                stats[j] = tmp;
            }
        }
    }

    printf("\n");
    print_separator();
    printf("              班级耗材用量排行榜\n");
    print_separator();
    printf("  %-4s %-20s %s\n", "排名", "班级", "总用量");
    print_line();

    for (int i = 0; i < stat_count; i++) {
        /* 前三名特殊标记 */
        const char *mark = "";
        if (i == 0)
            mark = "  ★ 冠军";
        else if (i == 1)
            mark = "  ▲ 亚军";
        else if (i == 2)
            mark = "  ◆ 季军";

        printf("  %-4d %-20s %d%s\n", i + 1, stats[i].class_name, stats[i].total_qty, mark);
    }
    print_line();
    printf("  共 %d 个班级\n", stat_count);
    print_separator();
}

/* ============================================================
 * 逾期统计
 * ============================================================ */

static void stats_overdue(void) {
    int count = 0;
    BorrowRecord *overdue = borrow_get_overdue_list(&count);

    if (!overdue || count == 0) {
        printf("\n  [提示] 当前没有逾期记录。\n");
        return;
    }

    /* 统计去重学生数 */
    int student_count = 0;
    typedef struct {
        char student_id[MAX_STUDENT_ID];
        char student_name[MAX_STUDENT_NAME];
    } StudentEntry;
    StudentEntry students[100];

    int tool_total = 0;

    for (int i = 0; i < count; i++) {
        /* 统计工具数量 */
        const Material *mat = material_find_by_id(overdue[i].material_id);
        if (mat && mat->attr == ATTR_REUSABLE) {
            tool_total += overdue[i].quantity;
        }

        /* 去重学生 */
        int dup = 0;
        for (int j = 0; j < student_count; j++) {
            if (strcmp(students[j].student_id, overdue[i].student_id) == 0) {
                dup = 1;
                break;
            }
        }
        if (!dup && student_count < 100) {
            strncpy(students[student_count].student_id, overdue[i].student_id,
                    sizeof(students[student_count].student_id) - 1);
            strncpy(students[student_count].student_name, overdue[i].student_name,
                    sizeof(students[student_count].student_name) - 1);
            student_count++;
        }
    }

    printf("\n");
    print_separator();
    printf("              逾期未归还统计\n");
    print_separator();
    printf("  逾期记录总数 : %d 条\n", count);
    printf("  逾期学生人数 : %d 人\n", student_count);
    printf("  逾期工具数量 : %d 件\n", tool_total);
    print_line();

    printf("  逾期学生名单:\n");
    for (int i = 0; i < student_count; i++) {
        printf("    %s %s\n", students[i].student_id, students[i].student_name);
    }

    print_separator();
    free(overdue);
}

/* ============================================================
 * 报废成本统计
 * ============================================================ */

static void stats_scrap_cost(void) {
    int count = 0;
    ScrapRecord *scraps = material_scrap_get_all(&count);

    if (!scraps || count == 0) {
        printf("\n  [提示] 暂无报废记录。\n");
        return;
    }

    /* 按分类聚合 */
    typedef struct {
        int category;
        int total_qty;
        double total_cost;
    } ScrapStat;

    ScrapStat stats[5] = {0}; /* 5 种分类 */
    double grand_cost = 0.0;
    int grand_qty = 0;

    for (int i = 0; i < count; i++) {
        int cat = -1;
        double price = 0.0;
        const Material *mat = material_find_by_id(scraps[i].material_id);
        if (mat) {
            cat = mat->category;
            price = mat->unit_price;
        } else {
            cat = 0; /* 未知归入电子元器件 */
        }

        stats[cat].total_qty += scraps[i].quantity;
        stats[cat].total_cost += price * scraps[i].quantity;
        grand_qty += scraps[i].quantity;
        grand_cost += price * scraps[i].quantity;
    }

    printf("\n");
    print_separator();
    printf("              报废耗材成本统计\n");
    print_separator();
    printf("  %-12s %8s %12s\n", "分类", "数量", "成本(元)");
    print_line();

    for (int i = 0; i < 5; i++) {
        if (stats[i].total_qty > 0) {
            printf("  %-12s %8d %12.2f\n", material_category_name(i), stats[i].total_qty,
                   stats[i].total_cost);
        }
    }
    print_line();
    printf("  %-12s %8d %12.2f\n", "合计", grand_qty, grand_cost);
    print_separator();

    free(scraps);
}

/* ============================================================
 * 统计主菜单
 * ============================================================ */

void stats_menu(void) {
    int running = 1;
    while (running) {
        print_title("数据统计");

        printf("  1. 月度耗材消耗统计\n");
        printf("  2. 班级耗材用量排行榜\n");
        printf("  3. 逾期未归还统计\n");
        printf("  4. 报废耗材成本统计\n");
        printf("  5. 全部统计概览\n");
        printf("  0. 返回\n");

        int choice = read_int("\n  请选择: ", 0, 5);

        switch (choice) {
        case 0:
            running = 0;
            break;
        case 1:
            print_title("月度耗材消耗统计");
            stats_monthly();
            break;
        case 2:
            print_title("班级耗材用量排行榜");
            stats_class_ranking();
            break;
        case 3:
            print_title("逾期未归还统计");
            stats_overdue();
            break;
        case 4:
            print_title("报废耗材成本统计");
            stats_scrap_cost();
            break;
        case 5: /* 全部概览 */
            print_title("统计概览");
            stats_monthly();
            printf("\n");
            stats_class_ranking();
            printf("\n");
            stats_overdue();
            printf("\n");
            stats_scrap_cost();
            break;
        }

        if (choice != 0)
            pause_screen();
    }
}
