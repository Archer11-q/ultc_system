/**
 * @file    borrow.h
 * @brief   学生领用 / 归还 / 逾期管理模块
 * @details 领用业务规则：
 *          - 一次性耗材：库存充足才可领用，领用后直接扣减库存
 *          - 可循环耗材：领用仅登记，不扣减总库存，须按期归还
 *          领用单号格式：BORROW-YYYYMMDD-NNN
 *          数据持久化至 data/borrow.dat
 */

#ifndef BORROW_H
#define BORROW_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 模块生命周期
 * ============================================================ */

int borrow_init(void);
void borrow_shutdown(void);

/* ============================================================
 * 领用操作
 * ============================================================ */

/**
 * @brief 生成新的领用单号
 * @param buf   输出缓冲区
 * @param bufsz 缓冲区大小
 */
void borrow_gen_id(char* buf, size_t bufsz);

/**
 * @brief 创建一条领用记录
 * @details 调用前应已完成库存扣减（一次性耗材）。
 *          可循环耗材不扣库存，直接创建记录。
 * @param rec 领用记录数据（record_id、student_*、material_id、quantity、
 *            borrow_time、operator_name 需填写，next 忽略）
 * @return 0 成功，-1 失败
 */
int borrow_create(const BorrowRecord* rec);

/**
 * @brief 按领用单号获取所有记录
 * @param record_id 领用单号
 * @param out_count 输出参数，记录条数
 * @return 记录数组（调用方 free），无记录返回 NULL
 */
BorrowRecord* borrow_get_by_record_id(const char* record_id, int* out_count);

/* ============================================================
 * 归还操作（v0.5 完整实现，v0.4 提供接口占位）
 * ============================================================ */

/**
 * @brief 按学号列出所有未归还记录
 * @param student_id 学号
 * @param out_count  输出参数，记录条数
 * @return 记录数组（调用方 free），无记录返回 NULL
 */
BorrowRecord* borrow_get_unreturned_by_student(const char* student_id,
                                                int* out_count);

/**
 * @brief 执行归还
 * @param record_id   领用单号
 * @param damage_note 损坏说明（无损坏传 ""）
 * @return 0 成功，-1 记录不存在，-2 已归还
 */
int borrow_return(const char* record_id, const char* damage_note);

/**
 * @brief 获取逾期未归还的记录
 * @param out_count 输出参数
 * @return 记录数组（调用方 free）
 */
BorrowRecord* borrow_get_overdue_list(int* out_count);

/* ============================================================
 * 查询
 * ============================================================ */

/**
 * @brief 按班级/学号/实训项目筛选领用记录
 * @param class_name   班级（空串表示不限）
 * @param student_id   学号（空串表示不限）
 * @param project_id   实训项目编号（空串表示不限）
 * @param out_count    输出参数
 * @return 匹配的记录数组（调用方 free）
 */
BorrowRecord* borrow_search(const char* class_name, const char* student_id,
                             const char* project_id, int* out_count);

/**
 * @brief 获取领用记录总数
 */
int borrow_count(void);

/**
 * @brief 分页展示领用记录
 * @param page        页码
 * @param total_pages 输出参数
 */
void borrow_list_page(int page, int* total_pages);

#ifdef __cplusplus
}
#endif

#endif /* BORROW_H */
