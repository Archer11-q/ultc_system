/**
 * @file    csv_io.h
 * @brief   CSV 导入导出模块
 * @details 支持耗材批量导入（含校验+错误报告），
 *          耗材/预警/采购清单/领用记录导出为 CSV。
 *          UTF-8 BOM 兼容 Microsoft Excel 中文显示。
 */

#ifndef CSV_IO_H
#define CSV_IO_H

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 导入
 * ============================================================ */

/**
 * @brief 从 CSV 文件批量导入耗材
 * @details CSV 格式（首行为列标题，后续为数据）：
 *          编号,名称,分类,属性,单价,库存,预警,柜号,采购日期
 *          - 分类支持中文或数字（0~4）
 *          - 属性：一次性/可循环
 *          - 日期格式：YYYY-MM-DD
 *          导入过程中校验每行数据，输出错误报告。
 *          自动跳过重复编号的耗材。
 * @param filepath  CSV 文件路径
 * @param operator_name 操作员
 * @return 成功导入条数，-1 表示文件无法打开
 */
int csv_import_materials(const char* filepath, const char* operator_name);

/* ============================================================
 * 导出
 * ============================================================ */

/**
 * @brief 导出全部耗材为 CSV
 * @param filepath 导出路径
 * @return 0 成功，-1 失败
 */
int csv_export_materials(const char* filepath);

/**
 * @brief 导出采购清单为 CSV
 * @param filepath 导出路径
 * @return 0 成功，-1 失败
 */
int csv_export_purchase_list(const char* filepath);

/**
 * @brief 导出全部领用记录为 CSV
 * @param filepath 导出路径
 * @return 0 成功，-1 失败
 */
int csv_export_borrow_records(const char* filepath);

#ifdef __cplusplus
}
#endif

#endif /* CSV_IO_H */
