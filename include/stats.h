/**
 * @file    stats.h
 * @brief   数据统计模块 — 月度消耗、班级排行、逾期汇总、报废成本
 * @details 统计功能委托 borrow 和 material 模块获取数据，
 *          本模块负责聚合计算和格式化报表输出。
 */

#ifndef STATS_H
#define STATS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 统计主菜单入口
 */
void stats_menu(void);

#ifdef __cplusplus
}
#endif

#endif /* STATS_H */
