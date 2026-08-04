/**
 * @file    search.h
 * @brief   检索模块 — 耗材检索 + 领用记录检索的 UI 交互
 * @details 封装检索的输入、查询、分页展示流程，
 *          委托 material 和 borrow 模块执行底层查询。
 */

#ifndef SEARCH_H
#define SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 耗材检索菜单（精准编号 + 模糊名称）
 */
void search_material_menu(void);

/**
 * @brief 领用记录检索菜单（班级/学号/项目多条件筛选）
 */
void search_record_menu(void);

#ifdef __cplusplus
}
#endif

#endif /* SEARCH_H */
