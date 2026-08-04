/**
 * @file    material.h
 * @brief   耗材档案管理模块 — 增删改查、报废、分页展示
 * @details 耗材分为 5 种分类、一次性/可循环两种属性。
 *          新增时校验编号唯一性，报废自动扣减库存并留存记录。
 *          数据持久化至 data/material.dat 和 data/scrap.dat。
 */

#ifndef MATERIAL_H
#define MATERIAL_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 模块生命周期
 * ============================================================ */

/**
 * @brief 初始化耗材管理模块
 * @details 从 data/material.dat 加载耗材列表，
 *          从 data/scrap.dat 加载报废记录列表。
 * @return 0 成功，-1 失败
 */
int material_init(void);

/**
 * @brief 关闭耗材管理模块
 * @details 保存耗材及报废记录至文件，释放链表内存。
 */
void material_shutdown(void);

/* ============================================================
 * 耗材 CRUD
 * ============================================================ */

/**
 * @brief 新增耗材
 * @param mat 耗材数据（id 字段必须填写，next 忽略）
 * @return 0 成功，-1 编号重复，-2 参数无效
 */
int material_add(const Material *mat);

/**
 * @brief 修改耗材信息
 * @details 根据 id 查找耗材，更新 name/category/attr/unit_price/
 *          total_stock/min_stock/cabinet/purchase_date。
 *          不可修改 id 字段。
 * @param mat 新的耗材数据
 * @return 0 成功，-1 耗材不存在
 */
int material_update(const Material *mat);

/**
 * @brief 删除耗材（物理删除）
 * @param id 耗材编号
 * @return 0 成功，-1 不存在
 */
int material_delete(const char *id);

/**
 * @brief 按编号查找耗材
 * @param id 耗材编号
 * @return 找到返回节点指针（只读），否则 NULL
 */
const Material *material_find_by_id(const char *id);

/* ============================================================
 * 库存操作
 * ============================================================ */

/**
 * @brief 扣减库存（领用一次性耗材时调用）
 * @param id       耗材编号
 * @param quantity 扣减数量
 * @return 0 成功，-1 库存不足，-2 耗材不存在
 */
int material_reduce_stock(const char *id, int quantity);

/**
 * @brief 增加库存（归还或补货时调用）
 * @param id       耗材编号
 * @param quantity 增加数量
 * @return 0 成功，-1 耗材不存在
 */
int material_increase_stock(const char *id, int quantity);

/* ============================================================
 * 报废管理
 * ============================================================ */

/**
 * @brief 报废耗材
 * @details 扣减库存并生成报废记录，持久化到 data/scrap.dat。
 * @param material_id 耗材编号
 * @param quantity    报废数量
 * @param reason      报废原因（如 "损坏"、"过期"、"学生归还时损坏"）
 * @param operator    操作管理员用户名
 * @return 0 成功，-1 库存不足，-2 耗材不存在
 */
int material_scrap(const char *material_id, int quantity, const char *reason,
                   const char *operator_name);

/**
 * @brief 获取全部报废记录
 * @param out_count 输出参数
 * @return 报废记录数组（调用方 free）
 */
ScrapRecord *material_scrap_get_all(int *out_count);

/**
 * @brief 列出全部报废记录（分页）
 * @param page       页码（从 1 开始）
 * @param total_pages 输出参数，总页数
 */
void material_scrap_list(int page, int *total_pages);

/* ============================================================
 * 查询与展示
 * ============================================================ */

/**
 * @brief 获取耗材总数
 */
int material_count(void);

/**
 * @brief 分页展示耗材列表
 * @param page       页码（从 1 开始）
 * @param total_pages 输出参数，总页数
 */
void material_list_page(int page, int *total_pages);

/**
 * @brief 按分类名称返回中文字符串
 */
const char *material_category_name(int category);

/**
 * @brief 按属性返回中文字符串
 */
const char *material_attr_name(int attr);

/* ============================================================
 * 库存预警与采购清单
 * ============================================================ */

/**
 * @brief 打印库存预警清单（所有 total_stock < min_stock 的耗材）
 */
void material_alert_print(void);

/**
 * @brief 按名称模糊搜索耗材
 * @param keyword   搜索关键词（子串匹配）
 * @param out_count 输出参数，匹配条数
 * @return 匹配的耗材数组（调用方 free），无匹配返回 NULL
 */
/**
 * @brief 获取全部耗材（数组形式）
 * @param out_count 输出参数
 * @return 耗材数组（调用方 free）
 */
Material *material_get_all(int *out_count);

Material *material_search_by_name(const char *keyword, int *out_count);

/**
 * @brief 打印采购清单
 * @details 列出所有预警耗材，含建议采购量（min_stock × 2 - total_stock）
 *          和预估金额，底部汇总总金额。
 */
void material_purchase_list(void);

#ifdef __cplusplus
}
#endif

#endif /* MATERIAL_H */
