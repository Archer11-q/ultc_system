/**
 * @file    search.c
 * @brief   检索模块实现
 * @details 耗材检索：编号精准查询 / 名称模糊匹配 → 分页展示结果。
 *          领用记录检索：班级/学号/项目多条件组合筛选 → 分页展示。
 */

#include "search.h"
#include "borrow.h"
#include "material.h"
#include "platform.h"
#include "types.h"
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 耗材检索
 * ============================================================ */

void search_material_menu(void) {
    print_title("耗材检索");

    printf("\n  1. 按编号精准查询\n");
    printf("  2. 按名称模糊搜索\n");
    printf("  0. 返回\n");

    int choice = read_int("\n  请选择: ", 0, 2);
    if (choice == 0)
        return;

    if (choice == 1) {
        /* 编号精准查询 */
        char id[MAX_MAT_ID];
        read_string("  耗材编号: ", id, sizeof(id));

        const Material *mat = material_find_by_id(id);
        if (!mat) {
            printf("\n  [提示] 未找到编号为 '%s' 的耗材。\n", id);
            pause_screen();
            return;
        }

        printf("\n");
        print_line();
        printf("  编号: %s\n", mat->id);
        printf("  名称: %s\n", mat->name);
        printf("  分类: %s\n", material_category_name(mat->category));
        printf("  属性: %s\n", material_attr_name(mat->attr));
        printf("  单价: ￥%.2f\n", mat->unit_price);
        printf("  库存: %d（预警值: %d）\n", mat->total_stock, mat->min_stock);
        printf("  柜号: %s\n", mat->cabinet);
        char date_buf[16];
        strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", localtime(&mat->purchase_date));
        printf("  采购日期: %s\n", date_buf);
        print_line();
    } else {
        /* 名称模糊搜索 */
        char keyword[MAX_MAT_NAME];
        read_string("  关键词: ", keyword, sizeof(keyword));

        int count = 0;
        Material *results = material_search_by_name(keyword, &count);
        if (!results || count == 0) {
            printf("\n  [提示] 未找到名称包含 '%s' 的耗材。\n", keyword);
            pause_screen();
            return;
        }

        /* 分页展示结果 */
        int page = 1, total_pages = 1, running = 1;
        while (running) {
            print_title("模糊搜索结果");
            printf("  关键词: \"%s\"  匹配 %d 条\n", keyword, count);

            total_pages = (count + PAGE_SIZE - 1) / PAGE_SIZE;
            if (page > total_pages)
                page = total_pages;

            printf("\n");
            printf("  %-4s %-12s %-20s %-10s %-6s %6s %s\n", "序号", "编号", "名称", "分类", "属性",
                   "库存", "柜号");
            print_line();

            int start = (page - 1) * PAGE_SIZE;
            int end = (start + PAGE_SIZE < count) ? (start + PAGE_SIZE) : count;
            for (int i = start; i < end; i++) {
                printf("  %-4d %-12s %-20s %-10s %-6s %6d %s\n", i + 1, results[i].id,
                       results[i].name, material_category_name(results[i].category),
                       material_attr_name(results[i].attr), results[i].total_stock,
                       results[i].cabinet);
            }
            print_line();
            printf("  第 %d/%d 页", page, total_pages);

            printf("\n  [N]下一页  [P]上一页  [Q]返回  → ");
            char buf[8];
            read_string("", buf, sizeof(buf));
            switch (buf[0]) {
            case 'n':
            case 'N':
                if (page < total_pages)
                    page++;
                break;
            case 'p':
            case 'P':
                if (page > 1)
                    page--;
                break;
            default:
                running = 0;
                break;
            }
        }
        free(results);
    }
    pause_screen();
}

/* ============================================================
 * 领用记录检索
 * ============================================================ */

void search_record_menu(void) {
    print_title("领用记录检索");

    char class_name[MAX_CLASS_NAME] = "";
    char student_id[MAX_STUDENT_ID] = "";
    char project_id[MAX_PROJECT_ID] = "";

    printf("\n  （直接回车表示不限条件）\n");
    read_string("  班级: ", class_name, sizeof(class_name));
    read_string("  学号: ", student_id, sizeof(student_id));
    read_string("  实训项目编号: ", project_id, sizeof(project_id));

    int count = 0;
    BorrowRecord *results = borrow_search(class_name, student_id, project_id, &count);
    if (!results || count == 0) {
        printf("\n  [提示] 未找到匹配的领用记录。\n");
        pause_screen();
        return;
    }

    /* 分页展示 */
    int page = 1, total_pages = 1, running = 1;
    while (running) {
        print_title("领用记录检索结果");
        printf("  筛选条件: ");
        if (class_name[0])
            printf("班级=%s ", class_name);
        if (student_id[0])
            printf("学号=%s ", student_id);
        if (project_id[0])
            printf("项目=%s ", project_id);
        if (!class_name[0] && !student_id[0] && !project_id[0])
            printf("（全部）");
        printf("  匹配 %d 条\n", count);

        total_pages = (count + PAGE_SIZE - 1) / PAGE_SIZE;
        if (page > total_pages)
            page = total_pages;

        printf("\n");
        printf("  %-4s %-22s %-10s %-10s %-12s %-6s %-8s %s\n", "序号", "领用单号", "学号", "姓名",
               "耗材编号", "数量", "状态", "领用时间");
        print_line();

        int start = (page - 1) * PAGE_SIZE;
        int end = (start + PAGE_SIZE < count) ? (start + PAGE_SIZE) : count;
        for (int i = start; i < end; i++) {
            const char *status_str;
            switch (results[i].status) {
            case BORROW_ACTIVE:
                status_str = "领用中";
                break;
            case BORROW_RETURNED:
                status_str = "已归还";
                break;
            case BORROW_OVERDUE:
                status_str = "逾期";
                break;
            case BORROW_SCRAPPED:
                status_str = "已报废";
                break;
            default:
                status_str = "未知";
                break;
            }

            char time_buf[20];
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M",
                     localtime(&results[i].borrow_time));

            printf("  %-4d %-22s %-10s %-10s %-12s %-6d %-8s %s\n", i + 1, results[i].record_id,
                   results[i].student_id, results[i].student_name, results[i].material_id,
                   results[i].quantity, status_str, time_buf);
        }
        print_line();
        printf("  第 %d/%d 页", page, total_pages);

        printf("\n  [N]下一页  [P]上一页  [Q]返回  → ");
        char buf[8];
        read_string("", buf, sizeof(buf));
        switch (buf[0]) {
        case 'n':
        case 'N':
            if (page < total_pages)
                page++;
            break;
        case 'p':
        case 'P':
            if (page > 1)
                page--;
            break;
        default:
            running = 0;
            break;
        }
    }
    free(results);
    pause_screen();
}
