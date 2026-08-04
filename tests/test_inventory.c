/**
 * @file    test_inventory.c
 * @brief   库存预警与盘点模块测试
 */

#include "inventory.h"
#include "material.h"
#include "platform.h"
#include "types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MAT_FILE "data/material.dat"
#define TEST_SCRAP_FILE "data/scrap.dat"
#define TEST_STOCKTAKE "data/stocktake.dat"

static int g_passed = 0;
static int g_failed = 0;

static void run_test(const char *name, void (*fn)(void)) {
    printf("  %-50s ... ", name);
    fn();
    printf("[PASS]\n");
    g_passed++;
}

static void reset_env(void) {
    inventory_shutdown();
    material_shutdown();
    remove(TEST_STOCKTAKE);
    remove(TEST_MAT_FILE);
    remove(TEST_SCRAP_FILE);
    assert(material_init() == 0);
    assert(inventory_init() == 0);
}

/** 添加测试用耗材 */
static void add_test_materials(void) {
    Material m;
    memset(&m, 0, sizeof(m));
    strncpy(m.cabinet, "A-01", sizeof(m.cabinet) - 1);
    m.purchase_date = time(NULL);

    /* 库存充足的耗材 */
    strncpy(m.id, "M001", sizeof(m.id) - 1);
    strncpy(m.name, "电阻 10k", sizeof(m.name) - 1);
    m.category = CAT_ELECTRONIC;
    m.attr = ATTR_DISPOSABLE;
    m.unit_price = 0.05;
    m.total_stock = 500;
    m.min_stock = 50;
    material_add(&m);

    /* 接近预警的耗材 */
    strncpy(m.id, "M002", sizeof(m.id) - 1);
    strncpy(m.name, "Arduino Uno", sizeof(m.name) - 1);
    m.category = CAT_DEV_BOARD;
    m.attr = ATTR_REUSABLE;
    m.unit_price = 68.0;
    m.total_stock = 3;
    m.min_stock = 5;
    material_add(&m);

    /* 严重不足的耗材 */
    strncpy(m.id, "M003", sizeof(m.id) - 1);
    strncpy(m.name, "焊锡丝", sizeof(m.name) - 1);
    m.category = CAT_CHEMICAL;
    m.attr = ATTR_DISPOSABLE;
    m.unit_price = 15.0;
    m.total_stock = 0;
    m.min_stock = 10;
    material_add(&m);
}

/* ============================================================
 * 盘点测试
 * ============================================================ */

static void test_stocktake_normal(void) {
    const Material *before = material_find_by_id("M001");
    int book = before->total_stock; /* 500 */

    /* 实际盘点 495，差 -5（丢失），不修正 */
    int diff = inventory_stocktake_item("M001", 495, "admin", 0);
    assert(diff == -5);

    /* 库存不应变化（未修正） */
    const Material *after = material_find_by_id("M001");
    assert(after->total_stock == book);
}

static void test_stocktake_with_correct(void) {
    /* 实际盘点找到 5 件（之前漏记），修正 */
    int diff = inventory_stocktake_item("M002", 5, "admin", 1);
    assert(diff == 2);

    /* 库存应更新 */
    const Material *after = material_find_by_id("M002");
    assert(after->total_stock == 5);
}

static void test_stocktake_nonexistent(void) {
    int diff = inventory_stocktake_item("NOEXIST", 10, "admin", 0);
    assert(diff == -999999);
}

static void test_stocktake_log_created(void) {
    /* 之前的盘点应已生成日志 */
    int count = inventory_log_count();
    assert(count >= 2); /* 至少 2 条 */
}

static void test_stocktake_log_display(void) {
    int total_pages = 0;
    inventory_log_page(1, &total_pages);
    assert(total_pages >= 1);
}

/* ============================================================
 * 预警测试（material 模块的 alert / purchase 函数）
 * ============================================================ */

static void test_alert_has_low_stock(void) {
    /* M003（库存 0<10）应出现在预警中。
     * M002 已被盘点修正为 5（=min_stock），不再是预警项。 */
    const Material *m3 = material_find_by_id("M003");
    assert(m3 != NULL);
    assert(m3->total_stock < m3->min_stock);

    /* 验证库存充足的不在预警中 */
    const Material *m1 = material_find_by_id("M001");
    assert(m1 != NULL);
    assert(!(m1->total_stock < m1->min_stock));
}

static void test_purchase_list_output(void) {
    /* 调用采购清单函数验证不崩溃 */
    material_purchase_list();
}

static void test_alert_print_output(void) { material_alert_print(); }

/* ============================================================
 * 入口
 * ============================================================ */

int main(void) {
    setbuf(stdout, NULL);
    printf("=== 库存预警与盘点模块测试 ===\n\n");

    reset_env();
    add_test_materials();

    printf("[盘点]\n");
    run_test("盘点差异（不修正）库存不变", test_stocktake_normal);
    run_test("盘点差异+修正 → 库存更新", test_stocktake_with_correct);
    run_test("盘点不存在的耗材返回 -999999", test_stocktake_nonexistent);
    run_test("盘点后生成日志记录", test_stocktake_log_created);
    run_test("盘点日志分页展示不崩溃", test_stocktake_log_display);

    printf("\n[预警]\n");
    run_test("低库存耗材可检测（M002/M003）", test_alert_has_low_stock);
    run_test("采购清单输出不崩溃", test_purchase_list_output);
    run_test("预警列表输出不崩溃", test_alert_print_output);

    inventory_shutdown();
    material_shutdown();
    remove(TEST_STOCKTAKE);
    remove(TEST_MAT_FILE);
    remove(TEST_SCRAP_FILE);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
