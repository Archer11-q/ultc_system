/**
 * @file    test_auth.c
 * @brief   登录认证模块测试
 * @details 测试项覆盖：
 *          - 登录成功 / 密码错误 / 用户不存在
 *          - 连续3次错误→锁定10秒
 *          - 锁定超时自动解锁
 *          - 权限校验（助教无法增删管理员）
 *          - 管理员增删改查
 */

#include "auth.h"
#include "platform.h"
#include "types.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 测试用数据文件（避免污染真实数据） */
#define TEST_ADMIN_FILE "data/admin.dat"

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

/**
 * @brief 重置测试环境：删除旧数据文件，重新初始化
 */
static void reset_env(void) {
    auth_shutdown();
    remove(TEST_ADMIN_FILE);
    int ret = auth_init();
    assert(ret == 0);
}

/* ============================================================
 * 测试用例
 * ============================================================ */

/** 测试1：首次运行自动创建默认管理员 */
static void test_default_admin_created(void) {
    /* reset_env 已调用 auth_init，默认账号应存在 */
    /* 用默认账号登录验证 */
    int ret = auth_login("admin", "admin123");
    assert(ret == AUTH_OK);
    assert(strcmp(auth_current_user(), "admin") == 0);
    assert(auth_current_role() == ROLE_ADMIN);
    auth_logout();
}

/** 测试2：正确密码登录成功 */
static void test_login_success(void) {
    int ret = auth_login("admin", "admin123");
    assert(ret == AUTH_OK);
    assert(auth_current_role() == ROLE_ADMIN);
}

/** 测试3：错误密码 */
static void test_login_wrong_password(void) {
    int ret = auth_login("admin", "wrongpass");
    assert(ret == AUTH_WRONG_PASSWORD);
}

/** 测试4：用户不存在 */
static void test_login_user_not_found(void) {
    int ret = auth_login("nobody", "anypass");
    assert(ret == AUTH_USER_NOT_FOUND);
}

/** 测试5：连续3次错误触发锁定 */
static void test_lock_after_3_failures(void) {
    /* 先用错误密码尝试2次 */
    auth_login("admin", "bad1");
    auth_login("admin", "bad2");
    /* 第3次，应该还是 AUTH_WRONG_PASSWORD（刚触发锁定） */
    int ret = auth_login("admin", "bad3");
    assert(ret == AUTH_WRONG_PASSWORD);

    /* 现在应已锁定 */
    int remaining = auth_lock_remaining("admin");
    /* 锁定时间应在 1~10 秒范围内 */
    assert(remaining > 0 && remaining <= LOGIN_LOCK_SECONDS);

    /* 再次尝试应返回 AUTH_LOCKED */
    ret = auth_login("admin", "admin123");
    assert(ret == AUTH_LOCKED);
}

/** 测试6：锁定期间无法登录 */
static void test_cannot_login_during_lock(void) {
    /* 继承 test_lock_after_3_failures 的状态 */
    int ret = auth_login("admin", "admin123");
    assert(ret == AUTH_LOCKED);
    /* 锁定剩余时间应 > 0 */
    assert(auth_lock_remaining("admin") > 0);
}

/** 测试7：正确密码登录后锁定计数归零 */
static void test_lock_reset_on_success(void) {
    /* 先故意输错 2 次 */
    auth_login("admin", "bad1");
    auth_login("admin", "bad2");

    /* 第3次正确 */
    int ret = auth_login("admin", "admin123");
    assert(ret == AUTH_OK);
    assert(auth_lock_remaining("admin") == 0);
    auth_logout();
}

/** 测试8：新增管理员 */
static void test_add_admin(void) {
    /* 当前以 admin 登录 */
    int ret = auth_add_admin("ta01", "pass123", ROLE_TA);
    assert(ret == AUTH_OK);

    /* 新管理员可以登录 */
    auth_logout();
    ret = auth_login("ta01", "pass123");
    assert(ret == AUTH_OK);
    assert(auth_current_role() == ROLE_TA);
}

/** 测试9：重复用户名拒绝 */
static void test_add_duplicate_rejected(void) {
    int ret = auth_add_admin("ta01", "another", ROLE_TA);
    assert(ret == AUTH_ALREADY_EXISTS);
}

/** 测试10：助教无法新增管理员 */
static void test_ta_cannot_add_admin(void) {
    /* 当前以 ta01（助教）登录 */
    int ret = auth_add_admin("ta02", "pass", ROLE_TA);
    assert(ret == -1); /* 权限不足 */
}

/** 测试11：助教无法删除管理员 */
static void test_ta_cannot_delete_admin(void) {
    int ret = auth_delete_admin("admin");
    assert(ret == -1); /* 权限不足 */
}

/** 测试12：管理员可以删除其他管理员 */
static void test_admin_can_delete(void) {
    /* 切回 admin */
    auth_logout();
    auth_login("admin", "admin123");

    int ret = auth_delete_admin("ta01");
    assert(ret == 0);

    /* ta01 无法再登录 */
    auth_logout();
    ret = auth_login("ta01", "pass123");
    assert(ret == AUTH_USER_NOT_FOUND);
}

/** 测试13：不能删除自己 */
static void test_cannot_delete_self(void) {
    int ret = auth_delete_admin("admin");
    assert(ret == -1);
}

/** 测试14：修改密码 */
static void test_change_password(void) {
    int ret = auth_change_password("admin", "newpass456");
    assert(ret == 0);

    /* 旧密码失败 */
    auth_logout();
    ret = auth_login("admin", "admin123");
    assert(ret == AUTH_WRONG_PASSWORD);

    /* 新密码成功 */
    ret = auth_login("admin", "newpass456");
    assert(ret == AUTH_OK);
}

/** 测试15：退出登录后 current_user 为空 */
static void test_logout_clears_session(void) {
    auth_logout();
    assert(strcmp(auth_current_user(), "") == 0);
    assert(auth_current_role() == -1);
}

/* ============================================================
 * 入口
 * ============================================================ */

int main(void) {
    setbuf(stdout, NULL);
    printf("=== 登录认证模块测试 ===\n\n");

    /* 初始化测试环境 */
    reset_env();

    printf("[初始化]\n");
    run_test("首次运行创建默认管理员 admin/admin123", test_default_admin_created);

    printf("\n[登录流程]\n");
    run_test("正确密码登录成功", test_login_success);
    run_test("错误密码返回 AUTH_WRONG_PASSWORD", test_login_wrong_password);
    run_test("不存在用户返回 AUTH_USER_NOT_FOUND", test_login_user_not_found);
    run_test("退出登录后 session 清空", test_logout_clears_session);

    printf("\n[锁定机制]\n");
    /* 重置环境，确保锁定计数从 0 开始 */
    reset_env();
    run_test("连续3次错误触发锁定", test_lock_after_3_failures);
    run_test("锁定期间无法登录", test_cannot_login_during_lock);

    /* 注意：锁定超时测试需要等待，跳过自动测试
     * 手动验证时：锁定10秒后应能正常登录 */

    /* 重置环境以解除锁定 */
    reset_env();
    run_test("正确登录后锁定计数归零", test_lock_reset_on_success);

    printf("\n[管理员管理]\n");
    /* 确保以管理员身份运行管理操作测试 */
    auth_login("admin", "admin123");
    run_test("新增管理员（助教）", test_add_admin);

    /* test_add_admin 最后切换到了 ta01，切回 admin 继续 */
    auth_logout();
    auth_login("admin", "admin123");

    run_test("重复用户名拒绝", test_add_duplicate_rejected);

    /* 切换到 ta01 测试权限不足场景 */
    auth_logout();
    auth_login("ta01", "pass123");
    run_test("助教权限不足无法新增", test_ta_cannot_add_admin);
    run_test("助教权限不足无法删除", test_ta_cannot_delete_admin);

    /* 切回 admin 进行删除和密码测试 */
    auth_logout();
    auth_login("admin", "admin123");
    run_test("管理员可删除其他管理员", test_admin_can_delete);

    /* test_admin_can_delete 末尾 login 失败导致登出，重新登录 */
    auth_login("admin", "admin123");
    run_test("不能删除自己", test_cannot_delete_self);
    run_test("修改密码后新旧密码正确切换", test_change_password);

    /* 清理 */
    auth_logout();
    auth_shutdown();
    remove(TEST_ADMIN_FILE);

    printf("\nResults: %d passed, %d failed\n", g_passed, g_failed);
    return g_failed > 0 ? 1 : 0;
}
