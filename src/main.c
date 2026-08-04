/**
 * @file    main.c
 * @brief   主入口 — 主菜单路由 + 全局状态
 * @details v0.1 阶段仅为菜单骨架，所有功能模块入口为占位函数，
 *          后续版本逐一替换为真实实现。
 */

#include "types.h"
#include "platform.h"
#include "file_io.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 全局状态
 * ============================================================ */

/** 当前登录用户名，空串表示未登录 */
static char g_current_user[MAX_USERNAME] = "";

/** 当前登录角色，-1 表示未登录 */
static int  g_current_role = -1;

/* ============================================================
 * 占位函数（后续版本实现）
 * ============================================================ */

static void placeholder(const char* module) {
    printf("\n  [提示] %s 模块将在后续版本中实现。\n", module);
    pause_screen();
}

static void do_login(void) {
    placeholder("登录认证");
}

static void do_logout(void) {
    g_current_user[0] = '\0';
    g_current_role = -1;
    printf("\n  [提示] 已退出登录。\n");
    pause_screen();
}

/* ---- 耗材管理 ---- */
static void menu_material_add(void)    { placeholder("新增耗材"); }
static void menu_material_edit(void)   { placeholder("修改耗材"); }
static void menu_material_delete(void) { placeholder("删除耗材"); }
static void menu_material_list(void)   { placeholder("耗材列表"); }

/* ---- 领用归还 ---- */
static void menu_borrow_new(void)      { placeholder("学生领用"); }
static void menu_borrow_return(void)   { placeholder("耗材归还"); }
static void menu_borrow_overdue(void)  { placeholder("逾期管理"); }

/* ---- 预警盘点 ---- */
static void menu_inventory_alert(void)   { placeholder("库存预警"); }
static void menu_inventory_stocktake(void) { placeholder("库存盘点"); }

/* ---- 检索 ---- */
static void menu_search_material(void) { placeholder("耗材检索"); }
static void menu_search_record(void)   { placeholder("领用记录检索"); }

/* ---- 统计 ---- */
static void menu_stats(void)           { placeholder("数据统计"); }

/* ============================================================
 * 管理员菜单
 * ============================================================ */

static void menu_admin(void) {
    int running = 1;
    while (running) {
        print_title("高校实验室实训耗材智能管理系统");
        printf("  当前用户: %s (管理员)\n\n", g_current_user);

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
        printf("  13. 管理员管理       14. 退出登录\n");
        printf("  0. 退出系统\n");

        int choice = read_int("\n  请选择: ", 0, 14);

        switch (choice) {
        case 0:  running = 0; do_logout(); break;
        case 1:  menu_material_add();    break;
        case 2:  menu_material_edit();   break;
        case 3:  menu_material_delete(); break;
        case 4:  menu_material_list();   break;
        case 5:  menu_borrow_new();      break;
        case 6:  menu_borrow_return();   break;
        case 7:  menu_borrow_overdue();  break;
        case 8:  menu_inventory_alert();   break;
        case 9:  menu_inventory_stocktake(); break;
        case 10: menu_search_material(); break;
        case 11: menu_search_record();   break;
        case 12: menu_stats();           break;
        case 13: placeholder("管理员管理"); break;
        case 14: do_logout();            break;
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
        printf("  当前用户: %s (助教 — 仅查询权限)\n\n", g_current_user);

        printf("  1. 耗材列表（分页）\n");
        printf("  2. 库存预警\n");
        printf("  3. 耗材检索\n");
        printf("  4. 领用记录检索\n");
        printf("  5. 数据统计\n");
        printf("\n  6. 退出登录\n");
        printf("  0. 退出系统\n");

        int choice = read_int("\n  请选择: ", 0, 6);

        switch (choice) {
        case 0: running = 0; do_logout(); break;
        case 1: menu_material_list();       break;
        case 2: menu_inventory_alert();     break;
        case 3: menu_search_material();     break;
        case 4: menu_search_record();       break;
        case 5: menu_stats();               break;
        case 6: do_logout();                break;
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
    /* 禁用 stdout 缓冲，确保 printf 立即输出 */
    setbuf(stdout, NULL);

    /* 确保数据目录存在 */
#ifdef _WIN32
    system("if not exist data mkdir data > nul");
#else
    system("mkdir -p data");
#endif

    /* 主循环 */
    int running = 1;
    while (running) {
        if (g_current_role == -1) {
            /* 未登录 */
            print_title("高校实验室实训耗材智能管理系统");
            printf("\n");
            printf("  1. 管理员登录\n");
            printf("  0. 退出系统\n");

            int choice = read_int("\n  请选择: ", 0, 1);
            switch (choice) {
            case 1: do_login(); break;
            case 0:
                running = 0;
                break;
            }
        } else if (g_current_role == ROLE_ADMIN) {
            menu_admin();
        } else {
            menu_ta();
        }
    }

    printf("\n  感谢使用，再见！\n");
    return 0;
}
