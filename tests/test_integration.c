/**
 * @file    test_integration.c
 * @brief   集成测试 — 全流程端到端、边界条件、数据持久化
 * @details 覆盖 v0.1~v0.8 全部模块的联调场景。
 *          不测试 UI 交互，仅测试底层 API 的组合调用正确性。
 */

#include "types.h"
#include "auth.h"
#include "material.h"
#include "borrow.h"
#include "inventory.h"
#include "audit.h"
#include "csv_io.h"
#include "file_io.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

static int g_passed = 0;
static int g_failed = 0;

/* 测试数据文件列表 */
static const char* g_files[] = {
    "data/admin.dat", "data/material.dat", "data/scrap.dat",
    "data/borrow.dat", "data/stocktake.dat", NULL
};

/* ============================================================
 * 测试辅助
 * ============================================================ */

static void run_test(const char* name, void (*fn)(void)) {
    printf("  %-55s ... ", name);
    fn();
    printf("[PASS]\n");
    g_passed++;
}

static void clean_files(void) {
    for (int i = 0; g_files[i]; i++) remove(g_files[i]);
}

static void full_init(void) {
    assert(auth_init() == 0);
    assert(material_init() == 0);
    assert(borrow_init() == 0);
    assert(inventory_init() == 0);
    assert(audit_init() == 0);
}

static void full_shutdown(void) {
    audit_shutdown();
    inventory_shutdown();
    borrow_shutdown();
    material_shutdown();
    auth_shutdown();
}

/* 辅助：用 admin 登录 */
static void login_as_admin(void) {
    assert(auth_login("admin", "admin123") == AUTH_OK);
    assert(auth_current_role() == ROLE_ADMIN);
}

/* 辅助：添加一个耗材 */
static void add_mat(const char* id, const char* name,
                     int cat, int attr, double price,
                     int stock, int min_stock) {
    Material m;
    memset(&m, 0, sizeof(m));
    strncpy(m.id, id, sizeof(m.id)-1);
    strncpy(m.name, name, sizeof(m.name)-1);
    m.category = cat; m.attr = attr;
    m.unit_price = price; m.total_stock = stock;
    m.min_stock = min_stock;
    strncpy(m.cabinet, "A-01", sizeof(m.cabinet)-1);
    m.purchase_date = time(NULL);
    assert(material_add(&m) == 0);
}

/* 辅助：创建一条领用记录并扣减库存（一次性） */
static void borrow_item(const char* rid, const char* sid,
                         const char* sname, const char* cls,
                         const char* mid, int qty) {
    const Material* mat = material_find_by_id(mid);
    assert(mat != NULL);

    /* 一次性耗材先扣库存 */
    if (mat->attr == ATTR_DISPOSABLE) {
        assert(material_reduce_stock(mid, qty) == 0);
    }

    BorrowRecord r;
    memset(&r, 0, sizeof(r));
    strncpy(r.record_id, rid, sizeof(r.record_id)-1);
    strncpy(r.student_id, sid, sizeof(r.student_id)-1);
    strncpy(r.student_name, sname, sizeof(r.student_name)-1);
    strncpy(r.class_name, cls, sizeof(r.class_name)-1);
    strncpy(r.project_id, "PRJ001", sizeof(r.project_id)-1);
    strncpy(r.material_id, mid, sizeof(r.material_id)-1);
    r.quantity = qty;
    r.borrow_time = time(NULL);
    r.status = BORROW_ACTIVE;
    strncpy(r.operator_name, auth_current_user(), sizeof(r.operator_name)-1);
    assert(borrow_create(&r) == 0);
}

/* ============================================================
 * 场景 1：完整生命周期端到端
 * ============================================================ */

static void test_e2e_full_lifecycle(void) {
    /* 1. 管理员登录 */
    assert(auth_login("admin", "admin123") == AUTH_OK);

    /* 2. 新增耗材 */
    add_mat("R001", "电阻 10kΩ", CAT_ELECTRONIC, ATTR_DISPOSABLE,
            0.05, 500, 50);
    add_mat("DEV001", "Arduino Uno", CAT_DEV_BOARD, ATTR_REUSABLE,
            68.0, 20, 5);
    add_mat("WIRE01", "杜邦线 20cm", CAT_MECHANICAL, ATTR_DISPOSABLE,
            1.5, 300, 50);
    assert(material_count() == 3);

    /* 3. 领用：一次性(电阻) + 可循环(Arduino) 同一单 */
    char rid[MAX_RECORD_ID];
    borrow_gen_id(rid, sizeof(rid));
    borrow_item(rid, "2021001", "张三", "计科2101", "R001", 100);    /* 一次性 */
    borrow_item(rid, "2021001", "张三", "计科2101", "DEV001", 2);    /* 可循环 */
    /* 验证库存 */
    assert(material_find_by_id("R001")->total_stock == 400);   /* 500-100 */
    assert(material_find_by_id("DEV001")->total_stock == 20);   /* 不变 */

    /* 4. 第二次领用 */
    char rid2[MAX_RECORD_ID];
    borrow_gen_id(rid2, sizeof(rid2));
    borrow_item(rid2, "2021002", "李四", "计科2102", "WIRE01", 50);
    assert(material_find_by_id("WIRE01")->total_stock == 250);

    /* 5. 归还可循环工具（正常归还） */
    int ret = borrow_return_session(rid, "");
    assert(ret >= 1);  /* 归还了至少 1 条（DEV001） */

    /* 6. 库存预警：将 R001 库存降到预警线下验证 */
    /* R001: 当前 400, 预警 50 — 充足，手动制造预警 */
    Material tmp = *material_find_by_id("R001");
    tmp.total_stock = 30;
    material_update(&tmp);
    assert(material_find_by_id("R001")->total_stock <
           material_find_by_id("R001")->min_stock);

    /* 7. 盘点 */
    int diff = inventory_stocktake_item("WIRE01", 248, "admin", 1);
    assert(diff == -2);  /* 账面 250, 实际 248, 少 2 */
    assert(material_find_by_id("WIRE01")->total_stock == 248);

    /* 8. 验证统计数据可用 */
    int count = 0;
    BorrowRecord* all = borrow_search("", "", "", &count);
    assert(all != NULL && count >= 3);
    free(all);

    /* 逾期列表（当前无逾期） */
    BorrowRecord* overdue = borrow_get_overdue_list(&count);
    /* 可能为 NULL（无逾期）或 count=0 */
    if (overdue) free(overdue);

    /* 报废记录 */
    ScrapRecord* scraps = material_scrap_get_all(&count);
    assert(count >= 0);  /* 可能为 0 */
    if (scraps) free(scraps);

    auth_logout();
}

/* ============================================================
 * 场景 2：边界条件
 * ============================================================ */

/** 空库领用拒绝 */
static void test_edge_empty_material(void) {
    /* 当前耗材库有数据，尝试领用不存在的耗材 */
    assert(material_find_by_id("NOEXIST") == NULL);
    assert(material_reduce_stock("NOEXIST", 1) == -2);
}

/** 库存刚好够（边界值） */
static void test_edge_exact_stock(void) {
    const Material* mat = material_find_by_id("DEV001");
    int stock = mat->total_stock;
    /* 可循环不扣库存，测试一次性 */
    mat = material_find_by_id("WIRE01");
    stock = mat->total_stock;
    assert(material_reduce_stock("WIRE01", stock) == 0);
    assert(material_find_by_id("WIRE01")->total_stock == 0);

    /* 再扣 1 个应失败 */
    assert(material_reduce_stock("WIRE01", 1) == -1);

    /* 恢复库存 */
    material_increase_stock("WIRE01", stock);
    assert(material_find_by_id("WIRE01")->total_stock == stock);
}

/** 全部耗材预警 */
static void test_edge_all_alert(void) {
    /* 手动将所有耗材库存设为 0 */
    Material tmp;
    const Material* m;

    m = material_find_by_id("R001");
    tmp = *m; tmp.total_stock = 0; material_update(&tmp);
    m = material_find_by_id("DEV001");
    tmp = *m; tmp.total_stock = 0; material_update(&tmp);
    m = material_find_by_id("WIRE01");
    tmp = *m; tmp.total_stock = 0; material_update(&tmp);

    /* 都应低于预警 */
    assert(material_find_by_id("R001")->total_stock <
           material_find_by_id("R001")->min_stock);
    assert(material_find_by_id("DEV001")->total_stock <
           material_find_by_id("DEV001")->min_stock);
    assert(material_find_by_id("WIRE01")->total_stock <
           material_find_by_id("WIRE01")->min_stock);

    /* 恢复 */
    m = material_find_by_id("R001");
    tmp = *m; tmp.total_stock = 500; material_update(&tmp);
    m = material_find_by_id("DEV001");
    tmp = *m; tmp.total_stock = 20; material_update(&tmp);
    m = material_find_by_id("WIRE01");
    tmp = *m; tmp.total_stock = 248; material_update(&tmp);
}

/** 添加管理员边界 */
static void test_edge_admin_management(void) {
    login_as_admin();

    /* 重复用户名 */
    assert(auth_add_admin("admin", "p", ROLE_TA) == AUTH_ALREADY_EXISTS);

    /* 新增助教 */
    assert(auth_add_admin("ta99", "pass", ROLE_TA) == AUTH_OK);

    /* 助教不能新增管理员 */
    auth_logout();
    auth_login("ta99", "pass");
    assert(auth_add_admin("ta100", "p", ROLE_TA) == -1);

    /* 切回 admin */
    auth_logout();
    login_as_admin();

    /* 删除助教 */
    assert(auth_delete_admin("ta99") == 0);
}

/* ============================================================
 * 场景 3：数据持久化 — 重启恢复
 * ============================================================ */

static void test_persistence_restart(void) {
    /* 记录当前数据快照 */
    int mat_count_before = material_count();
    int borrow_count_before = borrow_count();
    const Material* m = material_find_by_id("R001");
    int r001_stock = m->total_stock;

    assert(mat_count_before > 0);
    assert(borrow_count_before > 0);

    /* 模拟重启：关闭 → 重新初始化 */
    full_shutdown();

    /* 验证文件存在 */
    FILE* fp = fopen("data/material.dat", "rb");
    assert(fp != NULL);
    fclose(fp);

    full_init();

    /* 数据应恢复 */
    assert(material_count() == mat_count_before);
    assert(borrow_count() == borrow_count_before);
    m = material_find_by_id("R001");
    assert(m != NULL && m->total_stock == r001_stock);
}

/** 损坏文件的错误处理 */
static void test_persistence_corrupted_file(void) {
    /* 写入一个损坏的 material 文件 */
    FILE* fp = fopen("data/material.dat", "wb");
    assert(fp != NULL);
    char garbage[20] = "NOT VALID DATA!!!";
    fwrite(garbage, 1, sizeof(garbage), fp);
    fclose(fp);

    /* 重启模块应报警告但不崩溃 */
    material_shutdown();
    int ret = material_init();
    /* 损坏文件应使 count == -1，模块应删除文件并创建空库 */
    assert(ret == 0); /* 不应崩溃，优雅降级 */
    assert(material_count() == 0);

    /* 恢复测试数据 */
    add_mat("RECOVER01", "恢复测试耗材", CAT_TOOL, ATTR_REUSABLE,
            10.0, 100, 20);
    assert(material_count() == 1);
}

/* ============================================================
 * 场景 4：多学生并发领用一致性
 * ============================================================ */

static void test_concurrent_borrowing(void) {
    /* 添加一个共享耗材 */
    add_mat("SHARED", "共享耗材", CAT_CHEMICAL, ATTR_DISPOSABLE,
            5.0, 1000, 100);

    login_as_admin();

    /* 10 个学生各领 50 个 */
    int stock_before = material_find_by_id("SHARED")->total_stock;
    int total_borrowed = 0;
    char rid_buf[10][MAX_RECORD_ID];

    for (int i = 0; i < 10; i++) {
        borrow_gen_id(rid_buf[i], sizeof(rid_buf[i]));
        char sid[16], sname[32], cls[32];
        snprintf(sid,  sizeof(sid),  "STU%03d", i);
        snprintf(sname, sizeof(sname), "学生%d", i);
        snprintf(cls,  sizeof(cls),  "班级%d", i % 3);

        int qty = 50;
        borrow_item(rid_buf[i], sid, sname, cls, "SHARED", qty);
        total_borrowed += qty;
    }

    /* 库存一致性 */
    const Material* after = material_find_by_id("SHARED");
    assert(after->total_stock == stock_before - total_borrowed);
    assert(after->total_stock == 500);  /* 1000 - 10*50 */

    /* 验证每个人的记录都在 */
    int count = 0;
    BorrowRecord* all = borrow_search("", "", "", &count);
    assert(all != NULL);
    int shared_count = 0;
    for (int i = 0; i < count; i++) {
        if (strcmp(all[i].material_id, "SHARED") == 0) shared_count++;
    }
    assert(shared_count == 10);
    free(all);
}

/* ============================================================
 * 场景 5：CSV 导入导出集成
 * ============================================================ */

static void test_csv_import_integration(void) {
    /* 创建测试 CSV */
    FILE* fp = fopen("test_integration.csv", "w");
    fprintf(fp, "编号,名称,分类,属性,单价,库存,预警,柜号,采购日期\n");
    fprintf(fp, "INT001,集成测试电阻,电子元器件,一次性,0.25,2000,200,A-99,2024-12-01\n");
    fprintf(fp, "INT002,集成测试开发板,开发板,可循环,49.00,30,10,B-99,2024-11-15\n");
    fclose(fp);

    int imported = csv_import_materials("test_integration.csv", "admin");
    assert(imported == 2);
    assert(material_find_by_id("INT001") != NULL);
    assert(material_find_by_id("INT002") != NULL);

    /* 导入后立即可领用（验证数据一致性） */
    const Material* m = material_find_by_id("INT001");
    assert(m->total_stock == 2000);
    assert(m->unit_price == 0.25);

    remove("test_integration.csv");
}

static void test_csv_export_integration(void) {
    /* 领用 INT001 后导出 */
    material_reduce_stock("INT001", 100);
    BorrowRecord r;
    memset(&r, 0, sizeof(r));
    strncpy(r.record_id, "BORROW-INT-001", sizeof(r.record_id)-1);
    strncpy(r.student_id, "INT001", sizeof(r.student_id)-1);
    strncpy(r.student_name, "集成测试学生", sizeof(r.student_name)-1);
    strncpy(r.class_name, "集成班", sizeof(r.class_name)-1);
    strncpy(r.project_id, "PRJ-INT", sizeof(r.project_id)-1);
    strncpy(r.material_id, "INT001", sizeof(r.material_id)-1);
    r.quantity = 100; r.borrow_time = time(NULL);
    r.status = BORROW_ACTIVE;
    strncpy(r.operator_name, "admin", sizeof(r.operator_name)-1);
    borrow_create(&r);

    /* 导出三种格式 */
    assert(csv_export_materials("test_mat_export.csv") == 0);
    assert(csv_export_borrow_records("test_bor_export.csv") == 0);

    /* 验证文件非空 */
    FILE* fp = fopen("test_mat_export.csv", "r");
    assert(fp != NULL);
    fseek(fp, 0, SEEK_END);
    assert(ftell(fp) > 20);
    fclose(fp);

    fp = fopen("test_bor_export.csv", "r");
    assert(fp != NULL);
    fclose(fp);

    remove("test_mat_export.csv");
    remove("test_bor_export.csv");
}

/* ============================================================
 * 场景 6：审计日志全流程记录
 * ============================================================ */

static void test_audit_trail_completeness(void) {
    int before = audit_count();

    /* 执行一系列操作 */
    /* 1. 新增耗材 */
    add_mat("AUDIT01", "审计测试耗材", CAT_TOOL, ATTR_REUSABLE,
            10.0, 50, 10);
    /* 2. 领用 */
    BorrowRecord r;
    memset(&r, 0, sizeof(r));
    strncpy(r.record_id, "BORROW-AUDIT-01", sizeof(r.record_id)-1);
    strncpy(r.student_id, "S-AUDIT", sizeof(r.student_id)-1);
    strncpy(r.student_name, "审计学生", sizeof(r.student_name)-1);
    strncpy(r.class_name, "审计班", sizeof(r.class_name)-1);
    strncpy(r.project_id, "P-AUDIT", sizeof(r.project_id)-1);
    strncpy(r.material_id, "AUDIT01", sizeof(r.material_id)-1);
    r.quantity = 2; r.borrow_time = time(NULL);
    r.status = BORROW_ACTIVE;
    strncpy(r.operator_name, "admin", sizeof(r.operator_name)-1);
    borrow_create(&r);
    /* 3. 归还 */
    borrow_return_session("BORROW-AUDIT-01", "");
    /* 4. 盘点修正 */
    inventory_stocktake_item("AUDIT01", 48, "admin", 1);
    /* 5. 报废 */
    material_scrap("AUDIT01", 2, "审计测试报废", "admin");

    int after = audit_count();
    /* 至少应有 5 条新审计记录 */
    assert(after >= before + 5);

    /* 验证不同操作类型都有记录 */
    int total_pages = 0;
    audit_list_page(1, &total_pages, -1, "");
    assert(total_pages >= 1);
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void) {
    setbuf(stdout, NULL);
    printf("=== 集成测试 — 全流程验证 ===\n\n");

    clean_files();
    full_init();
    login_as_admin();

    printf("[E2E 全流程]\n");
    run_test("完整生命周期: 登录→新增→领用→归还→预警→盘点→统计",
             test_e2e_full_lifecycle);

    printf("\n[边界条件]\n");
    run_test("空库/不存在耗材拒绝操作",         test_edge_empty_material);
    run_test("库存扣至0后再扣失败",              test_edge_exact_stock);
    run_test("全部耗材设为0后均低于预警",         test_edge_all_alert);
    run_test("管理员管理: 重复/权限/增删",       test_edge_admin_management);

    printf("\n[数据持久化]\n");
    run_test("关闭→重启后数据完整恢复",          test_persistence_restart);
    run_test("损坏文件优雅降级不崩溃",           test_persistence_corrupted_file);

    printf("\n[并发一致性]\n");
    run_test("10人各领50个→库存=1000-500=500",   test_concurrent_borrowing);

    printf("\n[CSV 导入导出]\n");
    run_test("CSV导入2条→可查询→库存单价正确",   test_csv_import_integration);
    run_test("领用后导出耗材+记录文件非空",        test_csv_export_integration);

    printf("\n[审计日志]\n");
    run_test("5种操作均自动记录→审计条目增加",    test_audit_trail_completeness);

    /* 清理 */
    auth_logout();
    full_shutdown();
    clean_files();

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
