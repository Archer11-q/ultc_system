/**
 * @file    main.c
 * @brief   主入口 — 主菜单路由
 * @details 负责登录/菜单分发，具体业务逻辑委托各模块。
 */

#include "audit.h"
#include "auth.h"
#include "borrow.h"
#include "csv_io.h"
#include "inventory.h"
#include "material.h"
#include "platform.h"
#include "search.h"
#include "stats.h"
#include "types.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 占位函数（后续版本实现）
 * ============================================================ */

/* ---- 耗材管理 ---- */

/** 输入分类 */
static int input_category(void) {
    printf("\n  分类: 1.电子元器件 2.电工工具 3.开发板 4.化学耗材 5.机械零件\n");
    int c = read_int("  请选择: ", 1, 5);
    return c - 1; /* 转为枚举值 0~4 */
}

/** 输入属性 */
static int input_attr(void) {
    printf("\n  属性: 1.一次性  2.可循环复用\n");
    int a = read_int("  请选择: ", 1, 2);
    return a - 1;
}

/** 输入日期（YYYY-MM-DD -> time_t） */
static time_t input_date(void) {
    char buf[16];
    read_string("  采购日期 (YYYY-MM-DD): ", buf, sizeof(buf));

    struct tm tm = {0};
    if (sscanf(buf, "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday) != 3) {
        return time(NULL); /* 解析失败用当前时间 */
    }
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

static void menu_material_add(void) {
    print_title("新增耗材");

    Material mat;
    memset(&mat, 0, sizeof(mat));

    read_string("  耗材编号: ", mat.id, sizeof(mat.id));
    if (material_find_by_id(mat.id)) {
        printf("\n  [错误] 编号 '%s' 已存在，请使用其他编号。\n", mat.id);
        pause_screen();
        return;
    }

    read_string("  耗材名称: ", mat.name, sizeof(mat.name));
    mat.category = input_category();
    mat.attr = input_attr();
    mat.unit_price = read_double("  采购单价: ", 0.00, 999999.99);
    mat.total_stock = read_int("  初始库存: ", 0, 999999);
    mat.min_stock = read_int("  最低预警库存: ", 0, mat.total_stock);
    read_string("  存放柜号: ", mat.cabinet, sizeof(mat.cabinet));
    mat.purchase_date = input_date();

    int ret = material_add(&mat);
    if (ret == 0) {
        printf("\n  [提示] 耗材 '%s' 添加成功。\n", mat.id);
    } else {
        printf("\n  [错误] 添加失败（%d）。\n", ret);
    }
    pause_screen();
}

static void menu_material_edit(void) {
    print_title("修改耗材");

    char id[MAX_MAT_ID];
    read_string("  要修改的耗材编号: ", id, sizeof(id));

    const Material *old = material_find_by_id(id);
    if (!old) {
        printf("\n  [错误] 耗材 '%s' 不存在。\n", id);
        pause_screen();
        return;
    }

    /* 显示当前值 */
    printf("\n  当前信息:\n");
    printf("  名称: %s | 库存: %d | 预警: %d | 柜号: %s | 单价: %.2f\n", old->name,
           old->total_stock, old->min_stock, old->cabinet, old->unit_price);

    Material mat = *old; /* 拷贝当前值作为默认 */

    printf("\n  （直接回车保留原值，输入 '-' 回车跳过名称）\n\n");

    char buf[128];
    read_string("  新名称: ", buf, sizeof(buf));
    if (buf[0] != '\0' && strcmp(buf, "-") != 0) {
        strncpy(mat.name, buf, sizeof(mat.name) - 1);
    }

    mat.total_stock = read_int("  新库存量: ", 0, 999999);
    mat.min_stock = read_int("  新预警值: ", 0, mat.total_stock);
    read_string("  新柜号: ", buf, sizeof(buf));
    if (buf[0] != '\0' && strcmp(buf, "-") != 0) {
        strncpy(mat.cabinet, buf, sizeof(mat.cabinet) - 1);
    }
    mat.unit_price = read_double("  新单价: ", 0.00, 999999.99);

    int ret = material_update(&mat);
    if (ret == 0) {
        printf("\n  [提示] 耗材 '%s' 修改成功。\n", id);
    } else {
        printf("\n  [错误] 修改失败。\n");
    }
    pause_screen();
}

static void menu_material_delete(void) {
    print_title("删除耗材");

    char id[MAX_MAT_ID];
    read_string("  要删除的耗材编号: ", id, sizeof(id));

    const Material *mat = material_find_by_id(id);
    if (!mat) {
        printf("\n  [错误] 耗材 '%s' 不存在。\n", id);
        pause_screen();
        return;
    }

    printf("\n  名称: %s | 库存: %d | 分类: %s\n", mat->name, mat->total_stock,
           material_category_name(mat->category));

    if (!confirm("\n  确认删除该耗材？"))
        return;

    int ret = material_delete(id);
    if (ret == 0) {
        printf("\n  [提示] 耗材 '%s' 已删除。\n", id);
    } else {
        printf("\n  [错误] 删除失败。\n");
    }
    pause_screen();
}

static void menu_material_list(void) {
    int page = 1;
    int total_pages = 1;
    int running = 1;

    while (running) {
        print_title("耗材列表（分页）");
        material_list_page(page, &total_pages);

        if (material_count() == 0) {
            printf("\n  [提示] 耗材库为空，请先新增耗材。\n");
            pause_screen();
            return;
        }

        printf("\n  [N]下一页  [P]上一页  [Q]返回");
        char buf[8];
        read_string("  → ", buf, sizeof(buf));

        switch (buf[0]) {
        case 'n':
        case 'N':
            if (page < total_pages) {
                page++;
            }
            break;
        case 'p':
        case 'P':
            if (page > 1) {
                page--;
            }
            break;
        case 'q':
        case 'Q':
            running = 0;
            break;
        }
    }
}

/* ---- 领用归还 ---- */

/** 打印单次领用回执 */
static void print_receipt(const char *record_id, const char *student_id, const char *student_name,
                          const char *class_name, const char *project_id) {
    int count = 0;
    BorrowRecord *items = borrow_get_by_record_id(record_id, &count);
    if (!items || count == 0)
        return;

    double total_value = 0.0;

    printf("\n");
    print_separator();
    printf("                    领 用 回 执\n");
    print_separator();
    printf("  领用单号 : %s\n", record_id);
    printf("  学    号 : %s\n", student_id);
    printf("  姓    名 : %s\n", student_name);
    printf("  班    级 : %s\n", class_name);
    printf("  实训项目 : %s\n", project_id);

    char time_buf[32];
    struct tm *tm = localtime(&items[0].borrow_time);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm);
    printf("  领用时间 : %s\n", time_buf);
    printf("  操作员   : %s\n", items[0].operator_name);
    print_line();
    printf("  %-4s %-16s %-20s %-8s %s\n", "序号", "耗材编号", "耗材名称", "数量", "类型");
    print_line();

    for (int i = 0; i < count; i++) {
        const Material *mat = material_find_by_id(items[i].material_id);
        const char *mat_name = mat ? mat->name : "（已删除）";
        const char *type = (mat && mat->attr == ATTR_DISPOSABLE) ? "一次性" : "可循环";
        double item_val = mat ? (mat->unit_price * items[i].quantity) : 0.0;
        total_value += item_val;

        printf("  %-4d %-16s %-20s %-8d %s\n", i + 1, items[i].material_id, mat_name,
               items[i].quantity, type);
    }
    print_line();
    printf("  合计价值: ￥%.2f\n", total_value);
    print_separator();

    free(items);
}

static void menu_borrow_new(void) {
    print_title("学生耗材领用");

    /* 检查是否有耗材可领 */
    if (material_count() == 0) {
        printf("\n  [提示] 耗材库为空，请先添加耗材。\n");
        pause_screen();
        return;
    }

    /* 1. 录入学生信息 */
    char student_id[MAX_STUDENT_ID];
    char student_name[MAX_STUDENT_NAME];
    char class_name[MAX_CLASS_NAME];
    char project_id[MAX_PROJECT_ID];

    printf("\n  ── 学生信息 ──\n");
    read_string("  学号: ", student_id, sizeof(student_id));
    read_string("  姓名: ", student_name, sizeof(student_name));
    read_string("  班级: ", class_name, sizeof(class_name));
    read_string("  实训项目编号: ", project_id, sizeof(project_id));

    /* 2. 生成领用单号 */
    char record_id[MAX_RECORD_ID];
    borrow_gen_id(record_id, sizeof(record_id));

    /* 3. 循环选择耗材 */
    int item_count = 0;
    int running = 1;
    while (running) {
        /* 显示耗材列表供选择 */
        int total_pages = 0;
        material_list_page(1, &total_pages);

        printf("\n  ── 领用单号: %s ──\n", record_id);
        printf("  已添加 %d 种耗材\n", item_count);

        printf("\n  1. 添加耗材到领用单\n");
        printf("  2. 完成领用\n");
        printf("  0. 取消领用\n");

        int choice = read_int("\n  请选择: ", 0, 2);
        if (choice == 0 || choice == 2) {
            running = 0;
        }
        if (choice == 0) {
            /* 取消：归还已扣减的库存（一次性耗材） */
            int cnt = 0;
            BorrowRecord *items = borrow_get_by_record_id(record_id, &cnt);
            if (items) {
                for (int i = 0; i < cnt; i++) {
                    const Material *mat = material_find_by_id(items[i].material_id);
                    if (mat && mat->attr == ATTR_DISPOSABLE) {
                        material_increase_stock(items[i].material_id, items[i].quantity);
                    }
                }
                free(items);
            }
            printf("\n  [提示] 领用已取消。\n");
            pause_screen();
            return;
        }
        if (choice == 2)
            break;

        /* 选择耗材 */
        if (choice != 1)
            continue;

        char mat_id[MAX_MAT_ID];
        int quantity;
        read_string("\n  耗材编号: ", mat_id, sizeof(mat_id));

        const Material *mat = material_find_by_id(mat_id);
        if (!mat) {
            printf("  [错误] 耗材 '%s' 不存在。\n", mat_id);
            pause_screen();
            continue;
        }

        printf("  耗材: %s | 分类: %s | 属性: %s | 库存: %d | 单价: %.2f\n", mat->name,
               material_category_name(mat->category), material_attr_name(mat->attr),
               mat->total_stock, mat->unit_price);

        quantity = read_int("  领用数量: ", 1, 99999);

        /* 规则校验 */
        if (mat->attr == ATTR_DISPOSABLE) {
            /* 一次性耗材：库存充足才可领用 */
            if (mat->total_stock < quantity) {
                set_color_red();
                printf("  [错误] 库存不足！当前库存 %d，需要 %d。\n", mat->total_stock, quantity);
                reset_color();
                pause_screen();
                continue;
            }
            /* 扣减库存 */
            material_reduce_stock(mat_id, quantity);
        }
        /* 可循环耗材：仅登记，不扣库存 */

        /* 创建领用记录 */
        BorrowRecord rec;
        memset(&rec, 0, sizeof(rec));
        strncpy(rec.record_id, record_id, sizeof(rec.record_id) - 1);
        strncpy(rec.student_id, student_id, sizeof(rec.student_id) - 1);
        strncpy(rec.student_name, student_name, sizeof(rec.student_name) - 1);
        strncpy(rec.class_name, class_name, sizeof(rec.class_name) - 1);
        strncpy(rec.project_id, project_id, sizeof(rec.project_id) - 1);
        strncpy(rec.material_id, mat_id, sizeof(rec.material_id) - 1);
        rec.quantity = quantity;
        rec.borrow_time = time(NULL);
        rec.status = BORROW_ACTIVE;
        strncpy(rec.operator_name, auth_current_user(), sizeof(rec.operator_name) - 1);

        borrow_create(&rec);
        item_count++;

        set_color_green();
        printf("  [提示] 已添加: %s × %d\n", mat->name, quantity);
        reset_color();
        pause_screen();
    }

    if (item_count == 0) {
        printf("\n  [提示] 未添加任何耗材，领用已取消。\n");
        pause_screen();
        return;
    }

    /* 4. 打印回执 */
    print_title("领用回执");
    print_receipt(record_id, student_id, student_name, class_name, project_id);
    pause_screen();
}

static void menu_borrow_return(void) {
    print_title("耗材归还");

    /* 输入学号 */
    char student_id[MAX_STUDENT_ID];
    read_string("  学号: ", student_id, sizeof(student_id));

    /* 列出该学生所有未归还记录 */
    int count = 0;
    BorrowRecord *items = borrow_get_unreturned_by_student(student_id, &count);
    if (!items || count == 0) {
        printf("\n  [提示] 该学生没有未归还的耗材。\n");
        pause_screen();
        return;
    }

    printf("\n  该学生有 %d 条未归还记录:\n", count);
    print_line();
    printf("  %-3s %-22s %-12s %-12s %-6s %s\n", "序号", "领用单号", "耗材编号", "耗材名称", "数量",
           "属性");
    print_line();

    for (int i = 0; i < count; i++) {
        const Material *mat = material_find_by_id(items[i].material_id);
        const char *mat_name = mat ? mat->name : "（已删除）";
        const char *attr_str = (mat && mat->attr == ATTR_REUSABLE) ? "可循环" : "一次性";

        /* 检查是否逾期 */
        double days = difftime(time(NULL), items[i].borrow_time) / 86400.0;
        const char *overdue_mark = (days > OVERDUE_DAYS) ? " ★逾期" : "";

        printf("  %-3d %-22s %-12s %-12s %-6d %s%s\n", i + 1, items[i].record_id,
               items[i].material_id, mat_name, items[i].quantity, attr_str, overdue_mark);
    }
    print_line();

    /* 选择要归还的领用单号 */
    printf("\n");
    char record_id[MAX_RECORD_ID];
    read_string("  输入要归还的领用单号: ", record_id, sizeof(record_id));

    /* 验证单号是否在未归还列表中 */
    int valid = 0;
    int is_reusable = 0;
    int quantity = 0;
    char mat_id[MAX_MAT_ID] = "";
    for (int i = 0; i < count; i++) {
        if (strcmp(items[i].record_id, record_id) == 0) {
            valid = 1;
            const Material *mat = material_find_by_id(items[i].material_id);
            if (mat && mat->attr == ATTR_REUSABLE) {
                is_reusable = 1;
                quantity = items[i].quantity;
                strncpy(mat_id, items[i].material_id, sizeof(mat_id) - 1);
            }
            break;
        }
    }

    if (!valid) {
        printf("\n  [错误] 领用单号不在未归还列表中。\n");
        free(items);
        pause_screen();
        return;
    }

    /* 一次性耗材无需归还（领用时已扣库存） */
    if (!is_reusable) {
        printf("\n  [提示] 该耗材为一次性耗材，领用时已扣减库存，无需归还。\n");
        printf("  系统将自动标记为已归还。\n");
        borrow_return_session(record_id, "");
        free(items);
        pause_screen();
        return;
    }

    /* 可循环耗材：询问是否损坏 */
    printf("\n  该耗材为可循环耗材，归还时请检查是否损坏。\n");
    if (confirm("  耗材是否有损坏？")) {
        char damage_note[MAX_DAMAGE_NOTE];
        read_string("  损坏情况说明: ", damage_note, sizeof(damage_note));

        /* 损坏 → 报废：扣库存 + 写报废记录 */
        if (mat_id[0] != '\0') {
            material_scrap(mat_id, quantity, damage_note, auth_current_user());
        }
        borrow_return_session(record_id, damage_note);
        set_color_yellow();
        printf("\n  [提示] 已登记损坏并转入报废台账，库存已扣减。\n");
        reset_color();
    } else {
        borrow_return_session(record_id, "");
        set_color_green();
        printf("\n  [提示] 归还成功，库存未变动。\n");
        reset_color();
    }

    free(items);
    pause_screen();
}

static void menu_borrow_overdue(void) {
    print_title("逾期管理");

    /* 先自动刷新逾期状态：将超 7 天且状态为 ACTIVE 的记录标记为 OVERDUE */
    int overdue_count = 0;
    BorrowRecord *overdue = borrow_get_overdue_list(&overdue_count);

    if (!overdue || overdue_count == 0) {
        printf("\n  [提示] 当前没有逾期未归还的记录。\n");
        pause_screen();
        return;
    }

    /* 逾期统计 */
    set_color_red();
    printf("\n  当前逾期记录: %d 条\n", overdue_count);
    reset_color();

    /* 统计逾期学生 */
    printf("\n  ── 逾期学生名单 ──\n");
    printf("  %-4s %-12s %-14s %-22s %s\n", "序号", "学号", "姓名", "领用单号", "逾期天数");
    print_line();

    /* 去重显示：按 (学号, 单号) 组合 */
    int shown = 0;
    for (int i = 0; i < overdue_count; i++) {
        /* 跳过已显示过的单号 */
        int dup = 0;
        for (int j = 0; j < i; j++) {
            if (strcmp(overdue[i].record_id, overdue[j].record_id) == 0 &&
                strcmp(overdue[i].student_id, overdue[j].student_id) == 0) {
                dup = 1;
                break;
            }
        }
        if (dup)
            continue;

        double days = difftime(time(NULL), overdue[i].borrow_time) / 86400.0;
        printf("  %-4d %-12s %-14s %-22s %.0f 天\n", ++shown, overdue[i].student_id,
               overdue[i].student_name, overdue[i].record_id, days);
    }
    print_line();
    printf("  共 %d 名学生存在逾期\n\n", shown);

    /* 统计可循环工具逾期数量 */
    int tool_overdue = 0;
    for (int i = 0; i < overdue_count; i++) {
        const Material *mat = material_find_by_id(overdue[i].material_id);
        if (mat && mat->attr == ATTR_REUSABLE) {
            tool_overdue += overdue[i].quantity;
        }
    }
    printf("  逾期未归还工具总计: %d 件\n", tool_overdue);

    free(overdue);
    pause_screen();
}

/* ---- 预警盘点 ---- */

static void menu_inventory_alert(void) {
    print_title("库存预警");

    if (material_count() == 0) {
        printf("\n  [提示] 耗材库为空。\n");
        pause_screen();
        return;
    }

    material_alert_print();

    printf("\n");
    if (confirm("  是否生成采购清单？")) {
        material_purchase_list();
    }
    pause_screen();
}

static void menu_inventory_stocktake(void) {
    print_title("库存盘点");

    if (material_count() == 0) {
        printf("\n  [提示] 耗材库为空，无需盘点。\n");
        pause_screen();
        return;
    }

    /* 先展示当前耗材概览 */
    int total_pages = 0;
    material_list_page(1, &total_pages);

    printf("\n  ── 盘点操作 ──\n");
    if (!confirm("\n  是否开始逐项盘点？"))
        return;

    /* 逐项盘点：需要遍历所有耗材。
     * 因为 material 模块不提供 get_all 迭代器，
     * 采用 "按编号输入" 的方式：操作员对照货架逐一输入。
     */
    int checked = 0;
    int corrected = 0;
    int running = 1;

    while (running) {
        printf("\n  ── 已盘点 %d 种耗材 ──\n", checked);

        char mat_id[MAX_MAT_ID];
        read_string("  耗材编号（输入 0 结束盘点）: ", mat_id, sizeof(mat_id));

        if (strcmp(mat_id, "0") == 0) {
            running = 0;
            break;
        }

        const Material *mat = material_find_by_id(mat_id);
        if (!mat) {
            printf("  [错误] 耗材 '%s' 不存在。\n", mat_id);
            continue;
        }

        printf("  耗材: %s | 账面库存: %d | 分类: %s\n", mat->name, mat->total_stock,
               material_category_name(mat->category));

        int actual = read_int("  实际库存: ", 0, 999999);

        int auto_correct = 0;
        if (actual != mat->total_stock) {
            set_color_yellow();
            printf("  差异: %+d（账面 %d → 实际 %d）\n", actual - mat->total_stock,
                   mat->total_stock, actual);
            reset_color();

            if (confirm("  是否用实际值修正账面库存？")) {
                auto_correct = 1;
                corrected++;
            }
        } else {
            printf("  库存一致，无需修正。\n");
        }

        int diff = inventory_stocktake_item(mat_id, actual, auth_current_user(), auto_correct);
        if (diff != -999999) {
            checked++;
            if (auto_correct) {
                set_color_green();
                printf("  [提示] 已修正，差异 %+d。\n", diff);
                reset_color();
            }
        }
    }

    printf("\n  [提示] 盘点完成。共盘点 %d 种耗材，修正 %d 项差异。\n", checked, corrected);

    /* 显示盘点日志 */
    if (checked > 0 && confirm("\n  是否查看本次盘点日志？")) {
        int log_pages = 0;
        inventory_log_page(1, &log_pages);
    }

    pause_screen();
}

/* ---- 检索 ---- */
static void menu_search_material(void) { search_material_menu(); }
static void menu_search_record(void) { search_record_menu(); }

/* ---- 统计 ---- */
static void menu_stats(void) { stats_menu(); }

/* ---- CSV 导入导出 ---- */

static void menu_csv_import(void) {
    print_title("CSV 批量导入耗材");

    printf("\n  CSV 格式: 编号,名称,分类,属性,单价,库存,预警,柜号,采购日期\n");
    printf("  分类支持: 电子元器件/电工工具/开发板/化学耗材/机械零件\n");
    printf("  属性: 一次性/可循环\n");
    printf("  日期: YYYY-MM-DD\n\n");

    char path[256];
    read_string("  CSV 文件路径: ", path, sizeof(path));
    csv_import_materials(path, auth_current_user());
    pause_screen();
}

static void menu_csv_export(void) {
    int running = 1;
    while (running) {
        print_title("CSV 导出");
        printf("\n  1. 导出全部耗材\n");
        printf("  2. 导出采购清单\n");
        printf("  3. 导出领用记录\n");
        printf("  0. 返回\n");

        int choice = read_int("\n  请选择: ", 0, 3);
        if (choice == 0) {
            running = 0;
            continue;
        }

        char path[256];
        char default_name[64];
        time_t now = time(NULL);
        char date[16];
        strftime(date, sizeof(date), "%Y%m%d", localtime(&now));

        switch (choice) {
        case 1:
            snprintf(default_name, sizeof(default_name), "materials_%s.csv", date);
            printf("  导出文件（回车= %s）: ", default_name);
            break;
        case 2:
            snprintf(default_name, sizeof(default_name), "purchase_%s.csv", date);
            printf("  导出文件（回车= %s）: ", default_name);
            break;
        case 3:
            snprintf(default_name, sizeof(default_name), "borrows_%s.csv", date);
            printf("  导出文件（回车= %s）: ", default_name);
            break;
        }

        read_string("", path, sizeof(path));
        if (path[0] == '\0')
            strncpy(path, default_name, sizeof(path) - 1);

        switch (choice) {
        case 1:
            csv_export_materials(path);
            break;
        case 2:
            csv_export_purchase_list(path);
            break;
        case 3:
            csv_export_borrow_records(path);
            break;
        }
        pause_screen();
    }
}

/* ---- 审计日志 ---- */

static void menu_audit_view(void) {
    int running = 1, page = 1, total_pages = 1;
    int filter_action = -1;
    char filter_user[MAX_USERNAME] = "";

    while (running) {
        print_title("操作审计日志");
        audit_list_page(page, &total_pages, filter_action, filter_user);

        if (audit_count() == 0) {
            printf("\n  [提示] 暂无审计日志。\n");
            pause_screen();
            return;
        }

        printf("\n  [N]下一页 [P]上一页 [F]筛选 [R]重置筛选 [Q]返回  → ");
        char buf[16];
        read_string("", buf, sizeof(buf));

        switch (buf[0]) {
        case 'n':
        case 'N':
            if (page < total_pages) {
                page++;
            }
            break;
        case 'p':
        case 'P':
            if (page > 1) {
                page--;
            }
            break;
        case 'f':
        case 'F': {
            printf("\n  操作类型（-1=全部, 2=新增耗材, 6=领用, 7=归还...）: ");
            filter_action = read_int("", -1, 12);
            printf("  操作者（回车=全部）: ");
            read_string("", filter_user, sizeof(filter_user));
            page = 1;
            break;
        }
        case 'r':
        case 'R':
            filter_action = -1;
            filter_user[0] = '\0';
            page = 1;
            break;
        default:
            running = 0;
            break;
        }
    }
}

/* ============================================================
 * 登录流程
 * ============================================================ */

static void do_login(void) {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];

    printf("\n");
    read_string("  用户名: ", username, sizeof(username));
    get_password(password, sizeof(password));

    int result = auth_login(username, password);

    switch (result) {
    case AUTH_OK: {
        const char *role_name = (auth_current_role() == ROLE_ADMIN) ? "管理员" : "助教";
        printf("\n  [提示] 登录成功！欢迎 %s（%s）\n", auth_current_user(), role_name);
        pause_screen();
        break;
    }
    case AUTH_USER_NOT_FOUND:
        printf("\n  [错误] 用户名不存在。\n");
        pause_screen();
        break;
    case AUTH_WRONG_PASSWORD: {
        /* 检查是否因本次错误导致了锁定 */
        int remaining = auth_lock_remaining(username);
        if (remaining > 0) {
            set_color_red();
            printf("\n  [错误] 密码连续错误 %d 次，账号已锁定 %d 秒。\n", MAX_LOGIN_ATTEMPTS,
                   remaining);
            reset_color();
        } else {
            printf("\n  [错误] 密码错误。\n");
        }
        pause_screen();
        break;
    }
    case AUTH_LOCKED: {
        int remaining = auth_lock_remaining(username);
        set_color_red();
        printf("\n  [错误] 账号已锁定，请 %d 秒后重试。\n", remaining);
        reset_color();
        pause_screen();
        break;
    }
    default:
        printf("\n  [错误] 未知错误（%d）。\n", result);
        pause_screen();
        break;
    }
}

static void do_logout(void) {
    printf("\n  [提示] 已退出登录（%s）。\n", auth_current_user());
    auth_logout();
    pause_screen();
}

/* ============================================================
 * 管理员管理子菜单（仅 ROLE_ADMIN）
 * ============================================================ */

static void menu_admin_manage_add(void) {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
    int role_choice;

    printf("\n");
    read_string("  新用户名: ", username, sizeof(username));
    get_password(password, sizeof(password));

    printf("\n  角色: 1. 管理员  2. 助教\n");
    role_choice = read_int("  请选择: ", 1, 2);
    int role = (role_choice == 1) ? ROLE_ADMIN : ROLE_TA;

    int ret = auth_add_admin(username, password, role);
    if (ret == AUTH_OK) {
        printf("\n  [提示] 管理员 '%s' 添加成功。\n", username);
    } else if (ret == AUTH_ALREADY_EXISTS) {
        printf("\n  [错误] 用户名 '%s' 已存在。\n", username);
    } else {
        printf("\n  [错误] 添加失败。\n");
    }
    pause_screen();
}

static void menu_admin_manage_delete(void) {
    auth_list_admins();

    char username[MAX_USERNAME];
    printf("\n");
    read_string("  要删除的用户名: ", username, sizeof(username));

    if (strcmp(username, auth_current_user()) == 0) {
        printf("\n  [错误] 不能删除当前登录的账号。\n");
        pause_screen();
        return;
    }

    if (!confirm("\n  确认删除该管理员？")) {
        return;
    }

    int ret = auth_delete_admin(username);
    if (ret == 0) {
        printf("\n  [提示] 管理员 '%s' 已删除。\n", username);
    } else {
        printf("\n  [错误] 删除失败（用户不存在）。\n");
    }
    pause_screen();
}

static void menu_admin_manage_chpwd(void) {
    char username[MAX_USERNAME];
    char new_password[MAX_PASSWORD];

    printf("\n");
    read_string("  目标用户名（回车=修改自己）: ", username, sizeof(username));

    if (username[0] == '\0') {
        strncpy(username, auth_current_user(), sizeof(username) - 1);
    }

    get_password(new_password, sizeof(new_password));

    int ret = auth_change_password(username, new_password);
    if (ret == 0) {
        printf("\n  [提示] 密码修改成功。\n");
    } else {
        printf("\n  [错误] 密码修改失败。\n");
    }
    pause_screen();
}

static void menu_admin_manage(void) {
    int running = 1;
    while (running) {
        print_title("管理员管理");
        auth_list_admins();

        printf("\n  1. 新增管理员\n");
        printf("  2. 删除管理员\n");
        printf("  3. 修改密码\n");
        printf("  0. 返回\n");

        int choice = read_int("\n  请选择: ", 0, 3);

        switch (choice) {
        case 0:
            running = 0;
            break;
        case 1:
            menu_admin_manage_add();
            break;
        case 2:
            menu_admin_manage_delete();
            break;
        case 3:
            menu_admin_manage_chpwd();
            break;
        }
    }
}

/* ============================================================
 * 管理员菜单
 * ============================================================ */

static void menu_admin(void) {
    int running = 1;
    while (running) {
        print_title("高校实验室实训耗材智能管理系统");
        printf("  当前用户: %s（管理员）\n\n", auth_current_user());

        printf("  ── 耗材管理 ──\n");
        printf("  1. 新增耗材          2. 修改耗材\n");
        printf("  3. 删除耗材          4. 耗材列表（分页）\n");
        printf("\n  ── 领用归还 ──\n");
        printf("  5. 学生领用          6. 耗材归还\n");
        printf("  7. 逾期管理\n");
        printf("\n  ── 库存 ──\n");
        printf("  8. 库存预警          9. 库存盘点\n");
        printf("\n  ── 检索与统计 ──\n");
        printf("  10. 耗材检索         11. 领用记录检索\n");
        printf("  12. 数据统计\n");
        printf("\n  ── 系统 ──\n");
        printf("  13. 管理员管理       14. CSV批量导入\n");
        printf("  15. CSV导出          16. 操作审计日志\n");
        printf("  17. 退出登录\n");
        printf("  0. 退出系统\n");

        int choice = read_int("\n  请选择: ", 0, 17);

        switch (choice) {
        case 0:
            running = 0;
            do_logout();
            break;
        case 1:
            menu_material_add();
            break;
        case 2:
            menu_material_edit();
            break;
        case 3:
            menu_material_delete();
            break;
        case 4:
            menu_material_list();
            break;
        case 5:
            menu_borrow_new();
            break;
        case 6:
            menu_borrow_return();
            break;
        case 7:
            menu_borrow_overdue();
            break;
        case 8:
            menu_inventory_alert();
            break;
        case 9:
            menu_inventory_stocktake();
            break;
        case 10:
            menu_search_material();
            break;
        case 11:
            menu_search_record();
            break;
        case 12:
            menu_stats();
            break;
        case 13:
            menu_admin_manage();
            break;
        case 14:
            menu_csv_import();
            break;
        case 15:
            menu_csv_export();
            break;
        case 16:
            menu_audit_view();
            break;
        case 17:
            do_logout();
            break;
        }
    }
}

/* ============================================================
 * 助教菜单（权限受限）
 * ============================================================ */

static void menu_ta(void) {
    int running = 1;
    while (running) {
        print_title("高校实验室实训耗材智能管理系统");
        printf("  当前用户: %s（助教 — 仅查询权限）\n\n", auth_current_user());

        printf("  1. 耗材列表（分页）\n");
        printf("  2. 库存预警\n");
        printf("  3. 耗材检索\n");
        printf("  4. 领用记录检索\n");
        printf("  5. 数据统计\n");
        printf("\n  6. 退出登录\n");
        printf("  0. 退出系统\n");

        int choice = read_int("\n  请选择: ", 0, 6);

        switch (choice) {
        case 0:
            running = 0;
            do_logout();
            break;
        case 1:
            menu_material_list();
            break;
        case 2:
            menu_inventory_alert();
            break;
        case 3:
            menu_search_material();
            break;
        case 4:
            menu_search_record();
            break;
        case 5:
            menu_stats();
            break;
        case 6:
            do_logout();
            break;
        }
    }
}

/* ============================================================
 * 主入口
 * ============================================================ */

int main(void) {
    /* Windows: 设置控制台 UTF-8 编码 */
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif
    setbuf(stdout, NULL);

    /* 确保数据目录存在 */
#ifdef _WIN32
    system("if not exist data mkdir data > nul");
#else
    system("mkdir -p data");
#endif

    /* 初始化各模块 */
    if (auth_init() != 0) {
        fprintf(stderr, "[致命错误] 认证模块初始化失败，程序退出。\n");
        return 1;
    }
    if (material_init() != 0) {
        fprintf(stderr, "[致命错误] 耗材模块初始化失败，程序退出。\n");
        auth_shutdown();
        return 1;
    }
    if (borrow_init() != 0) {
        fprintf(stderr, "[致命错误] 领用模块初始化失败，程序退出。\n");
        material_shutdown();
        auth_shutdown();
        return 1;
    }
    if (inventory_init() != 0) {
        fprintf(stderr, "[致命错误] 盘点模块初始化失败，程序退出。\n");
        borrow_shutdown();
        material_shutdown();
        auth_shutdown();
        return 1;
    }
    if (audit_init() != 0) {
        fprintf(stderr, "[致命错误] 审计模块初始化失败，程序退出。\n");
        inventory_shutdown();
        borrow_shutdown();
        material_shutdown();
        auth_shutdown();
        return 1;
    }

    /* 主循环 */
    int running = 1;
    while (running) {
        if (auth_current_role() == -1) {
            /* 未登录 */
            print_title("高校实验室实训耗材智能管理系统");
            printf("\n");
            printf("  1. 管理员登录\n");
            printf("  0. 退出系统\n");

            int choice = read_int("\n  请选择: ", 0, 1);
            switch (choice) {
            case 1:
                do_login();
                break;
            case 0:
                running = 0;
                break;
            }
        } else if (auth_current_role() == ROLE_ADMIN) {
            menu_admin();
        } else {
            menu_ta();
        }
    }

    audit_shutdown();
    inventory_shutdown();
    borrow_shutdown();
    material_shutdown();
    auth_shutdown();
    printf("\n  感谢使用，再见！\n");
    return 0;
}
