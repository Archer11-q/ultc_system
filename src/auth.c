/**
 * @file    auth.c
 * @brief   登录认证模块实现
 * @details 管理员链表管理、密码校验、锁定/解锁、文件持久化。
 *          文件路径：data/admin.dat
 */

#include "auth.h"
#include "file_io.h"
#include "audit.h"
#include "platform.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 内部常量
 * ============================================================ */

#define ADMIN_FILE      "data/admin.dat"
#define DEFAULT_USER    "admin"
#define DEFAULT_PASS    "admin123"

/* ============================================================
 * 模块内部状态
 * ============================================================ */

static Admin* g_admin_list = NULL;      /**< 管理员链表头       */
static char  g_cur_user[MAX_USERNAME];  /**< 当前登录用户名     */
static int   g_cur_role = -1;           /**< 当前登录角色       */

/* ============================================================
 * 内部辅助
 * ============================================================ */

/**
 * @brief 在链表中按用户名查找管理员
 * @return 找到返回节点指针，否则 NULL
 */
static Admin* find_admin(const char* username) {
    Admin* p = g_admin_list;
    while (p) {
        if (strcmp(p->username, username) == 0) return p;
        p = p->next;
    }
    return NULL;
}

/**
 * @brief 链表尾部追加管理员节点
 */
static void append_admin(Admin* node) {
    node->next = NULL;
    if (!g_admin_list) {
        g_admin_list = node;
        return;
    }
    Admin* p = g_admin_list;
    while (p->next) p = p->next;
    p->next = node;
}

/**
 * @brief 保存管理员链表至文件
 */
static int save_admins(void) {
    /* 统计节点数 */
    int count = 0;
    Admin* p = g_admin_list;
    while (p) { count++; p = p->next; }

    if (count == 0) {
        /* 删除文件 */
        remove(ADMIN_FILE);
        return 0;
    }

    /* 链表 → 数组 */
    Admin* arr = (Admin*)malloc(sizeof(Admin) * count);
    if (!arr) {
        fprintf(stderr, "[错误] 保存管理员时内存分配失败\n");
        return -1;
    }

    p = g_admin_list;
    for (int i = 0; i < count; i++) {
        arr[i] = *p;          /* 结构体值拷贝 */
        arr[i].next = NULL;   /* 清除指针字段 */
        p = p->next;
    }

    int ret = file_write_all(ADMIN_FILE, arr, sizeof(Admin), count);
    free(arr);
    return ret;
}

/**
 * @brief 释放管理员链表
 */
static void free_admins(void) {
    Admin* p = g_admin_list;
    while (p) {
        Admin* next = p->next;
        free(p);
        p = next;
    }
    g_admin_list = NULL;
}

/* ============================================================
 * 模块生命周期
 * ============================================================ */

int auth_init(void) {
    int count = 0;
    Admin* loaded = (Admin*)file_read_all(ADMIN_FILE, sizeof(Admin), &count);

    if (!loaded || count <= 0) {
        /* 文件不存在或为空 → 创建默认管理员 */
        if (count == -1) {
            /* 文件损坏，备份后重建 */
            fprintf(stderr, "[警告] 管理员数据文件损坏，将创建默认账号\n");
        } else {
            printf("[提示] 首次运行，创建默认管理员账号\n");
        }

        Admin* def = (Admin*)calloc(1, sizeof(Admin));
        if (!def) return -1;

        strncpy(def->username, DEFAULT_USER, sizeof(def->username) - 1);
        strncpy(def->password, DEFAULT_PASS, sizeof(def->password) - 1);
        def->role       = ROLE_ADMIN;
        def->lock_count = 0;
        def->lock_until = 0;

        g_admin_list = def;

        if (save_admins() != 0) {
            fprintf(stderr, "[错误] 默认管理员写入失败\n");
            free_admins();
            return -1;
        }
        return 0;
    }

    /* 将数组还原为链表 */
    for (int i = 0; i < count; i++) {
        Admin* node = (Admin*)malloc(sizeof(Admin));
        if (!node) {
            free_admins();
            free(loaded);
            return -1;
        }
        *node = loaded[i];
        append_admin(node);
    }

    free(loaded);
    return 0;
}

void auth_shutdown(void) {
    save_admins();
    free_admins();
}

/* ============================================================
 * 登录 / 登出
 * ============================================================ */

int auth_login(const char* username, const char* password) {
    if (!username || !password) return AUTH_USER_NOT_FOUND;

    Admin* adm = find_admin(username);
    if (!adm) return AUTH_USER_NOT_FOUND;

    /* 检查锁定状态 */
    if (adm->lock_until > 0) {
        time_t now = time(NULL);
        if (now < adm->lock_until) {
            /* 仍在锁定期 */
            return AUTH_LOCKED;
        }
        /* 锁定已超时 → 自动解锁 */
        adm->lock_count = 0;
        adm->lock_until = 0;
    }

    /* 校验密码 */
    if (strcmp(adm->password, password) != 0) {
        adm->lock_count++;
        if (adm->lock_count >= MAX_LOGIN_ATTEMPTS) {
            adm->lock_until = time(NULL) + LOGIN_LOCK_SECONDS;
            save_admins();  /* 持久化锁定状态 */
        }
        return AUTH_WRONG_PASSWORD;
    }

    /* 登录成功 → 清除锁定计数 */
    adm->lock_count = 0;
    adm->lock_until = 0;

    /* 设置当前会话 */
    strncpy(g_cur_user, adm->username, sizeof(g_cur_user) - 1);
    g_cur_role = adm->role;

    return AUTH_OK;
}

void auth_logout(void) {
    g_cur_user[0] = '\0';
    g_cur_role = -1;
}

const char* auth_current_user(void) {
    return g_cur_user;
}

int auth_current_role(void) {
    return g_cur_role;
}

/* ============================================================
 * 管理员管理
 * ============================================================ */

int auth_add_admin(const char* username, const char* password, int role) {
    /* 权限校验 */
    if (g_cur_role != ROLE_ADMIN) return -1;
    if (!username || !password) return -1;
    if (role != ROLE_ADMIN && role != ROLE_TA) return -1;

    /* 查重 */
    if (find_admin(username)) return AUTH_ALREADY_EXISTS;

    /* 创建节点 */
    Admin* node = (Admin*)calloc(1, sizeof(Admin));
    if (!node) return -1;

    strncpy(node->username, username, sizeof(node->username) - 1);
    strncpy(node->password, password, sizeof(node->password) - 1);
    node->role       = role;
    node->lock_count = 0;
    node->lock_until = 0;

    append_admin(node);
    save_admins();
    audit_log(AUDIT_ADMIN_ADD, username, "新增管理员", NULL);
    return AUTH_OK;
}

int auth_delete_admin(const char* username) {
    if (g_cur_role != ROLE_ADMIN) return -1;
    if (!username) return -1;

    /* 不能删除自己 */
    if (strcmp(username, g_cur_user) == 0) return -1;

    Admin* prev = NULL;
    Admin* curr = g_admin_list;

    while (curr) {
        if (strcmp(curr->username, username) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                g_admin_list = curr->next;
            }
            free(curr);
            save_admins();
            audit_log(AUDIT_ADMIN_DEL, username, "删除管理员", NULL);
            return 0;
        }
        prev = curr;
        curr = curr->next;
    }
    return -1;  /* 用户不存在 */
}

int auth_change_password(const char* username, const char* new_password) {
    /* 仅管理员可修改他人密码，普通用户仅可修改自己 */
    if (!username || !new_password) return -1;
    if (g_cur_role != ROLE_ADMIN && strcmp(username, g_cur_user) != 0)
        return -1;

    Admin* adm = find_admin(username);
    if (!adm) return -1;

    strncpy(adm->password, new_password, sizeof(adm->password) - 1);
    save_admins();
    audit_log(AUDIT_ADMIN_CHPWD, username, "修改密码", NULL);
    return 0;
}

void auth_list_admins(void) {
    Admin* p = g_admin_list;
    int idx = 1;

    printf("\n");
    printf("  %-4s %-20s %-12s %s\n", "序号", "用户名", "角色", "状态");
    print_line();

    while (p) {
        const char* role_str = (p->role == ROLE_ADMIN) ? "管理员" : "助教";
        const char* status = "正常";
        if (p->lock_until > 0) {
            time_t now = time(NULL);
            if (now < p->lock_until) {
                status = "锁定中";
            }
        }
        printf("  %-4d %-20s %-12s %s\n", idx++, p->username, role_str, status);
        p = p->next;
    }
    print_line();
}

int auth_lock_remaining(const char* username) {
    Admin* adm = find_admin(username);
    if (!adm || adm->lock_until <= 0) return 0;

    time_t now = time(NULL);
    int remaining = (int)(adm->lock_until - now);
    return remaining > 0 ? remaining : 0;
}
