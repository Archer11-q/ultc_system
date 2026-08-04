/**
 * @file    auth.h
 * @brief   登录认证模块 — 多管理员账号管理、密码校验、锁定机制
 * @details 支持实验老师（全权限）和实训助教（仅查询）两种角色。
 *          连续密码错误达到阈值后锁定账号，超时后自动解锁。
 *          数据持久化至 data/admin.dat。
 */

#ifndef AUTH_H
#define AUTH_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 登录结果码
 * ============================================================ */

#define AUTH_OK 0              /**< 登录成功                   */
#define AUTH_USER_NOT_FOUND -1 /**< 用户名不存在               */
#define AUTH_WRONG_PASSWORD -2 /**< 密码错误                   */
#define AUTH_LOCKED -3         /**< 账号已锁定，请稍后重试     */
#define AUTH_ALREADY_EXISTS -4 /**< 用户名已存在（添加时）     */

/* ============================================================
 * 模块生命周期
 * ============================================================ */

/**
 * @brief 初始化认证模块
 * @details 从 data/admin.dat 加载管理员列表。
 *          若文件不存在或无管理员记录，自动创建默认管理员账号
 *          （用户名 admin，密码 admin123，角色 ROLE_ADMIN）。
 * @return 0 成功，-1 文件损坏
 */
int auth_init(void);

/**
 * @brief 关闭认证模块
 * @details 保存管理员列表至文件，释放链表内存。
 */
void auth_shutdown(void);

/* ============================================================
 * 登录 / 登出
 * ============================================================ */

/**
 * @brief 管理员登录
 * @param username 用户名
 * @param password 密码
 * @return AUTH_OK 成功，其他负值见结果码定义
 */
int auth_login(const char *username, const char *password);

/**
 * @brief 退出当前登录
 */
void auth_logout(void);

/**
 * @brief 获取当前登录用户名
 * @return 未登录返回空串 ""
 */
const char *auth_current_user(void);

/**
 * @brief 获取当前登录角色
 * @return 未登录返回 -1
 */
int auth_current_role(void);

/* ============================================================
 * 管理员管理（需要 ROLE_ADMIN 权限）
 * ============================================================ */

/**
 * @brief 新增管理员
 * @param username 用户名
 * @param password 密码
 * @param role     角色（ROLE_ADMIN 或 ROLE_TA）
 * @return AUTH_OK 成功，AUTH_ALREADY_EXISTS 用户名重复，-1 权限不足
 */
int auth_add_admin(const char *username, const char *password, int role);

/**
 * @brief 删除管理员
 * @param username 用户名
 * @return 0 成功，-1 不存在或权限不足
 */
int auth_delete_admin(const char *username);

/**
 * @brief 修改管理员密码
 * @param username     目标用户名
 * @param new_password 新密码
 * @return 0 成功，-1 失败
 */
int auth_change_password(const char *username, const char *new_password);

/**
 * @brief 列出全部管理员（控制台打印）
 */
void auth_list_admins(void);

/**
 * @brief 获取指定用户的锁定剩余秒数
 * @param username 用户名
 * @return 剩余锁定秒数，0 表示未锁定
 */
int auth_lock_remaining(const char *username);

#ifdef __cplusplus
}
#endif

#endif /* AUTH_H */
