/**
 * @file    file_io.c
 * @brief   二进制文件读写实现
 * @details 文件格式：[魔数 4B][版本 2B][记录数 4B][N × 固定大小结构体]
 *          读取时校验魔数和版本号，写入时统一转大端序。
 */

#include "file_io.h"
#include "platform.h"
#include "types.h"

#include <stdio.h>
#include <stdlib.h>

/* ============================================================
 * 写入二进制文件
 * ============================================================ */

int file_write_all(const char *filename, const void *data, size_t elem_size, int count) {
    if (!filename || elem_size == 0 || count < 0) {
        fprintf(stderr, "[错误] file_write_all: 参数无效\n");
        return -1;
    }

    /* count=0 时允许 data 为 NULL（仅写文件头） */
    if (count > 0 && !data) {
        fprintf(stderr, "[错误] file_write_all: count>0 但 data 为 NULL\n");
        return -1;
    }

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "[错误] 无法打开文件写入: %s\n", filename);
        return -1;
    }

    /* 构建文件头 */
    FileHeader hdr;
    hdr.magic = host_to_be32(FILE_MAGIC);
    hdr.version = host_to_be16((uint16_t)SYSTEM_VERSION);
    hdr.count = host_to_be32((uint32_t)count);

    if (fwrite(&hdr, sizeof(hdr), 1, fp) != 1) {
        fprintf(stderr, "[错误] 写入文件头失败: %s\n", filename);
        fclose(fp);
        return -1;
    }

    /* 写入数据体 */
    if (count > 0) {
        size_t written = fwrite(data, elem_size, (size_t)count, fp);
        if (written != (size_t)count) {
            fprintf(stderr, "[错误] 写入数据失败: 预期 %d 条，实际 %zu 条\n", count, written);
            fclose(fp);
            return -1;
        }
    }

    fclose(fp);
    return 0;
}

/* ============================================================
 * 读取二进制文件
 * ============================================================ */

void *file_read_all(const char *filename, size_t elem_size, int *out_count) {
    if (!filename || elem_size == 0 || !out_count)
        return NULL;

    *out_count = 0;

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        /* 文件不存在属于正常情况（首次运行） */
        return NULL;
    }

    /* 读取文件头 */
    FileHeader hdr;
    if (fread(&hdr, sizeof(hdr), 1, fp) != 1) {
        fprintf(stderr, "[错误] 读取文件头失败: %s\n", filename);
        fclose(fp);
        *out_count = -1;
        return NULL;
    }

    /* 校验魔数 */
    if (be32_to_host(hdr.magic) != FILE_MAGIC) {
        fprintf(stderr, "[错误] 文件魔数不匹配: %s（期望 0x%08X，实际 0x%08X）\n", filename,
                FILE_MAGIC, be32_to_host(hdr.magic));
        fclose(fp);
        *out_count = -1;
        return NULL;
    }

    /* 校验版本 */
    uint16_t ver = be16_to_host(hdr.version);
    if (ver != SYSTEM_VERSION) {
        fprintf(stderr, "[错误] 文件版本不支持: %s（文件版本 %u，程序版本 %u）\n", filename,
                (unsigned)ver, (unsigned)SYSTEM_VERSION);
        fclose(fp);
        *out_count = -1;
        return NULL;
    }

    int count = (int)be32_to_host(hdr.count);

    /* 合理性检查 */
    if (count < 0 || count > 1000000) {
        fprintf(stderr, "[错误] 记录条数异常: %d\n", count);
        fclose(fp);
        *out_count = -1;
        return NULL;
    }

    /* 空文件 */
    if (count == 0) {
        fclose(fp);
        return NULL;
    }

    /* 分配内存并读取数据体 */
    size_t total = elem_size * (size_t)count;
    void *data = malloc(total);
    if (!data) {
        fprintf(stderr, "[错误] 内存分配失败（%zu 字节）\n", total);
        fclose(fp);
        *out_count = -1;
        return NULL;
    }

    size_t read_count = fread(data, elem_size, (size_t)count, fp);
    fclose(fp);

    if ((int)read_count != count) {
        fprintf(stderr, "[警告] 文件读取不完整: %s（预期 %d 条，实际 %zu 条）\n", filename, count,
                read_count);
    }

    *out_count = (int)read_count;
    return data;
}
