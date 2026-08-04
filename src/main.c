/**
 * @file    main.c
 * @brief   主入口 — 主菜单路由
 * @details 负责登录/菜单分发，具体业务逻辑委托各模块。
 */

#include "types.h"
#include "platform.h"
#include "auth.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 占位函数（后续版本实现）
 * ============================================================ */

static void placeholder(const char* module) {
    printf("\n  [提示] %s 模块将在后续版本中实现。\n", module);
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
static void menu_inventory_alert(void)    { placeholder("库存预警"); }
static void menu_inventory_stocktake(void) { placeholder("库存盘点"); }

/* ---- 检索 ---- */
static void menu_search_material(void) { placeholder("耗材检索"); }
static void menu_search_record(void)   { placeholder("领用记录检索"); }

/* ---- 统计 ---- */
static void menu_stats(void)           { placeholder("数据统计"); }

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
        const char* role_name = (auth_current_role() == ROLE_ADMIN)
                                    ? "管理员" : "助教";
        printf("\n  [提示] 登录成功！欢迎 %s（%s）\n",
               auth_current_user(), role_name);
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
            printf("\n  [错误] 密码连续错误 %d 次，账号已锁定 %d 秒。\n",
                   MAX_LOGIN_ATTEMPTS, remaining);
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
        case 0: running = 0; break;
        case 1: menu_admin_manage_add();    break;
        case 2: menu_admin_manage_delete(); break;
        case 3: menu_admin_manage_chpwd();  break;
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
        case 13: menu_admin_manage();    break;
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
    setbuf(stdout, NULL);

    /* 确保数据目录存在 */
#ifdef _WIN32
    system("if not exist data mkdir data > nul");
#else
    system("mkdir -p data");
#endif

    /* 初始化认证模块 */
    if (auth_init() != 0) {
        fprintf(stderr, "[致命错误] 认证模块初始化失败，程序退出。\n");
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
            case 1: do_login(); break;
            case 0: running = 0; break;
            }
        } else if (auth_current_role() == ROLE_ADMIN) {
            menu_admin();
        } else {
            menu_ta();
        }
    }

    auth_shutdown();
    printf("\n  感谢使用，再见！\n");
    return 0;
}
