/**
 * @file    test_file_io.c
 * @brief   二进制文件 IO 模块测试
 * @details 测试项：
 *          1. 写入+读取往返校验（基本功能）
 *          2. 空数据写入+读取
 *          3. 魔数校验 — 损坏文件应被拒绝
 */

#include "types.h"
#include "file_io.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST_FILE "data/test_io.dat"

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

/* 定义一个测试用结构体 */
#pragma pack(push, 1)
typedef struct {
    int   id;
    char  name[32];
    double value;
} TestItem;
#pragma pack(pop)

/* ============================================================
 * 测试用例
 * ============================================================ */

/** 测试1：写入 3 条记录，读出后逐字段比对 */
static void test_write_read_roundtrip(void) {
    /* 准备数据 */
    TestItem items[3];
    items[0].id = 100;
    strncpy(items[0].name, "电阻-10kΩ", sizeof(items[0].name));
    items[0].value = 0.05;

    items[1].id = 200;
    strncpy(items[1].name, "Arduino Uno", sizeof(items[1].name));
    items[1].value = 68.00;

    items[2].id = 300;
    strncpy(items[2].name, "杜邦线-公母", sizeof(items[2].name));
    items[2].value = 1.50;

    /* 写入 */
    int ret = file_write_all(TEST_FILE, items, sizeof(TestItem), 3);
    assert(ret == 0);

    /* 读出 */
    int count = 0;
    TestItem* read = (TestItem*)file_read_all(TEST_FILE, sizeof(TestItem), &count);
    assert(read != NULL);
    assert(count == 3);

    /* 逐字段比对 */
    assert(read[0].id == 100);
    assert(strcmp(read[0].name, "电阻-10kΩ") == 0);
    assert(read[0].value == 0.05);

    assert(read[1].id == 200);
    assert(strcmp(read[1].name, "Arduino Uno") == 0);
    assert(read[1].value == 68.00);

    assert(read[2].id == 300);
    assert(strcmp(read[2].name, "杜邦线-公母") == 0);
    assert(read[2].value == 1.50);

    free(read);
}

/** 测试2：写入 0 条记录，读出应返回 NULL + count=0 */
static void test_write_read_empty(void) {
    int ret = file_write_all(TEST_FILE, NULL, sizeof(TestItem), 0);
    assert(ret == 0);

    int count = -999;
    void* data = file_read_all(TEST_FILE, sizeof(TestItem), &count);
    assert(data == NULL);
    assert(count == 0);
}

/** 测试3：损坏文件 — 写入正确数据后改坏魔数，读取应失败 */
static void test_corrupted_magic(void) {
    /* 先写一条正常数据 */
    TestItem item = { 42, "test", 1.0 };
    int ret = file_write_all(TEST_FILE, &item, sizeof(TestItem), 1);
    assert(ret == 0);

    /* 修改文件头第一字节，破坏魔数 */
    FILE* fp = fopen(TEST_FILE, "r+b");
    assert(fp != NULL);
    char corrupt = 0x00;
    fwrite(&corrupt, 1, 1, fp);  /* 覆盖魔数第一字节 */
    fclose(fp);

    /* 读取应失败 */
    int count = 0;
    void* data = file_read_all(TEST_FILE, sizeof(TestItem), &count);
    assert(data == NULL);
    assert(count == -1);  /* 损坏文件返回 -1 */
}

/** 测试4：文件不存在时返回 NULL + count=0 */
static void test_file_not_exist(void) {
    int count = -999;
    void* data = file_read_all("data/nonexistent.dat", sizeof(TestItem), &count);
    assert(data == NULL);
    assert(count == 0);
}

/** 测试5：读取回真实的结构体大小（用 Material 验证） */
static void test_with_material_struct(void) {
    /* 准备 Material 数据 */
    Material mats[2];
    memset(&mats, 0, sizeof(mats));
    strncpy(mats[0].id, "MAT-001", sizeof(mats[0].id));
    strncpy(mats[0].name, "电阻 10kΩ 1/4W", sizeof(mats[0].name));
    mats[0].category = CAT_ELECTRONIC;
    mats[0].attr     = ATTR_DISPOSABLE;
    mats[0].unit_price = 0.05;
    mats[0].total_stock = 500;
    mats[0].min_stock   = 50;
    strncpy(mats[0].cabinet, "A-03", sizeof(mats[0].cabinet));
    mats[0].purchase_date = 1717171200;  /* 2024-06-01 */

    strncpy(mats[1].id, "MAT-002", sizeof(mats[1].id));
    strncpy(mats[1].name, "Arduino Uno R3", sizeof(mats[1].name));
    mats[1].category = CAT_DEV_BOARD;
    mats[1].attr     = ATTR_REUSABLE;
    mats[1].unit_price = 68.00;
    mats[1].total_stock = 20;
    mats[1].min_stock   = 5;
    strncpy(mats[1].cabinet, "B-07", sizeof(mats[1].cabinet));
    mats[1].purchase_date = 1725148800;  /* 2024-09-01 */

    /* 写入 */
    int ret = file_write_all(TEST_FILE, mats, sizeof(Material), 2);
    assert(ret == 0);

    /* 读出 */
    int count = 0;
    Material* read = (Material*)file_read_all(TEST_FILE, sizeof(Material), &count);
    assert(read != NULL);
    assert(count == 2);

    assert(strcmp(read[0].id, "MAT-001") == 0);
    assert(read[0].category == CAT_ELECTRONIC);
    assert(read[0].attr == ATTR_DISPOSABLE);
    assert(read[0].total_stock == 500);

    assert(strcmp(read[1].id, "MAT-002") == 0);
    assert(read[1].category == CAT_DEV_BOARD);
    assert(read[1].attr == ATTR_REUSABLE);
    assert(read[1].total_stock == 20);

    free(read);
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void) {
    setbuf(stdout, NULL);
    printf("=== 二进制文件 IO 模块测试 ===\n\n");

    printf("[基本功能]\n");
    run_test("写入+读取 3 条往返校验",      test_write_read_roundtrip);
    run_test("写入+读取 0 条（空数据）",    test_write_read_empty);

    printf("\n[异常处理]\n");
    run_test("损坏魔数时返回错误 count=-1",  test_corrupted_magic);
    run_test("文件不存在时返回 NULL count=0", test_file_not_exist);

    printf("\n[真实结构体]\n");
    run_test("Material 结构体写入+读取 2 条", test_with_material_struct);

    /* 清理测试文件 */
    remove(TEST_FILE);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
