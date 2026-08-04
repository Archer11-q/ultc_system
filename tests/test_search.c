/**
 * @file    test_search.c
 * @brief   检索模块测试 — 精准查询 / 模糊匹配 / 多条件筛选
 */

#include "types.h"
#include "material.h"
#include "borrow.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int g_passed = 0;
static int g_failed = 0;

static void run_test(const char* name, void (*fn)(void)) {
    printf("  %-50s ... ", name);
    fn();
    printf("[PASS]\n");
    g_passed++;
}

static void reset_env(void) {
    borrow_shutdown();
    material_shutdown();
    remove("data/material.dat");
    remove("data/scrap.dat");
    remove("data/borrow.dat");
    assert(material_init() == 0);
    assert(borrow_init() == 0);
}

static void add_test_data(void) {
    Material m;
    memset(&m, 0, sizeof(m));
    strncpy(m.cabinet, "A-01", sizeof(m.cabinet)-1);
    m.purchase_date = time(NULL);

    strncpy(m.id, "R001", sizeof(m.id)-1);
    strncpy(m.name, "电阻 10kΩ 1/4W", sizeof(m.name)-1);
    m.category = CAT_ELECTRONIC; m.attr = ATTR_DISPOSABLE;
    m.unit_price = 0.05; m.total_stock = 500; m.min_stock = 50;
    material_add(&m);

    strncpy(m.id, "R002", sizeof(m.id)-1);
    strncpy(m.name, "电阻 100Ω 1/2W", sizeof(m.name)-1);
    m.category = CAT_ELECTRONIC; m.attr = ATTR_DISPOSABLE;
    m.unit_price = 0.10; m.total_stock = 200; m.min_stock = 30;
    material_add(&m);

    strncpy(m.id, "DEV001", sizeof(m.id)-1);
    strncpy(m.name, "Arduino Uno R3 开发板", sizeof(m.name)-1);
    m.category = CAT_DEV_BOARD; m.attr = ATTR_REUSABLE;
    m.unit_price = 68.0; m.total_stock = 10; m.min_stock = 3;
    material_add(&m);

    strncpy(m.id, "WIRE01", sizeof(m.id)-1);
    strncpy(m.name, "杜邦线 公母 20cm", sizeof(m.name)-1);
    m.category = CAT_MECHANICAL; m.attr = ATTR_DISPOSABLE;
    m.unit_price = 1.5; m.total_stock = 300; m.min_stock = 50;
    material_add(&m);

    /* 添加领用记录用于多条件筛选测试 */
    BorrowRecord r;
    memset(&r, 0, sizeof(r));
    r.borrow_time = time(NULL);
    r.status = BORROW_ACTIVE;
    strncpy(r.operator_name, "admin", sizeof(r.operator_name)-1);

    strncpy(r.record_id, "BORROW-20260804-101", sizeof(r.record_id)-1);
    strncpy(r.student_id, "2021001", sizeof(r.student_id)-1);
    strncpy(r.student_name, "张三", sizeof(r.student_name)-1);
    strncpy(r.class_name, "计科2101", sizeof(r.class_name)-1);
    strncpy(r.project_id, "PRJ001", sizeof(r.project_id)-1);
    strncpy(r.material_id, "R001", sizeof(r.material_id)-1);
    r.quantity = 100;
    borrow_create(&r);

    strncpy(r.record_id, "BORROW-20260804-102", sizeof(r.record_id)-1);
    strncpy(r.student_id, "2021002", sizeof(r.student_id)-1);
    strncpy(r.student_name, "李四", sizeof(r.student_name)-1);
    strncpy(r.class_name, "计科2102", sizeof(r.class_name)-1);
    strncpy(r.project_id, "PRJ002", sizeof(r.project_id)-1);
    strncpy(r.material_id, "DEV001", sizeof(r.material_id)-1);
    r.quantity = 1;
    borrow_create(&r);

    strncpy(r.record_id, "BORROW-20260804-103", sizeof(r.record_id)-1);
    strncpy(r.student_id, "2021001", sizeof(r.student_id)-1);
    strncpy(r.student_name, "张三", sizeof(r.student_name)-1);
    strncpy(r.class_name, "计科2101", sizeof(r.class_name)-1);
    strncpy(r.project_id, "PRJ001", sizeof(r.project_id)-1);
    strncpy(r.material_id, "WIRE01", sizeof(r.material_id)-1);
    r.quantity = 20;
    borrow_create(&r);
}

/* ============================================================
 * 精准查询
 * ============================================================ */

static void test_find_by_id_exists(void) {
    const Material* m = material_find_by_id("DEV001");
    assert(m != NULL);
    assert(strcmp(m->name, "Arduino Uno R3 开发板") == 0);
}

static void test_find_by_id_not_exists(void) {
    const Material* m = material_find_by_id("NOPE99");
    assert(m == NULL);
}

/* ============================================================
 * 模糊匹配
 * ============================================================ */

static void test_fuzzy_single_match(void) {
    int count = 0;
    Material* results = material_search_by_name("Arduino", &count);
    assert(results != NULL);
    assert(count == 1);
    assert(strcmp(results[0].id, "DEV001") == 0);
    free(results);
}

static void test_fuzzy_multiple_matches(void) {
    int count = 0;
    Material* results = material_search_by_name("电阻", &count);
    assert(results != NULL);
    assert(count == 2);  /* R001 + R002 */
    free(results);
}

static void test_fuzzy_no_match(void) {
    int count = 0;
    Material* results = material_search_by_name("树莓派", &count);
    assert(results == NULL);
    assert(count == 0);
}

static void test_fuzzy_empty_keyword(void) {
    int count = -1;
    Material* results = material_search_by_name("", &count);
    assert(results == NULL);
    assert(count == 0);
}

static void test_fuzzy_substring_match(void) {
    /* "杜邦" 匹配 "杜邦线 公母 20cm" */
    int count = 0;
    Material* results = material_search_by_name("杜邦", &count);
    assert(results != NULL);
    assert(count == 1);
    assert(strcmp(results[0].id, "WIRE01") == 0);
    free(results);
}

/* ============================================================
 * 多条件筛选
 * ============================================================ */

static void test_search_by_class(void) {
    int count = 0;
    BorrowRecord* r = borrow_search("计科2101", "", "", &count);
    assert(r != NULL);
    assert(count == 2);  /* 张三的 2 条 */
    free(r);
}

static void test_search_by_student(void) {
    int count = 0;
    BorrowRecord* r = borrow_search("", "2021002", "", &count);
    assert(r != NULL);
    assert(count == 1);  /* 李四的 1 条 */
    assert(strcmp(r[0].student_name, "李四") == 0);
    free(r);
}

static void test_search_combined(void) {
    int count = 0;
    BorrowRecord* r = borrow_search("计科2101", "2021001", "PRJ001", &count);
    assert(r != NULL);
    assert(count == 2);
    free(r);
}

static void test_search_no_match(void) {
    int count = 0;
    BorrowRecord* r = borrow_search("不存在的班级", "", "", &count);
    assert(r == NULL);
    assert(count == 0);
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void) {
    setbuf(stdout, NULL);
    printf("=== 检索模块测试 ===\n\n");

    reset_env();
    add_test_data();

    printf("[精准查询]\n");
    run_test("按编号查找到 DEV001",               test_find_by_id_exists);
    run_test("不存在的编号返回 NULL",              test_find_by_id_not_exists);

    printf("\n[模糊匹配]\n");
    run_test("\"Arduino\" 匹配 1 条",             test_fuzzy_single_match);
    run_test("\"电阻\" 匹配 2 条",                test_fuzzy_multiple_matches);
    run_test("\"树莓派\" 无匹配返回 NULL",         test_fuzzy_no_match);
    run_test("空关键词返回 NULL",                  test_fuzzy_empty_keyword);
    run_test("\"杜邦\" 子串匹配成功",              test_fuzzy_substring_match);

    printf("\n[多条件筛选]\n");
    run_test("按班级筛选 2 条",                    test_search_by_class);
    run_test("按学号筛选 1 条",                    test_search_by_student);
    run_test("组合条件筛选 2 条",                  test_search_combined);
    run_test("无匹配返回 NULL",                    test_search_no_match);

    borrow_shutdown();
    material_shutdown();
    remove("data/material.dat");
    remove("data/scrap.dat");
    remove("data/borrow.dat");

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
