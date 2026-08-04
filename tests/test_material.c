/**
 * @file    test_material.c
 * @brief   耗材档案管理模块测试
 * @details 测试项覆盖：
 *          - 新增/重复拒绝/查询/修改/删除
 *          - 库存扣减/增加（领用/归还/补货场景）
 *          - 报废扣库存 + 报废记录生成
 *          - 分页展示
 *          - 分类/属性名称映射
 */

#include "material.h"
#include "platform.h"
#include "types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MAT_FILE "data/material.dat"
#define TEST_SCRAP_FILE "data/scrap.dat"

static int g_passed = 0;
static int g_failed = 0;

/* ============================================================
 * 测试辅助
 * ============================================================ */

static void run_test(const char *name, void (*fn)(void)) {
    printf("  %-50s ... ", name);
    fn();
    printf("[PASS]\n");
    g_passed++;
}

static void reset_env(void) {
    material_shutdown();
    remove(TEST_MAT_FILE);
    remove(TEST_SCRAP_FILE);
    int ret = material_init();
    assert(ret == 0);
}

/** 构造一个测试用耗材 */
static Material make_mat(const char *id, const char *name, int category, int attr, double price,
                         int stock, int min_stock) {
    Material m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id, id, sizeof(m.id) - 1);
    strncpy(m.name, name, sizeof(m.name) - 1);
    m.category = category;
    m.attr = attr;
    m.unit_price = price;
    m.total_stock = stock;
    m.min_stock = min_stock;
    strncpy(m.cabinet, "A-01", sizeof(m.cabinet) - 1);
    m.purchase_date = time(NULL);
    return m;
}

/* ============================================================
 * 测试用例
 * ============================================================ */

/** 新增耗材 */
static void test_add_material(void) {
    Material m = make_mat("M001", "电阻 10kΩ", CAT_ELECTRONIC, ATTR_DISPOSABLE, 0.05, 500, 50);
    int ret = material_add(&m);
    assert(ret == 0);
    assert(material_count() == 1);
}

/** 重复编号拒绝 */
static void test_add_duplicate_rejected(void) {
    Material m = make_mat("M001", "另一个电阻", CAT_ELECTRONIC, ATTR_DISPOSABLE, 0.10, 100, 10);
    int ret = material_add(&m);
    assert(ret == -1); /* 编号重复 */
    assert(material_count() == 1);
}

/** 按编号查找 */
static void test_find_by_id(void) {
    const Material *m = material_find_by_id("M001");
    assert(m != NULL);
    assert(strcmp(m->name, "电阻 10kΩ") == 0);
    assert(m->total_stock == 500);

    /* 不存在 */
    m = material_find_by_id("NOEXIST");
    assert(m == NULL);
}

/** 修改耗材 */
static void test_update_material(void) {
    Material m = make_mat("M001", "电阻 10kΩ 1/4W", CAT_ELECTRONIC, ATTR_DISPOSABLE, 0.08, 800, 80);
    int ret = material_update(&m);
    assert(ret == 0);

    const Material *updated = material_find_by_id("M001");
    assert(strcmp(updated->name, "电阻 10kΩ 1/4W") == 0);
    assert(updated->unit_price == 0.08);
    assert(updated->total_stock == 800);
    assert(updated->min_stock == 80);
}

/** 修改不存在的耗材 */
static void test_update_nonexistent(void) {
    Material m = make_mat("NOEXIST", "x", CAT_TOOL, ATTR_REUSABLE, 1.0, 1, 1);
    int ret = material_update(&m);
    assert(ret == -1);
}

/** 删除耗材 */
static void test_delete_material(void) {
    /* 先加一个临时耗材用于删除 */
    Material m = make_mat("TMP001", "临时耗材", CAT_CHEMICAL, ATTR_DISPOSABLE, 1.0, 10, 2);
    material_add(&m);

    int ret = material_delete("TMP001");
    assert(ret == 0);
    assert(material_find_by_id("TMP001") == NULL);
}

/** 删除不存在的耗材 */
static void test_delete_nonexistent(void) {
    int ret = material_delete("NOEXIST");
    assert(ret == -1);
}

/** 扣减库存 */
static void test_reduce_stock(void) {
    int ret = material_reduce_stock("M001", 100);
    assert(ret == 0);

    const Material *m = material_find_by_id("M001");
    assert(m->total_stock == 700); /* 原 800 - 100 */
}

/** 扣减库存不足 */
static void test_reduce_stock_insufficient(void) {
    int ret = material_reduce_stock("M001", 9999);
    assert(ret == -1);

    /* 库存未变 */
    const Material *m = material_find_by_id("M001");
    assert(m->total_stock == 700);
}

/** 增加库存 */
static void test_increase_stock(void) {
    int ret = material_increase_stock("M001", 50);
    assert(ret == 0);

    const Material *m = material_find_by_id("M001");
    assert(m->total_stock == 750);
}

/** 报废扣库存 */
static void test_scrap_reduces_stock(void) {
    int ret = material_scrap("M001", 200, "测试批量报废", "admin");
    assert(ret == 0);

    const Material *m = material_find_by_id("M001");
    assert(m->total_stock == 550); /* 750 - 200 */
}

/** 报废库存不足 */
static void test_scrap_insufficient(void) {
    int ret = material_scrap("M001", 99999, "不可能", "admin");
    assert(ret == -1);
}

/** 报废后生成记录 */
static void test_scrap_creates_record(void) {
    /* 已报废 200 + 尝试报废 99999(失败)，新增一次报废 */
    int ret = material_scrap("M001", 50, "损毁报废", "admin");
    assert(ret == 0);

    /* 验证报废记录列表可正常显示（至少不崩溃） */
    int total_pages = 0;
    material_scrap_list(1, &total_pages);
    assert(total_pages >= 1);
}

/** 分类名称映射 */
static void test_category_names(void) {
    assert(strcmp(material_category_name(CAT_ELECTRONIC), "电子元器件") == 0);
    assert(strcmp(material_category_name(CAT_TOOL), "电工工具") == 0);
    assert(strcmp(material_category_name(CAT_DEV_BOARD), "开发板") == 0);
    assert(strcmp(material_category_name(CAT_CHEMICAL), "化学耗材") == 0);
    assert(strcmp(material_category_name(CAT_MECHANICAL), "机械零件") == 0);
}

/** 属性名称映射 */
static void test_attr_names(void) {
    assert(strcmp(material_attr_name(ATTR_DISPOSABLE), "一次性") == 0);
    assert(strcmp(material_attr_name(ATTR_REUSABLE), "可循环") == 0);
}

/** 添加多条耗材验证分页 */
static void test_pagination(void) {
    reset_env(); /* 清空重来 */

    /* 添加 25 条耗材（3 页） */
    for (int i = 1; i <= 25; i++) {
        char id[16], name[64];
        snprintf(id, sizeof(id), "PG%03d", i);
        snprintf(name, sizeof(name), "测试耗材 %d", i);
        Material m = make_mat(id, name, (i % 5), /* 轮流 5 种分类 */
                              (i % 2),           /* 轮流属性 */
                              10.0 * i,          /* 价格递增 */
                              100 + i,           /* 库存 */
                              30);               /* 预警 30 */
        int ret = material_add(&m);
        assert(ret == 0);
    }
    assert(material_count() == 25);

    /* 验证分页 */
    int total_pages = 0;
    material_list_page(1, &total_pages);
    assert(total_pages == 3); /* 25 条 / 10 = 3 页 */

    material_list_page(3, &total_pages);
    assert(total_pages == 3);
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void) {
    setbuf(stdout, NULL);
    printf("=== 耗材档案管理模块测试 ===\n\n");

    reset_env();

    printf("[CRUD]\n");
    run_test("新增耗材", test_add_material);
    run_test("重复编号拒绝", test_add_duplicate_rejected);
    run_test("按编号查找（存在+不存在）", test_find_by_id);
    run_test("修改耗材字段", test_update_material);
    run_test("修改不存在的耗材返回-1", test_update_nonexistent);
    run_test("删除耗材", test_delete_material);
    run_test("删除不存在的耗材返回-1", test_delete_nonexistent);

    printf("\n[库存操作]\n");
    run_test("扣减库存 100 → 700", test_reduce_stock);
    run_test("库存不足拒绝扣减", test_reduce_stock_insufficient);
    run_test("增加库存 50 → 750", test_increase_stock);

    printf("\n[报废管理]\n");
    run_test("报废 200 扣减库存 → 550", test_scrap_reduces_stock);
    run_test("报废库存不足拒绝", test_scrap_insufficient);
    run_test("报废后生成记录可展示", test_scrap_creates_record);

    printf("\n[名称映射]\n");
    run_test("5 种分类名称正确", test_category_names);
    run_test("2 种属性名称正确", test_attr_names);

    printf("\n[分页]\n");
    run_test("25 条耗材分 3 页", test_pagination);

    /* 清理 */
    material_shutdown();
    remove(TEST_MAT_FILE);
    remove(TEST_SCRAP_FILE);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
