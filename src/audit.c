/**
 * @file    audit.c
 * @brief   操作审计日志模块实现
 * @details AuditRecord 链表管理，所有写操作自动记录。
 *          文件路径：data/audit.dat
 */

#include "audit.h"
#include "auth.h"
#include "file_io.h"
#include "platform.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define AUDIT_FILE "data/audit.dat"

static AuditRecord *g_audit_list = NULL;

/* ============================================================
 * 内部辅助
 * ============================================================ */

static void append_audit(AuditRecord *node) {
    node->next = NULL;
    if (!g_audit_list) {
        g_audit_list = node;
        return;
    }
    AuditRecord *p = g_audit_list;
    while (p->next)
        p = p->next;
    p->next = node;
}

static int save_audits(void) {
    int count = 0;
    AuditRecord *p = g_audit_list;
    while (p) {
        count++;
        p = p->next;
    }
    if (count == 0) {
        remove(AUDIT_FILE);
        return 0;
    }

    AuditRecord *arr = (AuditRecord *)malloc(sizeof(AuditRecord) * count);
    if (!arr)
        return -1;
    p = g_audit_list;
    for (int i = 0; i < count; i++) {
        arr[i] = *p;
        arr[i].next = NULL;
        p = p->next;
    }
    int ret = file_write_all(AUDIT_FILE, arr, sizeof(AuditRecord), count);
    free(arr);
    return ret;
}

/* ============================================================
 * 模块生命周期
 * ============================================================ */

int audit_init(void) {
    int count = 0;
    AuditRecord *arr = (AuditRecord *)file_read_all(AUDIT_FILE, sizeof(AuditRecord), &count);
    if (count == -1) {
        fprintf(stderr, "[警告] 审计日志文件损坏，将创建空记录\n");
        remove(AUDIT_FILE);
        return 0;
    }
    if (!arr || count == 0)
        return 0;
    for (int i = 0; i < count; i++) {
        AuditRecord *node = (AuditRecord *)malloc(sizeof(AuditRecord));
        if (!node) {
            free(arr);
            return -1;
        }
        *node = arr[i];
        append_audit(node);
    }
    free(arr);
    return 0;
}

void audit_shutdown(void) {
    save_audits();
    AuditRecord *p = g_audit_list;
    while (p) {
        AuditRecord *n = p->next;
        free(p);
        p = n;
    }
    g_audit_list = NULL;
}

/* ============================================================
 * 记录操作
 * ============================================================ */

void audit_log(int action, const char *target_id, const char *detail, const char *operator_name) {
    if (!target_id)
        target_id = "";
    if (!detail)
        detail = "";
    if (!operator_name)
        operator_name = auth_current_user();
    if (!operator_name || operator_name[0] == '\0')
        operator_name = "系统";

    AuditRecord *rec = (AuditRecord *)calloc(1, sizeof(AuditRecord));
    if (!rec)
        return;

    /* 生成日志编号 */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    int seq = audit_count() + 1;
    strftime(rec->log_id, sizeof(rec->log_id), "AUDIT-%Y%m%d-", tm);
    char seq_str[16];
    snprintf(seq_str, sizeof(seq_str), "%06d", seq);
    strncat(rec->log_id, seq_str, sizeof(rec->log_id) - strlen(rec->log_id) - 1);

    rec->timestamp = now;
    strncpy(rec->operator_name, operator_name, sizeof(rec->operator_name) - 1);
    rec->action = action;
    strncpy(rec->target_id, target_id, sizeof(rec->target_id) - 1);
    strncpy(rec->detail, detail, sizeof(rec->detail) - 1);

    append_audit(rec);
    save_audits();
}

/* ============================================================
 * 查询
 * ============================================================ */

void audit_list_page(int page, int *total_pages, int filter_action, const char *filter_operator) {
    if (page < 1)
        page = 1;

    /* 统计匹配数 */
    int total = 0;
    AuditRecord *p = g_audit_list;
    while (p) {
        int match = 1;
        if (filter_action >= 0 && p->action != filter_action)
            match = 0;
        if (filter_operator && filter_operator[0] != '\0' &&
            strcmp(p->operator_name, filter_operator) != 0)
            match = 0;
        if (match)
            total++;
        p = p->next;
    }

    *total_pages = (total == 0) ? 1 : (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (page > *total_pages)
        page = *total_pages;

    if (total == 0) {
        printf("\n  [提示] 暂无匹配的审计日志。\n");
        return;
    }

    printf("\n");
    printf("  %-4s %-28s %-18s %-10s %-20s %s\n", "序号", "日志编号", "时间", "操作者", "操作类型",
           "对象/详情");
    print_line();

    int start = (page - 1) * PAGE_SIZE;
    int idx = 0, shown = 0;
    p = g_audit_list;

    /* 跳过前面不匹配的项 */
    while (p) {
        int match = 1;
        if (filter_action >= 0 && p->action != filter_action)
            match = 0;
        if (filter_operator && filter_operator[0] != '\0' &&
            strcmp(p->operator_name, filter_operator) != 0)
            match = 0;
        if (match) {
            if (idx >= start)
                break;
            idx++;
        }
        p = p->next;
    }

    int display_idx = start + 1;
    while (p && shown < PAGE_SIZE) {
        int match = 1;
        if (filter_action >= 0 && p->action != filter_action)
            match = 0;
        if (filter_operator && filter_operator[0] != '\0' &&
            strcmp(p->operator_name, filter_operator) != 0)
            match = 0;

        if (match) {
            char time_buf[20];
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&p->timestamp));
            printf("  %-4d %-28s %-18s %-10s %-20s %s\n", display_idx++, p->log_id, time_buf,
                   p->operator_name, audit_action_name(p->action),
                   p->detail[0] ? p->detail : p->target_id);
            shown++;
        }
        p = p->next;
    }
    print_line();
    printf("  第 %d/%d 页（共 %d 条）\n", page, *total_pages, total);
}

int audit_count(void) {
    int count = 0;
    AuditRecord *p = g_audit_list;
    while (p) {
        count++;
        p = p->next;
    }
    return count;
}

const char *audit_action_name(int action) {
    switch (action) {
    case AUDIT_LOGIN:
        return "登录";
    case AUDIT_LOGOUT:
        return "登出";
    case AUDIT_MAT_ADD:
        return "新增耗材";
    case AUDIT_MAT_EDIT:
        return "修改耗材";
    case AUDIT_MAT_DELETE:
        return "删除耗材";
    case AUDIT_MAT_SCRAP:
        return "报废耗材";
    case AUDIT_BORROW:
        return "学生领用";
    case AUDIT_RETURN:
        return "耗材归还";
    case AUDIT_STOCKTAKE:
        return "盘点修正";
    case AUDIT_ADMIN_ADD:
        return "新增管理员";
    case AUDIT_ADMIN_DEL:
        return "删除管理员";
    case AUDIT_ADMIN_CHPWD:
        return "修改密码";
    case AUDIT_IMPORT:
        return "CSV批量导入";
    default:
        return "未知";
    }
}
