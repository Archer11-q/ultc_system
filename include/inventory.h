/**
 * @file    inventory.h
 * @brief   库存盘点模块 — 账面核对、差异修正、盘点日志持久化
 * @details 盘点日志记录每次盘点的账面值、实际值、差异值。
 *          支持自动修正（用实际值覆盖账面值）或仅记录差异。
 *          数据文件：data/stocktake.dat
 *
 *          库存预警和采购清单功能已集成在 material 模块中
 *          （material_alert_print / material_purchase_list），
 *          本模块聚焦盘点业务。
 */

#ifndef INVENTORY_H
#define INVENTORY_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 模块生命周期
 * ============================================================ */

int inventory_init(void);
void inventory_shutdown(void);

/* ============================================================
 * 盘点操作
 * ============================================================ */

/**
 * @brief 执行单条耗材盘点
 * @param material_id  耗材编号
 * @param actual_stock 实际盘点库存
 * @param operator_name 操作员用户名
 * @param auto_correct  1=用实际值修正账面值，0=仅记录不修正
 * @return 差异值（actual - book），-999999 表示耗材不存在
 */
int inventory_stocktake_item(const char *material_id, int actual_stock, const char *operator_name,
                             int auto_correct);

/**
 * @brief 分页展示盘点日志
 */
void inventory_log_page(int page, int *total_pages);

/**
 * @brief 盘点日志总数
 */
int inventory_log_count(void);

#ifdef __cplusplus
}
#endif

#endif /* INVENTORY_H */
