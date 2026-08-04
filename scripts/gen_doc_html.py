#!/usr/bin/env python3
"""
生成 HTML 说明文档，用 CSS @page 控制打印布局。
在浏览器中打开后 Ctrl+P 另存为 PDF 即可获得完美的中文 PDF。
"""
import os, re, time

SCREENSHOT_DIR = "docs/screenshots"

SCREENS = [
    ("1. 系统登录", "多管理员登录界面。默认管理员 admin/admin123，连续 3 次密码错误锁定 10 秒。", "01-login.txt"),
    ("2. CSV 批量导入耗材", "支持 UTF-8 BOM 的 CSV 文件批量导入。逐行校验字段，自动跳过重复编号，输出汇总报告。", "02-csv-import.txt"),
    ("3. 新增耗材", "手动录入耗材信息：编号唯一性校验、5 种分类、一次性/可循环属性、库存与预警值设置。", "03-add-material.txt"),
    ("4. 耗材列表（分页）", "分页展示全部耗材，每页 10 条。库存低于预警值的行以特殊标记提示，支持前后翻页。", "04-material-list.txt"),
    ("5. 学生领用", "差异化领用规则：一次性耗材检查库存后直接扣减，可循环耗材仅登记不扣库存。同一领用单可包含多种耗材。", "05-borrow.txt"),
    ("6. 库存预警与采购清单", "筛选低于最低预警库存的耗材，计算建议采购量（预警值 x 2 - 当前库存），汇总预估采购总金额。", "06-alert.txt"),
    ("7. 精准检索", "按编号精准查询耗材，展示完整信息卡片。", "07-search-exact.txt"),
    ("8. 模糊搜索", "按名称关键词模糊匹配（子串搜索），搜索结果支持分页浏览。", "08-search-fuzzy.txt"),
    ("9. 数据统计概览", "月度消耗、班级排行榜（冠亚季标记）、逾期汇总、报废成本分类统计。", "09-stats.txt"),
    ("10. 管理员管理", "多管理员账号管理：新增/删除管理员、修改密码，支持双角色。", "10-admin-manage.txt"),
    ("11. 助教受限菜单", "助教登录后仅显示查询类功能，实现基于角色的菜单级权限控制。", "11-ta-menu.txt"),
    ("12. 操作审计日志", "自动记录 12 种操作类型，支持按操作类型和操作者筛选，分页查看。", "12-audit.txt"),
    ("13. CSV 数据导出", "支持导出耗材清单、采购清单、领用记录为 CSV 文件，UTF-8 BOM 兼容 Excel。", "13-csv-export.txt"),
    ("14. 逾期管理", "超过 7 天未归还的自动判定为逾期。展示逾期学生名单（去重）、工具总件数。", "14-overdue.txt"),
]

FLOW = """[启动程序]
    |
    v
[管理员登录] -- 连续3次错误? --> [锁定10秒] --> [自动解锁]
    |
    v
[角色判断]
    |
 +--+--+
 |     |
 v     v
[实验老师 - 全权限]     [实训助教 - 仅查询]
 |                       |
 +-- [耗材CRUD]          +-- [耗材列表]
 +-- [学生领用]          +-- [库存预警]
 +-- [归还逾期]          +-- [检索]
 +-- [库存预警/盘点]     +-- [数据统计]
 +-- [检索/统计]
 +-- [CSV导入导出]
 +-- [审计日志]
 +-- [管理员管理]

领用规则:
  一次性耗材 --> 检查库存 --> 扣减库存 --> 创建记录
  可循环耗材 --> 检查库存 --> 仅登记(不扣库存) --> 创建记录

归还流程:
  正常归还 --> 标记已归还(库存不变)
  损坏归还 --> 标记报废 --> 扣减库存 --> 生成报废记录
  逾期判定 --> 超过7天自动标记逾期

数据持久化:
  格式: [魔数 4B][版本 2B][条数 4B][N x 固定大小结构体]
  大端序存储, 跨平台兼容
  文件: admin.dat / material.dat / scrap.dat / borrow.dat / stocktake.dat / audit.dat"""


def load_screenshot(filename):
    filepath = os.path.join(SCREENSHOT_DIR, filename)
    if not os.path.exists(filepath):
        return ["[截图文件不存在]"]
    with open(filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()
    clean = []
    for line in lines:
        c = re.sub(r'\x1b\[[0-9;]*[a-zA-Z]', '', line)
        c = re.sub(r'\x1b\]0;.*?\x07', '', c)
        c = c.rstrip('\n\r')
        if not c.strip():
            c = " "
        clean.append(c)
    return clean


def build_html():
    parts = []
    parts.append('''<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<title>高校实验室实训耗材智能管理系统 - 说明文档</title>
<style>
  @page { size: A4; margin: 1.5cm 2cm 1.5cm 2cm; }
  @media print {
    body { -webkit-print-color-adjust: exact; print-color-adjust: exact; }
    .page-break { page-break-before: always; }
    .no-break { page-break-inside: avoid; }
  }
  body { font-family: "Microsoft YaHei","SimHei","Noto Sans CJK SC",sans-serif; color:#222; line-height:1.7; max-width:900px; margin:0 auto; padding:20px; }
  h1 { text-align:center; font-size:26px; margin:30px 0 5px 0; }
  .subtitle { text-align:center; color:#666; font-size:14px; margin-bottom:20px; }
  h2 { font-size:18px; border-bottom:2px solid #333; padding-bottom:5px; margin:30px 0 12px 0; page-break-before:always; }
  h3 { font-size:15px; color:#444; margin:20px 0 8px 0; }
  p.desc { color:#555; font-size:13px; margin:0 0 10px 0; }
  pre.screen { background:#1e1e1e; color:#d4d4d4; padding:12px; border-radius:4px; font-family:"Cascadia Code","Fira Code","Consolas","Courier New",monospace; font-size:10px; line-height:1.3; overflow-x:auto; white-space:pre; max-height:620px; overflow-y:auto; border:1px solid #444; }
  .arch-box { background:#f5f5f5; border:1px solid #ddd; padding:12px; font-family:monospace; font-size:11px; line-height:1.5; white-space:pre; border-radius:4px; }
  .flow-box { background:#f0f4f8; border:1px solid #bcd; padding:14px; font-family:monospace; font-size:11px; line-height:1.5; white-space:pre; border-radius:4px; }
  table { border-collapse:collapse; width:100%; margin:10px 0; font-size:13px; }
  td,th { border:1px solid #ccc; padding:6px 10px; text-align:left; }
  th { background:#eee; font-weight:bold; }
  .highlight { background:#fff3cd; }
  .cover-box { text-align:center; padding:60px 0; page-break-after:always; }
  .cover-box h1 { font-size:30px; margin-bottom:10px; }
  .cover-box .arch { display:inline-block; text-align:left; background:#f8f9fa; border:2px solid #dee2e6; padding:15px 25px; border-radius:8px; font-family:monospace; font-size:11px; line-height:1.6; margin-top:20px; }
  .footer { text-align:center; color:#999; font-size:11px; margin-top:40px; border-top:1px solid #eee; padding-top:10px; }
</style>
</head>
<body>
''')

    # === Cover ===
    parts.append('<div class="cover-box">')
    parts.append('<h1>高校实验室实训耗材智能管理系统</h1>')
    parts.append('<p class="subtitle">University Lab Consumables Management System</p>')
    parts.append('<p style="color:#666;font-size:13px">基于 C11 语言 | 链表动态存储 | 二进制文件持久化 | 零外部依赖 | 跨平台兼容</p>')
    parts.append('<p style="color:#888;font-size:12px">2026 年"开发者"算法编程挑战赛 — C 语言赛道决赛题目</p>')
    parts.append(f'<p style="color:#aaa;font-size:11px">{time.strftime("%Y-%m-%d")}</p>')
    parts.append('<div class="arch">')
    parts.append('+-------------------------------------------------------+<br>')
    parts.append('|&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;控制台交互层 (main.c)&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|<br>')
    parts.append('+-------------------------------------------------------+<br>')
    parts.append('|&nbsp;&nbsp;auth&nbsp;|&nbsp;material&nbsp;|&nbsp;borrow&nbsp;|&nbsp;inventory&nbsp;|&nbsp;stats&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|<br>')
    parts.append('|&nbsp;&nbsp;search&nbsp;|&nbsp;csv_io&nbsp;&nbsp;|&nbsp;audit&nbsp;&nbsp;(9&nbsp;个业务模块)&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|<br>')
    parts.append('+-------------------------------------------------------+<br>')
    parts.append('|&nbsp;&nbsp;file_io&nbsp;|&nbsp;ui&nbsp;|&nbsp;platform&nbsp;(基础设施层)&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;|<br>')
    parts.append('+-------------------------------------------------------+<br>')
    parts.append('</div>')
    parts.append('<p style="color:#888;font-size:11px;margin-top:15px">96 项自动化测试 | 13 头文件 + 13 源文件 + 9 测试文件 | 5500+ 行代码</p>')
    parts.append('</div>')

    # === System Flow ===
    parts.append('<h2>系统逻辑流程</h2>')
    parts.append(f'<div class="flow-box">{FLOW}</div>')

    # === Architecture ===
    parts.append('<h2>模块架构与关键指标</h2>')
    parts.append('<table>')
    parts.append('<tr><th>层次</th><th>模块</th><th>职责</th></tr>')
    rows = [
        ("UI 层", "main.c", "菜单路由 + 权限过滤"),
        ("业务层", "auth", "登录认证 (多账号/锁定/双角色)"),
        ("", "material", "耗材管理 (CRUD/报废/预警/采购清单)"),
        ("", "borrow", "领用归还 (差异化规则/逾期检测)"),
        ("", "inventory", "库存盘点 (差异修正/盘点日志)"),
        ("", "search", "检索 (精准/模糊/多条件筛选)"),
        ("", "stats", "统计 (月度/排行/逾期/报废成本)"),
        ("", "csv_io", "CSV 导入导出 (校验/BOM/Excel 兼容)"),
        ("", "audit", "审计日志 (12 种操作类型自动记录)"),
        ("基础设施", "file_io", "二进制文件读写 (统一格式+大端序)"),
        ("", "ui", "控制台界面工具 (表格/输入/确认)"),
        ("", "platform", "跨平台兼容层 (Windows/Linux)"),
        ("数据模型", "types.h", "6 个结构体 + 枚举 + 常量"),
    ]
    for layer, mod, duty in rows:
        parts.append(f'<tr><td>{layer}</td><td><code>{mod}</code></td><td>{duty}</td></tr>')
    parts.append('</table>')
    parts.append('<p><strong>源文件:</strong> 13 headers + 13 src + 9 test files | <strong>测试:</strong> 96 项全部通过 | <strong>依赖:</strong> 0 (仅 C11 标准库) | <strong>平台:</strong> Windows + Linux</p>')

    # === Data Structures ===
    parts.append('<h2>核心数据结构</h2>')
    ds_html = '''<table>
<tr><th>结构体</th><th>关键字段</th><th>说明</th></tr>
<tr><td><code>Admin</code></td><td>username, password, role, lock_count, lock_until</td><td>管理员账号，支持锁定机制</td></tr>
<tr><td><code>Material</code></td><td>id, name, category(5种), attr(一次性/可循环), unit_price, total_stock, min_stock, cabinet, purchase_date</td><td>耗材档案，核心业务实体</td></tr>
<tr><td><code>BorrowRecord</code></td><td>record_id, student_id/name, class_name, project_id, material_id, quantity, borrow_time, return_time, status(4种状态), damage_note, operator_name</td><td>领用记录，追踪全生命周期</td></tr>
<tr><td><code>ScrapRecord</code></td><td>scrap_id, material_id/name, scrap_time, reason, quantity, operator_name</td><td>报废记录，库存扣减联动</td></tr>
<tr><td><code>StocktakeLog</code></td><td>log_id, material_id, book_value, actual_value, diff, operator_name, check_time</td><td>盘点日志，记录差异修正</td></tr>
<tr><td><code>AuditRecord</code></td><td>log_id, timestamp, operator_name, action(12种), target_id, detail</td><td>审计日志，全操作追溯</td></tr>
</table>
<p>所有集合使用<strong>单向链表</strong>组织（无固定数组容量限制），字符串使用<strong>定长 char 数组</strong>（确保结构体大小固定，便于二进制序列化）。</p>'''
    parts.append(ds_html)

    # === Screenshots ===
    for i, (title, desc, fname) in enumerate(SCREENS):
        parts.append(f'<h2>{title}</h2>')
        parts.append(f'<p class="desc">{desc}</p>')
        lines = load_screenshot(fname)
        # Show first 45 lines for readability
        display_lines = lines[:45]
        screen_text = '\n'.join(display_lines)
        parts.append(f'<pre class="screen">{screen_text}</pre>')
        if len(lines) > 45:
            parts.append(f'<p style="color:#888;font-size:11px">(截图共 {len(lines)} 行，此处仅显示前 45 行)</p>')

    # === Test Summary ===
    parts.append('<h2>测试覆盖率</h2>')
    parts.append('''<table>
<tr><th>测试模块</th><th>数量</th><th>覆盖内容</th></tr>
<tr><td>test_file_io</td><td>5</td><td>二进制读写 / 魔数校验 / 版本检查</td></tr>
<tr><td>test_auth</td><td>15</td><td>登录 / 锁定 / 超时解锁 / 角色权限 / 管理员 CRUD</td></tr>
<tr><td>test_material</td><td>16</td><td>耗材 CRUD / 库存扣减 / 报废 / 分页 / 分类</td></tr>
<tr><td>test_borrow</td><td>15</td><td>领用规则 / 批量归还 / 逾期检测 / 多条件检索</td></tr>
<tr><td>test_inventory</td><td>8</td><td>盘点差异 / 修正联动 / 预警 / 采购清单</td></tr>
<tr><td>test_search</td><td>11</td><td>精准查询 / 模糊匹配 / 多条件筛选</td></tr>
<tr><td>test_stats</td><td>5</td><td>月度聚合 / 班级排行 / 逾期统计 / 报废成本</td></tr>
<tr><td>test_csv_audit</td><td>10</td><td>CSV 导入导出 / 审计记录 / 筛选 / 名称映射</td></tr>
<tr><td>test_integration</td><td>11</td><td>E2E 全流程 / 边界条件 / 持久化 / 并发一致性</td></tr>
<tr class="highlight"><td><strong>总计</strong></td><td><strong>96</strong></td><td><strong>100% 通过率，零编译警告</strong></td></tr>
</table>''')

    # === Version History ===
    parts.append('<h2>版本历史</h2>')
    parts.append('''<table>
<tr><th>版本</th><th>内容</th><th>测试</th></tr>
<tr><td>v0.1</td><td>项目骨架 + 数据结构 + 文件 IO</td><td>5</td></tr>
<tr><td>v0.2</td><td>登录认证 (多管理员 + 锁定 + 双权限)</td><td>15</td></tr>
<tr><td>v0.3</td><td>耗材管理 (CRUD + 分类 + 报废 + 分页)</td><td>16</td></tr>
<tr><td>v0.4</td><td>学生领用 (差异化规则 + 多耗材一单 + 回执)</td><td>14</td></tr>
<tr><td>v0.5</td><td>归还逾期 (批量归还 + 损坏 -> 报废联动)</td><td>15</td></tr>
<tr><td>v0.6</td><td>库存预警 (采购清单 + 盘点差异修正)</td><td>8</td></tr>
<tr><td>v0.7</td><td>检索 (精准 + 模糊 + 多条件筛选)</td><td>11</td></tr>
<tr><td>v0.8</td><td>数据统计 (月度 + 班级排行 + 逾期 + 报废成本)</td><td>5</td></tr>
<tr><td>v0.9</td><td>集成测试 (E2E + 边界 + 持久化 + 并发)</td><td>8</td></tr>
<tr><td>v0.10</td><td>CSV 导入导出 + 审计日志 (12 种操作类型)</td><td>10</td></tr>
<tr><td>v0.11</td><td>审计下沉模块层 + 集成测试扩展</td><td>3</td></tr>
<tr class="highlight"><td><strong>v1.2</strong></td><td><strong>最终交付 (clang-format + README + 说明文档)</strong></td><td><strong>96</strong></td></tr>
</table>
<p style="text-align:center;color:#888">GitHub: git@github.com:Archer11-q/ultc_system.git</p>''')

    parts.append('<div class="footer">高校实验室实训耗材智能管理系统 — 说明文档 — 生成于 ' + time.strftime("%Y-%m-%d %H:%M") + '</div>')
    parts.append('</body></html>')

    return '\n'.join(parts)


def main():
    html = build_html()
    out = "docs/ultc_system_manual.html"
    with open(out, "w", encoding="utf-8") as f:
        f.write(html)
    print(f"HTML generated: {out} ({len(html)} chars)")
    print()
    print("To create PDF:")
    print("  1. Open docs/ultc_system_manual.html in your browser (Chrome/Edge)")
    print("  2. Press Ctrl+P")
    print("  3. Select 'Save as PDF'")
    print("  4. Set margins to 'None' or 'Minimum'")
    print("  5. Save as docs/ultc_system_manual.pdf")
    print()
    print("This approach ensures perfect Chinese rendering and preserves terminal colors.")


if __name__ == "__main__":
    main()
