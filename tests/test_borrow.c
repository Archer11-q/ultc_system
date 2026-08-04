/**
 * @file    test_borrow.c
 * @brief   领用/归还模块测试
 * @details 测试项覆盖：
 *          - 领用单号生成
 *          - 创建领用记录 + 按单号查询
 *          - 一次性耗材领用扣库存 / 可循环仅登记
 *          - 库存不足拒绝领用（在主菜单层测试，此处测 borrow_create）
 *          - 按学号查询未归还记录
 *          - 归还操作（正常归还 + 损坏报废）
 *          - 逾期判定
 */

#include "types.h"
#include "material.h"
#include "borrow.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_BORROW_FILE "data/borrow.dat"
#define TEST_MAT_FILE    "data/material.dat"
#define TEST_SCRAP_FILE  "data/scrap.dat"

static int g_passed = 0;
static int g_failed = 0;

/* ============================================================
 * 测试辅助
 * ============================================================ */

static void run_test(const char* name, void (*fn)(void)) {
    printf("  %-50s ... ", name);
    fn();
    printf("[PASS]\n");
    g_passed++;
}

static void reset_env(void) {
    borrow_shutdown();
    material_shutdown();
    remove(TEST_BORROW_FILE);
    remove(TEST_MAT_FILE);
    remove(TEST_SCRAP_FILE);
    assert(material_init() == 0);
    assert(borrow_init() == 0);
}

/** 创建一个测试用耗材 */
static void add_test_material(void) {
    Material m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id, "R001", sizeof(m.id) - 1);
    strncpy(m.name, "电阻 10kΩ", sizeof(m.name) - 1);
    m.category = CAT_ELECTRONIC;
    m.attr = ATTR_DISPOSABLE;
    m.unit_price = 0.05;
    m.total_stock = 500;
    m.min_stock = 50;
    strncpy(m.cabinet, "A-01", sizeof(m.cabinet) - 1);
    m.purchase_date = time(NULL);
    material_add(&m);

    strncpy(m.id, "DEV001", sizeof(m.id) - 1);
    strncpy(m.name, "Arduino Uno", sizeof(m.name) - 1);
    m.category = CAT_DEV_BOARD;
    m.attr = ATTR_REUSABLE;
    m.unit_price = 68.00;
    m.total_stock = 20;
    m.min_stock = 5;
    strncpy(m.cabinet, "B-07", sizeof(m.cabinet) - 1);
    material_add(&m);
}

/** 构造一条 BorrowRecord */
static BorrowRecord make_borrow(const char* record_id,
                                 const char* student_id,
                                 const char* material_id,
                                 int quantity) {
    BorrowRecord r;
    memset(&r, 0, sizeof(r));
    strncpy(r.record_id,    record_id,    sizeof(r.record_id) - 1);
    strncpy(r.student_id,   student_id,   sizeof(r.student_id) - 1);
    strncpy(r.student_name, "测试学生",    sizeof(r.student_name) - 1);
    strncpy(r.class_name,   "计科2101",    sizeof(r.class_name) - 1);
    strncpy(r.project_id,   "PRJ001",      sizeof(r.project_id) - 1);
    strncpy(r.material_id,  material_id,   sizeof(r.material_id) - 1);
    r.quantity    = quantity;
    r.borrow_time = time(NULL);
    r.status      = BORROW_ACTIVE;
    strncpy(r.operator_name, "admin", sizeof(r.operator_name) - 1);
    return r;
}

/* ============================================================
 * 测试用例
 * ============================================================ */

/** 领用单号生成 */
static void test_gen_borrow_id(void) {
    char id[MAX_RECORD_ID];
    borrow_gen_id(id, sizeof(id));
    /* 格式: BORROW-YYYYMMDD-NNN */
    assert(strncmp(id, "BORROW-", 7) == 0);
    /* 日期部分是 8 位数字 */
    assert(strlen(id) >= 19);
}

/** 创建领用记录 */
static void test_create_borrow(void) {
    BorrowRecord r = make_borrow("BORROW-20260804-001",
                                  "2021001", "R001", 100);
    int ret = borrow_create(&r);
    assert(ret == 0);
    assert(borrow_count() == 1);
}

/** 按单号查询 */
static void test_get_by_record_id(void) {
    int count = 0;
    BorrowRecord* items = borrow_get_by_record_id("BORROW-20260804-001", &count);
    assert(items != NULL);
    assert(count == 1);
    assert(strcmp(items[0].student_id, "2021001") == 0);
    assert(items[0].quantity == 100);
    free(items);
}

/** 查询不存在的单号 */
static void test_get_nonexistent_record(void) {
    int count = -1;
    BorrowRecord* items = borrow_get_by_record_id("BORROW-99999999-999", &count);
    assert(items == NULL);
    assert(count == 0);
}

/** 一次性耗材领用后库存扣减（模拟真实流程） */
static void test_disposable_reduces_stock(void) {
    /* 领用前库存 */
    const Material* before = material_find_by_id("R001");
    int stock_before = before->total_stock;

    /* 扣减库存 */
    int ret = material_reduce_stock("R001", 50);
    assert(ret == 0);

    /* 创建记录 */
    BorrowRecord r = make_borrow("BORROW-20260804-002",
                                  "2021002", "R001", 50);
    borrow_create(&r);

    /* 验证库存变化 */
    const Material* after = material_find_by_id("R001");
    assert(after->total_stock == stock_before - 50);
}

/** 可循环耗材领用不扣库存 */
static void test_reusable_no_stock_change(void) {
    const Material* before = material_find_by_id("DEV001");
    int stock_before = before->total_stock;

    /* 注意：可循环耗材不调用 material_reduce_stock，直接创建记录 */
    BorrowRecord r = make_borrow("BORROW-20260804-003",
                                  "2021003", "DEV001", 2);
    borrow_create(&r);

    /* 库存不应变化 */
    const Material* after = material_find_by_id("DEV001");
    assert(after->total_stock == stock_before);
}

/** 库存不足时 refuse（material_reduce_stock 返回 -1） */
static void test_disposable_insufficient_stock(void) {
    const Material* mat = material_find_by_id("R001");
    int ret = material_reduce_stock("R001", mat->total_stock + 1);
    assert(ret == -1);
}

/** 同一领用单多条记录 */
static void test_multiple_items_same_record(void) {
    /* R001 一次性 + DEV001 可循环 */
    material_reduce_stock("R001", 10);
    BorrowRecord r1 = make_borrow("BORROW-20260804-004",
                                   "2021004", "R001", 10);
    borrow_create(&r1);

    BorrowRecord r2 = make_borrow("BORROW-20260804-004",
                                   "2021004", "DEV001", 1);
    borrow_create(&r2);

    int count = 0;
    BorrowRecord* items = borrow_get_by_record_id("BORROW-20260804-004", &count);
    assert(items != NULL);
    assert(count == 2);
    free(items);
}

/** 按学号查询未归还记录 */
static void test_unreturned_by_student(void) {
    int count = 0;
    BorrowRecord* items = borrow_get_unreturned_by_student("2021004", &count);
    assert(items != NULL);
    assert(count == 2);
    free(items);

    /* 无记录的学号 */
    items = borrow_get_unreturned_by_student("NOEXIST", &count);
    assert(items == NULL);
    assert(count == 0);
}

/** 批量归还（borrow_return_session） */
static void test_return_session(void) {
    /* BORROW-20260804-004 下有2条记录（R001一次性+DEV001可循环） */
    int ret = borrow_return_session("BORROW-20260804-004", "");
    assert(ret == 2);  /* 归还了2条 */

    /* 再次查询应排除已归还的 */
    int count = 0;
    BorrowRecord* items = borrow_get_unreturned_by_student("2021004", &count);
    assert(count == 0);  /* 全部归还 */
    free(items);
}

/** 已全部归还后再归还返回 0 */
static void test_return_already_returned(void) {
    int ret = borrow_return_session("BORROW-20260804-004", "");
    assert(ret == 0);  /* 没有未归还的了 */
}

/** 损坏归还 → 报废（含 scrap 联动） */
static void test_return_with_damage_and_scrap(void) {
    /* 先创建一条可循环工具的领用记录 */
    BorrowRecord r = make_borrow("BORROW-20260804-005",
                                  "2021005", "DEV001", 2);
    r.borrow_time = time(NULL) - 86400;  /* 1 天前 */
    borrow_create(&r);

    /* 记录归还前库存 */
    const Material* mat_before = material_find_by_id("DEV001");
    int stock_before = mat_before->total_stock;

    /* 模拟完整归还+报废流程：先 material_scrap，再 borrow_return_session */
    int scrap_ret = material_scrap("DEV001", 2, "归还时发现屏幕碎裂", "admin");
    assert(scrap_ret == 0);

    int ret = borrow_return_session("BORROW-20260804-005", "归还时发现屏幕碎裂");
    assert(ret == 1);

    /* 验证状态 */
    int count = 0;
    BorrowRecord* items = borrow_get_by_record_id("BORROW-20260804-005", &count);
    assert(items != NULL);
    assert(items[0].status == BORROW_SCRAPPED);
    assert(strcmp(items[0].damage_note, "归还时发现屏幕碎裂") == 0);
    free(items);

    /* 验证库存扣减 */
    const Material* mat_after = material_find_by_id("DEV001");
    assert(mat_after->total_stock == stock_before - 2);
}

/** 正常归还无损坏（不扣库存） */
static void test_return_normal_no_scrap(void) {
    /* 创建可循环工具领用 */
    BorrowRecord r = make_borrow("BORROW-20260804-006",
                                  "2021006", "DEV001", 1);
    borrow_create(&r);

    const Material* mat_before = material_find_by_id("DEV001");
    int stock_before = mat_before->total_stock;

    /* 正常归还（无损坏） */
    int ret = borrow_return_session("BORROW-20260804-006", "");
    assert(ret == 1);

    /* 库存不应变化 */
    const Material* mat_after = material_find_by_id("DEV001");
    assert(mat_after->total_stock == stock_before);

    int count = 0;
    BorrowRecord* items = borrow_get_by_record_id("BORROW-20260804-006", &count);
    assert(items[0].status == BORROW_RETURNED);
    free(items);
}

/** 逾期判定 */
static void test_overdue_detection(void) {
    /* 创建一条 8 天前的领用记录 */
    BorrowRecord r = make_borrow("BORROW-20260804-OVERDUE",
                                  "2021099", "DEV001", 1);
    r.borrow_time = time(NULL) - 8 * 86400;  /* 8 天前 */
    borrow_create(&r);

    int count = 0;
    BorrowRecord* overdue = borrow_get_overdue_list(&count);
    /* 至少包含这条逾期记录 */
    int found = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(overdue[i].record_id, "BORROW-20260804-OVERDUE") == 0) {
            found = 1;
            break;
        }
    }
    assert(found == 1);
    free(overdue);
}

/** 多条件检索 */
static void test_search(void) {
    int count = 0;

    /* 按班级 */
    BorrowRecord* r = borrow_search("计科2101", "", "", &count);
    assert(r != NULL && count > 0);
    free(r);

    /* 按学号 */
    r = borrow_search("", "2021001", "", &count);
    assert(r != NULL && count == 1);
    free(r);

    /* 按项目 */
    r = borrow_search("", "", "PRJ001", &count);
    assert(r != NULL && count > 0);
    free(r);

    /* 组合条件 */
    r = borrow_search("计科2101", "2021001", "PRJ001", &count);
    assert(r != NULL && count == 1);
    free(r);

    /* 无匹配 */
    r = borrow_search("不存在的班级", "", "", &count);
    assert(r == NULL && count == 0);
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void) {
    setbuf(stdout, NULL);
    printf("=== 领用 / 归还模块测试 ===\n\n");

    reset_env();
    add_test_material();

    printf("[领用]\n");
    run_test("领用单号格式 BORROW-YYYYMMDD-NNN",   test_gen_borrow_id);
    run_test("创建领用记录",                       test_create_borrow);
    run_test("按单号查询",                         test_get_by_record_id);
    run_test("查询不存在的单号返回 NULL",           test_get_nonexistent_record);
    run_test("一次性耗材领用扣减库存",             test_disposable_reduces_stock);
    run_test("可循环耗材领用不扣库存",             test_reusable_no_stock_change);
    run_test("库存不足拒绝扣减",                   test_disposable_insufficient_stock);
    run_test("同一领用单包含多种耗材",             test_multiple_items_same_record);

    printf("\n[查询]\n");
    run_test("按学号查询未归还记录",               test_unreturned_by_student);

    printf("\n[归还]\n");
    run_test("批量归还 2 条 → 全部已归还",         test_return_session);
    run_test("全部已归还后再归还返回 0",            test_return_already_returned);
    run_test("损坏归还+scrap联动 → 库存扣减+报废",  test_return_with_damage_and_scrap);
    run_test("正常归还无损坏 → 库存不变",          test_return_normal_no_scrap);

    printf("\n[逾期]\n");
    run_test("8天前记录被判为逾期",                test_overdue_detection);

    printf("\n[检索]\n");
    run_test("多条件筛选（班级/学号/项目）",        test_search);

    /* 清理 */
    borrow_shutdown();
    material_shutdown();
    remove(TEST_BORROW_FILE);
    remove(TEST_MAT_FILE);
    remove(TEST_SCRAP_FILE);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
