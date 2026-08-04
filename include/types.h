/**
 * @file    types.h
 * @brief   全部核心数据结构、枚举、常量定义
 * @details 本文件是系统数据模型的基础，所有模块共享此头文件。
 *          结构体采用定长字符数组，确保二进制序列化时结构体大小固定。
 */

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <time.h>

/* ============================================================
 * 版本信息
 * ============================================================ */

#define SYSTEM_VERSION  1       /**< 文件格式版本号              */
#define FILE_MAGIC      0x554C5443u  /**< 文件魔数 "ULTC"       */

/* ============================================================
 * 字符串长度常量
 * ============================================================ */

#define MAX_USERNAME        32      /**< 用户名最大长度         */
#define MAX_PASSWORD        32      /**< 密码最大长度           */
#define MAX_MAT_ID          16      /**< 耗材编号最大长度       */
#define MAX_MAT_NAME        64      /**< 耗材名称最大长度       */
#define MAX_CABINET         16      /**< 存放柜号最大长度       */
#define MAX_STUDENT_ID      16      /**< 学号最大长度           */
#define MAX_STUDENT_NAME    32      /**< 学生姓名最大长度       */
#define MAX_CLASS_NAME      32      /**< 班级名称最大长度       */
#define MAX_PROJECT_ID      16      /**< 实训项目编号最大长度   */
#define MAX_RECORD_ID       32      /**< 领用单号最大长度       */
#define MAX_DAMAGE_NOTE     128     /**< 损坏说明最大长度       */
#define MAX_SCRAP_ID        32      /**< 报废单号最大长度       */
#define MAX_LOG_ID          32      /**< 盘点日志编号最大长度   */
#define MAX_REASON          128     /**< 报废原因最大长度       */

/* ============================================================
 * 审计日志操作类型
 * ============================================================ */

typedef enum {
    AUDIT_LOGIN        = 0,   /**< 用户登录                       */
    AUDIT_LOGOUT       = 1,   /**< 用户登出                       */
    AUDIT_MAT_ADD      = 2,   /**< 新增耗材                       */
    AUDIT_MAT_EDIT     = 3,   /**< 修改耗材                       */
    AUDIT_MAT_DELETE   = 4,   /**< 删除耗材                       */
    AUDIT_MAT_SCRAP    = 5,   /**< 报废耗材                       */
    AUDIT_BORROW       = 6,   /**< 学生领用                       */
    AUDIT_RETURN       = 7,   /**< 耗材归还                       */
    AUDIT_STOCKTAKE    = 8,   /**< 盘点修正                       */
    AUDIT_ADMIN_ADD    = 9,   /**< 新增管理员                     */
    AUDIT_ADMIN_DEL    = 10,  /**< 删除管理员                     */
    AUDIT_ADMIN_CHPWD  = 11,  /**< 修改密码                       */
    AUDIT_IMPORT       = 12   /**< CSV批量导入                    */
} AuditAction;

/* ============================================================
 * 审计日志结构体
 * ============================================================ */

typedef struct AuditRecord {
    char        log_id[32];         /**< 日志编号 AUDIT-YYYYMMDD-NNNNNN */
    time_t      timestamp;          /**< 操作时间                       */
    char        operator_name[MAX_USERNAME];  /**< 操作者               */
    int         action;             /**< AuditAction                    */
    char        target_id[64];      /**< 操作对象（耗材编号/学号/用户名）*/
    char        detail[256];        /**< 操作详情                       */
    struct AuditRecord* next;       /**< 链表后继                       */
} AuditRecord;

/* ============================================================
 * 业务常量
 * ============================================================ */

#define MAX_LOGIN_ATTEMPTS  3       /**< 登录最大错误次数       */
#define LOGIN_LOCK_SECONDS  10      /**< 锁定时间（秒）         */
#define OVERDUE_DAYS        7       /**< 逾期天数阈值           */
#define PAGE_SIZE           10      /**< 分页每页条数           */

/* ============================================================
 * 管理员角色
 * ============================================================ */

typedef enum {
    ROLE_ADMIN = 0,     /**< 实验老师：全权限                 */
    ROLE_TA    = 1      /**< 实训助教：仅查询                 */
} AdminRole;

/* ============================================================
 * 耗材分类
 * ============================================================ */

typedef enum {
    CAT_ELECTRONIC = 0,     /**< 电子元器件（电阻、芯片等）    */
    CAT_TOOL       = 1,     /**< 电工工具（万用表、烙铁等）    */
    CAT_DEV_BOARD  = 2,     /**< 开发板（Arduino、STM32等）    */
    CAT_CHEMICAL   = 3,     /**< 化学耗材（焊锡、助焊剂等）    */
    CAT_MECHANICAL = 4      /**< 机械零件（螺丝、杜邦线等）    */
} MaterialCategory;

/* ============================================================
 * 耗材属性
 * ============================================================ */

typedef enum {
    ATTR_DISPOSABLE = 0,    /**< 一次性耗材：领用直接扣减库存   */
    ATTR_REUSABLE   = 1     /**< 可循环耗材：领用仅登记不扣减   */
} MaterialAttr;

/* ============================================================
 * 领用状态
 * ============================================================ */

typedef enum {
    BORROW_ACTIVE   = 0,    /**< 领用中                         */
    BORROW_RETURNED = 1,    /**< 已归还                         */
    BORROW_OVERDUE  = 2,    /**< 逾期未归还                     */
    BORROW_SCRAPPED = 3     /**< 已报废（归还时损坏转入）       */
} BorrowStatus;

/* ============================================================
 * 管理员结构体（链表节点）
 * ============================================================ */

typedef struct Admin {
    char        username[MAX_USERNAME];     /**< 登录用户名     */
    char        password[MAX_PASSWORD];     /**< 登录密码       */
    int         role;                       /**< AdminRole      */
    int         lock_count;                 /**< 连续错误次数   */
    time_t      lock_until;                 /**< 锁定截止时间，0=未锁定 */
    struct Admin* next;                     /**< 链表后继节点   */
} Admin;

/* ============================================================
 * 耗材结构体（链表节点）
 * ============================================================ */

typedef struct Material {
    char        id[MAX_MAT_ID];             /**< 耗材编号（唯一）*/
    char        name[MAX_MAT_NAME];         /**< 耗材名称       */
    int         category;                   /**< MaterialCategory */
    int         attr;                       /**< MaterialAttr   */
    double      unit_price;                 /**< 采购单价       */
    int         total_stock;                /**< 总库存量       */
    int         min_stock;                  /**< 最低预警库存   */
    char        cabinet[MAX_CABINET];       /**< 存放柜号       */
    time_t      purchase_date;              /**< 采购日期       */
    struct Material* next;                  /**< 链表后继节点   */
} Material;

/* ============================================================
 * 领用记录结构体（链表节点）
 * ============================================================ */

typedef struct BorrowRecord {
    char        record_id[MAX_RECORD_ID];       /**< 领用单号    */
    char        student_id[MAX_STUDENT_ID];     /**< 学号        */
    char        student_name[MAX_STUDENT_NAME]; /**< 学生姓名    */
    char        class_name[MAX_CLASS_NAME];     /**< 班级        */
    char        project_id[MAX_PROJECT_ID];     /**< 实训项目编号*/
    char        material_id[MAX_MAT_ID];        /**< 领用耗材编号*/
    int         quantity;                       /**< 领用数量    */
    time_t      borrow_time;                    /**< 领用时间    */
    time_t      return_time;                    /**< 归还时间，0=未归还 */
    int         status;                         /**< BorrowStatus */
    char        damage_note[MAX_DAMAGE_NOTE];   /**< 损坏情况说明*/
    char        operator_name[MAX_USERNAME];    /**< 操作管理员  */
    struct BorrowRecord* next;                  /**< 链表后继节点*/
} BorrowRecord;

/* ============================================================
 * 报废记录结构体（链表节点）
 * ============================================================ */

typedef struct ScrapRecord {
    char        scrap_id[MAX_SCRAP_ID];         /**< 报废单号    */
    char        material_id[MAX_MAT_ID];        /**< 耗材编号    */
    char        material_name[MAX_MAT_NAME];    /**< 耗材名称    */
    time_t      scrap_time;                     /**< 报废时间    */
    char        reason[MAX_REASON];             /**< 报废原因    */
    int         quantity;                       /**< 报废数量    */
    char        operator_name[MAX_USERNAME];    /**< 操作管理员  */
    struct ScrapRecord* next;                   /**< 链表后继节点*/
} ScrapRecord;

/* ============================================================
 * 盘点日志结构体（链表节点）
 * ============================================================ */

typedef struct StocktakeLog {
    char        log_id[MAX_LOG_ID];             /**< 日志编号    */
    char        material_id[MAX_MAT_ID];        /**< 耗材编号    */
    int         book_value;                     /**< 账面库存    */
    int         actual_value;                   /**< 实际库存    */
    int         diff;                           /**< 差异值      */
    char        operator_name[MAX_USERNAME];    /**< 操作管理员  */
    time_t      check_time;                     /**< 盘点时间    */
    struct StocktakeLog* next;                  /**< 链表后继节点*/
} StocktakeLog;

#endif /* TYPES_H */
