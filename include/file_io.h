/**
 * @file    file_io.h
 * @brief   二进制文件读写 — 通用持久化层
 * @details 所有持久化数据采用统一的文件格式：
 *          [魔数 4B][版本 2B][记录数 4B][N × 固定大小结构体]
 *          多字节整数使用大端序存储，确保跨平台可读。
 */

#ifndef FILE_IO_H
#define FILE_IO_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 文件头结构体（二进制文件前 10 字节）
 * ============================================================ */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;     /**< 魔数 0x554C5443 ("ULTC")         */
    uint16_t version;   /**< 文件格式版本号                    */
    uint32_t count;     /**< 记录条数                          */
} FileHeader;
#pragma pack(pop)

/** 文件头固定大小 */
#define FILE_HEADER_SIZE 10

/* ============================================================
 * 写入二进制文件
 *
 * @param filename  文件路径（如 "data/material.dat"）
 * @param data      结构体数组指针
 * @param elem_size 单个结构体字节数（sizeof）
 * @param count     结构体条数
 * @return 0 成功，-1 失败
 * ============================================================ */

int file_write_all(const char* filename, const void* data,
                   size_t elem_size, int count);

/* ============================================================
 * 读取二进制文件
 *
 * @param filename  文件路径
 * @param elem_size 单个结构体字节数（sizeof）
 * @param out_count 输出参数，实际读取条数
 * @return 成功返回已分配内存指针（调用方负责 free），
 *         文件不存在或为空返回 NULL（out_count=0），
 *         文件损坏返回 NULL（out_count=-1）
 * ============================================================ */

void* file_read_all(const char* filename, size_t elem_size, int* out_count);

#ifdef __cplusplus
}
#endif

#endif /* FILE_IO_H */
