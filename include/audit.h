/**
 * @file    audit.h
 * @brief   操作审计日志模块
 * @details 自动记录所有关键写操作（增删改、领用、归还、报废、盘点修正），
 *          支持按操作者/类型/时间范围筛选查看。
 *          数据文件：data/audit.dat
 */

#ifndef AUDIT_H
#define AUDIT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 模块生命周期
 * ============================================================ */

int audit_init(void);
void audit_shutdown(void);

/* ============================================================
 * 记录操作
 * ============================================================ */

/**
 * @brief 记录一条审计日志
 * @param action    操作类型（AuditAction 枚举）
 * @param target_id 操作对象标识（耗材编号/学号/用户名等）
 * @param detail    操作详情描述
 * @param operator_name 操作者用户名（传 NULL 则使用当前登录用户）
 */
void audit_log(int action, const char *target_id, const char *detail, const char *operator_name);

/* ============================================================
 * 查询
 * ============================================================ */

/**
 * @brief 分页展示审计日志
 * @param page        页码
 * @param total_pages 输出参数
 * @param filter_action 筛选操作类型（-1 表示全部）
 * @param filter_operator 筛选操作者（空串表示全部）
 */
void audit_list_page(int page, int *total_pages, int filter_action, const char *filter_operator);

/**
 * @brief 审计日志总数
 */
int audit_count(void);

/**
 * @brief 获取操作类型的中文名称
 */
const char *audit_action_name(int action);

#ifdef __cplusplus
}
#endif

#endif /* AUDIT_H */
