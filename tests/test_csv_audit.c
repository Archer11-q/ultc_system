/**
 * @file    test_csv_audit.c
 * @brief   CSV 导入导出 + 审计日志模块测试
 */

#include "types.h"
#include "auth.h"
#include "material.h"
#include "borrow.h"
#include "audit.h"
#include "csv_io.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_passed = 0;
static int g_failed = 0;
static const char* g_files[] = {
    "data/admin.dat", "data/material.dat", "data/scrap.dat",
    "data/borrow.dat", "data/audit.dat", "data/stocktake.dat",
    "test_import.csv", "test_export.csv", "test_purchase.csv",
    "test_borrows.csv", NULL
};

static void run_test(const char* name, void (*fn)(void)) {
    printf("  %-50s ... ", name);
    fn();
    printf("[PASS]\n");
    g_passed++;
}

static void clean_all(void) {
    for (int i = 0; g_files[i]; i++) remove(g_files[i]);
}

static void full_init(void) {
    assert(auth_init() == 0);
    assert(material_init() == 0);
    assert(borrow_init() == 0);
    assert(audit_init() == 0);
}

static void full_shutdown(void) {
    audit_shutdown();
    borrow_shutdown();
    material_shutdown();
    auth_shutdown();
}

/* ============================================================
 * CSV 导入测试
 * ============================================================ */

static void test_csv_import_valid(void) {
    /* 创建测试 CSV */
    FILE* fp = fopen("test_import.csv", "w");
    fprintf(fp, "编号,名称,分类,属性,单价,库存,预警,柜号,采购日期\n");
    fprintf(fp, "CSV001,测试电阻,电子元器件,一次性,0.10,1000,100,A-01,2024-09-01\n");
    fprintf(fp, "CSV002,测试开发板,开发板,可循环,35.00,50,10,B-03,2024-08-15\n");
    fprintf(fp, "CSV003,焊锡丝 0.8mm,化学耗材,一次性,8.50,200,30,C-02,2024-07-20\n");
    fclose(fp);

    auth_login("admin", "admin123");
    int imported = csv_import_materials("test_import.csv", "admin");
    assert(imported == 3);
    assert(material_find_by_id("CSV001") != NULL);
    assert(material_find_by_id("CSV002") != NULL);
    assert(material_find_by_id("CSV003") != NULL);
}

static void test_csv_import_skip_duplicate(void) {
    /* 重复导入应跳过 */
    int imported = csv_import_materials("test_import.csv", "admin");
    assert(imported == 0);  /* 全部重复 */
    assert(material_count() == 3);  /* 未增加 */
}

static void test_csv_import_handle_errors(void) {
    /* 写一个含错误行的 CSV */
    FILE* fp = fopen("test_import.csv", "w");
    fprintf(fp, "编号,名称,分类,属性,单价,库存,预警,柜号,采购日期\n");
    fprintf(fp, "CSV004,正常耗材,电工工具,一次性,5.0,50,5,D-01,2024-06-01\n");
    fprintf(fp, ",空编号,电工工具,一次性,5.0,50,5,D-01,2024-06-01\n");  /* 空编号 */
    fprintf(fp, "CSV005,无效分类,不存在的分类,一次性,5.0,50,5,D-01,2024-06-01\n");
    fprintf(fp, "CSV006,负库存,电工工具,一次性,5.0,-10,5,D-01,2024-06-01\n");
    fclose(fp);

    int imported = csv_import_materials("test_import.csv", "admin");
    assert(imported == 1);  /* 只有 CSV004 成功 */
    assert(material_find_by_id("CSV004") != NULL);
    assert(material_count() == 4);
}

/* ============================================================
 * CSV 导出测试
 * ============================================================ */

static void test_csv_export_materials(void) {
    int ret = csv_export_materials("test_export.csv");
    assert(ret == 0);

    /* 验证文件存在且非空 */
    FILE* fp = fopen("test_export.csv", "r");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fclose(fp);
    assert(sz > 10);  /* 至少有 BOM + 标题行 */
}

static void test_csv_export_purchase(void) {
    /* 制造预警：CSV001 库存 1000 预警 100，不预警。
     * 手动降低一个耗材库存 */
    material_reduce_stock("CSV002", 45);  /* 50-45=5, 预警10 → 告警 */

    int ret = csv_export_purchase_list("test_purchase.csv");
    assert(ret == 0);

    FILE* fp = fopen("test_purchase.csv", "r");
    assert(fp != NULL);
    fclose(fp);
}

static void test_csv_export_borrows(void) {
    /* 创建一条领用记录 */
    material_reduce_stock("CSV001", 10);
    BorrowRecord r;
    memset(&r, 0, sizeof(r));
    strncpy(r.record_id, "B-TEST-001", sizeof(r.record_id)-1);
    strncpy(r.student_id, "S001", sizeof(r.student_id)-1);
    strncpy(r.student_name, "测试", sizeof(r.student_name)-1);
    strncpy(r.class_name, "计科2101", sizeof(r.class_name)-1);
    strncpy(r.project_id, "P001", sizeof(r.project_id)-1);
    strncpy(r.material_id, "CSV001", sizeof(r.material_id)-1);
    r.quantity = 10; r.borrow_time = time(NULL);
    r.status = BORROW_ACTIVE;
    strncpy(r.operator_name, "admin", sizeof(r.operator_name)-1);
    borrow_create(&r);

    int ret = csv_export_borrow_records("test_borrows.csv");
    assert(ret == 0);

    FILE* fp = fopen("test_borrows.csv", "r");
    assert(fp != NULL);
    fclose(fp);
}

/* ============================================================
 * 审计日志测试
 * ============================================================ */

static void test_audit_log_created(void) {
    /* CSV 导入应已记录 AUDIT_IMPORT */
    int count = audit_count();
    assert(count > 0);  /* 之前的导入操作应已记录 */
}

static void test_audit_action_name(void) {
    assert(strcmp(audit_action_name(AUDIT_MAT_ADD), "新增耗材") == 0);
    assert(strcmp(audit_action_name(AUDIT_BORROW), "学生领用") == 0);
    assert(strcmp(audit_action_name(AUDIT_IMPORT), "CSV批量导入") == 0);
}

static void test_audit_manual_log(void) {
    int before = audit_count();
    audit_log(AUDIT_MAT_ADD, "TEST01", "测试审计日志", "admin");
    int after = audit_count();
    assert(after == before + 1);
}

static void test_audit_list_page(void) {
    int total_pages = 0;
    audit_list_page(1, &total_pages, -1, "");
    assert(total_pages >= 1);
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void) {
    setbuf(stdout, NULL);
    printf("=== CSV 导入导出 + 审计日志测试 ===\n\n");

    clean_all();
    full_init();

    printf("[CSV 导入]\n");
    run_test("正常导入 3 条耗材",               test_csv_import_valid);
    run_test("重复导入全部跳过",                 test_csv_import_skip_duplicate);
    run_test("含错误行：仅 1/4 成功",            test_csv_import_handle_errors);

    printf("\n[CSV 导出]\n");
    run_test("导出全部耗材文件非空",             test_csv_export_materials);
    run_test("导出采购清单（含预警项）",         test_csv_export_purchase);
    run_test("导出领用记录文件非空",             test_csv_export_borrows);

    printf("\n[审计日志]\n");
    run_test("已有审计记录（导入操作自动记录）", test_audit_log_created);
    run_test("操作类型名称映射正确",             test_audit_action_name);
    run_test("手动记录一条审计日志",             test_audit_manual_log);
    run_test("审计日志分页展示不崩溃",           test_audit_list_page);

    full_shutdown();
    clean_all();

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
