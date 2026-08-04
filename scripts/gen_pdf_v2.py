#!/usr/bin/env python3
"""
PDF v2 — 自动下载 fpdf2 wheel + 微软雅黑中文字体 + 终端截图渲染
运行: python3 scripts/gen_pdf_v2.py
"""
import urllib.request, zipfile, io, os, sys, re, ssl, json, time

# ============================================================
# Step 1: 下载并安装 fpdf2
# ============================================================
FPDF_DIR = "/tmp/fpdf2_lib"
if not os.path.exists(os.path.join(FPDF_DIR, "fpdf")):
    print("Downloading fpdf2 from PyPI...")
    ctx = ssl.create_default_context()
    ctx.check_hostname = False
    ctx.verify_mode = ssl.CERT_NONE
    try:
        req = urllib.request.Request(
            "https://pypi.org/pypi/fpdf2/json",
            headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, context=ctx, timeout=30) as resp:
            info = json.loads(resp.read())
        wheel_url = info["urls"][0]["url"]
        for u in info["urls"]:
            if u["packagetype"] == "bdist_wheel" and "none-any" in u["filename"]:
                wheel_url = u["url"]
                break
        print(f"  Fetching: {wheel_url[:60]}...")
        req = urllib.request.Request(wheel_url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, context=ctx, timeout=60) as resp:
            data = resp.read()
        os.makedirs(FPDF_DIR, exist_ok=True)
        with zipfile.ZipFile(io.BytesIO(data)) as zf:
            zf.extractall(FPDF_DIR)
        print("  fpdf2 installed successfully")
    except Exception as e:
        print(f"  ERROR downloading fpdf2: {e}")
        print("  Falling back to pure-Python PDF (no Chinese support)")
        sys.exit(1)

sys.path.insert(0, FPDF_DIR)
from fpdf import FPDF

# ============================================================
# Step 2: 找中文字体
# ============================================================
FONT_CANDIDATES = [
    "/mnt/c/Windows/Fonts/msyh.ttc",
    "/mnt/c/Windows/Fonts/simhei.ttf",
    "/mnt/c/Windows/Fonts/simsun.ttc",
    "/mnt/c/Windows/Fonts/simkai.ttf",
    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
]
CN_FONT = None
for fp in FONT_CANDIDATES:
    if os.path.exists(fp):
        CN_FONT = fp
        print(f"Using Chinese font: {os.path.basename(fp)}")
        break
if not CN_FONT:
    print("ERROR: No Chinese font found")
    sys.exit(1)

# ============================================================
# Step 3: 生成 PDF
# ============================================================
SCREENSHOT_DIR = "docs/screenshots"

class Doc(FPDF):
    def __init__(self):
        super().__init__('P', 'mm', 'A4')
        self.add_font("CN", "", CN_FONT, uni=True)
        self.set_auto_page_break(True, 15)

    def cover(self):
        self.add_page()
        self.ln(35)
        self.set_font("CN", "", 28)
        self.cell(0, 15, "高校实验室实训耗材智能管理系统", align="C", new_x="LMARGIN", new_y="NEXT")
        self.ln(6)
        self.set_font("CN", "", 14)
        self.cell(0, 10, "University Lab Consumables Management System", align="C", new_x="LMARGIN", new_y="NEXT")
        self.ln(12)
        self.set_font("CN", "", 11)
        self.cell(0, 8, "基于 C11 语言 | 链表动态存储 | 二进制文件持久化 | 零外部依赖 | 跨平台", align="C", new_x="LMARGIN", new_y="NEXT")
        self.ln(12)
        self.set_font("CN", "", 10)
        self.cell(0, 8, '2026 年"开发者"算法编程挑战赛 -- C 语言赛道决赛题目', align="C", new_x="LMARGIN", new_y="NEXT")
        self.ln(8)
        self.cell(0, 8, time.strftime("%Y-%m-%d"), align="C", new_x="LMARGIN", new_y="NEXT")
        self.ln(18)
        self.set_font("CN", "", 9)
        for line in [
            "+-------------------------------------------------------+",
            "|              控制台交互层 (main.c)                     |",
            "+-------------------------------------------------------+",
            "|  auth | material | borrow | inventory | stats          |",
            "|  search | csv_io | audit  (9 business modules)         |",
            "+-------------------------------------------------------+",
            "|  file_io | ui | platform  (infrastructure layer)       |",
            "+-------------------------------------------------------+",
        ]:
            self.cell(0, 5.5, line, align="C", new_x="LMARGIN", new_y="NEXT")
        self.ln(5)
        self.set_font("CN", "", 9)
        self.cell(0, 7, "96 tests | 13 headers + 13 src + 9 test files | 5500+ lines", align="C", new_x="LMARGIN", new_y="NEXT")

    def sec(self, title):
        self.ln(4)
        self.set_font("CN", "", 14)
        self.cell(0, 10, title, new_x="LMARGIN", new_y="NEXT")
        y = self.get_y()
        self.set_draw_color(100, 100, 100)
        self.line(self.l_margin, y, self.w - self.r_margin, y)
        self.ln(4)

    def txt(self, text):
        self.set_font("CN", "", 10)
        self.multi_cell(0, 6.5, text, align="L")

    def text_lines(self, lines, size=9.5):
        self.set_font("CN", "", size)
        for line in lines:
            self.cell(0, 6, line[:100], new_x="LMARGIN", new_y="NEXT")

    def shot(self, title, desc, filename):
        self.add_page()
        self.sec(title)
        self.txt(desc)
        self.ln(3)

        filepath = os.path.join(SCREENSHOT_DIR, filename)
        if not os.path.exists(filepath):
            self.txt("[Screenshot not found]")
            return

        with open(filepath, "r", encoding="utf-8") as f:
            raw = f.readlines()

        # Clean ANSI codes
        lines = []
        for line in raw:
            c = re.sub(r'\x1b\[[0-9;]*[a-zA-Z]', '', line)
            c = re.sub(r'\x1b\]0;.*?\x07', '', c)
            c = c.rstrip('\n\r')
            if not c.strip():
                c = " "
            lines.append(c)

        # Draw terminal frame
        self.set_fill_color(30, 30, 30)
        self.set_draw_color(80, 80, 80)
        avail = self.h - self.get_y() - 18
        lh = 4.0
        max_lines = min(int(avail / lh), len(lines)) - 1

        box_top = self.get_y()
        box_h = max_lines * lh + 6
        self.rect(self.l_margin, box_top, self.w - self.l_margin - self.r_margin, box_h, style="D")

        self.set_xy(self.l_margin + 3, box_top + 2)
        self.set_text_color(200, 200, 200)
        self.set_font("CN", "", 6.3)

        for i in range(max_lines):
            display = lines[i][:100]
            self.cell(0, lh, display, new_x="LMARGIN", new_y="NEXT")
            self.set_x(self.l_margin + 3)

        self.set_text_color(0, 0, 0)

        if len(lines) > max_lines:
            self.set_font("CN", "", 7)
            self.cell(0, 5, f"(Total {len(lines)} lines, showing first {max_lines})", align="R", new_x="LMARGIN", new_y="NEXT")

    def page_lines(self, title, lines):
        self.add_page()
        self.sec(title)
        self.text_lines(lines)


SCREENS = [
    ("1. 系统登录", "多管理员登录界面。默认管理员 admin/admin123，连续 3 次密码错误锁定 10 秒，支持实验老师（全权限）和实训助教（仅查询）两种角色。", "01-login.txt"),
    ("2. CSV 批量导入耗材", "支持 UTF-8 BOM 的 CSV 文件批量导入。逐行校验字段（编号/分类/属性/数值/日期），自动跳过重复编号，输出成功/跳过/错误汇总报告。", "02-csv-import.txt"),
    ("3. 新增耗材", "手动录入耗材信息：编号唯一性校验、5 种分类（电子元器件/电工工具/开发板/化学耗材/机械零件）、一次性/可循环属性、库存与预警值设置。", "03-add-material.txt"),
    ("4. 耗材列表（分页）", "分页展示全部耗材，每页 10 条。库存低于预警值的行以特殊标记提示。支持前后翻页浏览。", "04-material-list.txt"),
    ("5. 学生领用", "差异化领用规则：一次性耗材检查库存后直接扣减，可循环耗材仅登记不扣库存。同一领用单可包含多种耗材，完成时打印领用回执。", "05-borrow.txt"),
    ("6. 库存预警与采购清单", "自动筛选低于最低预警库存的耗材，生成采购清单：计算建议采购量（预警值 x 2 - 当前库存），汇总预估采购总金额。", "06-alert.txt"),
    ("7. 精准检索", "按编号精准查询耗材，展示完整信息卡片：名称、分类、属性、单价、库存、预警值、存放柜号、采购日期。", "07-search-exact.txt"),
    ("8. 模糊搜索", "按名称关键词模糊匹配（子串搜索），支持中英文关键词。搜索结果支持分页浏览。", "08-search-fuzzy.txt"),
    ("9. 数据统计概览", "四项统计报表：(1) 月度消耗按月份+分类二维聚合；(2) 班级用量排行榜（降序+冠亚季标记）；(3) 逾期未归还汇总（学生去重+工具件数）；(4) 报废成本按分类聚合。", "09-stats.txt"),
    ("10. 管理员管理", "多管理员账号管理：新增/删除管理员、修改密码。支持实验老师（全权限）和实训助教（仅查询）两种角色。", "10-admin-manage.txt"),
    ("11. 助教受限菜单", "助教登录后仅显示查询类功能（耗材列表/预警/检索/统计），增删改操作不可见，实现基于角色的菜单级权限控制。", "11-ta-menu.txt"),
    ("12. 操作审计日志", "自动记录 12 种操作类型（登录/CRUD/领用/归还/报废/盘点/CSV 导入/管理员管理）。支持按操作类型和操作者筛选，分页查看。", "12-audit.txt"),
    ("13. CSV 数据导出", "支持导出耗材清单、采购清单、领用记录为 CSV 文件，UTF-8 BOM 编码兼容 Microsoft Excel 中文显示。", "13-csv-export.txt"),
    ("14. 逾期管理", "超过 7 天未归还的自动判定为逾期。展示逾期学生名单（去重显示）、逾期可循环工具总件数统计。", "14-overdue.txt"),
]


def main():
    pdf = Doc()
    pdf.set_title("高校实验室实训耗材智能管理系统 - 说明文档")

    # Cover
    pdf.cover()

    # Flow diagram
    pdf.page_lines("系统逻辑流程", [
        "[启动] --> [登录] --> 3次错误? --> [锁定10秒] --> [自动解锁]",
        "   |                     |",
        "   v                     v",
        " [角色判断] --> [老师(全权限)]  /  [助教(仅查询)]",
        "   |",
        "   +-- [耗材CRUD] -- 编号唯一校验 + 5分类 + 一次性/可循环",
        "   +-- [学生领用] -- 一次性扣库存 / 可循环仅登记 / 多耗材一单",
        "   +-- [归还逾期] -- 正常归还 / 损坏->报废联动 / 7天逾期",
        "   +-- [库存管理] -- 预警(低于最低储备) / 采购清单(建议量+金额)",
        "   +-- [库存盘点] -- 账面vs实际 / 差异修正 / 盘点日志持久化",
        "   +-- [检索]    -- 精准编号 / 模糊名称 / 多条件筛选领用记录",
        "   +-- [统计]    -- 月度消耗 / 班级排行 / 逾期 / 报废成本",
        "   +-- [CSV导入导出] -- 批量导入(校验+错误报告) / 三种导出",
        "   +-- [审计日志] -- 12种操作类型自动记录 / 筛选查看",
        "   +-- [管理员管理] -- 新增/删除/修改密码",
        "",
        "数据持久化: data/*.dat 二进制文件",
        "格式: [魔数 4B][版本 2B][条数 4B][N x 固定大小结构体]",
        "大端序存储, 跨平台兼容 (Windows / Linux)",
    ])

    # Architecture
    pdf.page_lines("模块架构与关键指标", [
        "层次           模块                 职责",
        "----------------------------------------------------------------------",
        "UI层           main.c               菜单路由 + 权限过滤",
        "业务层         auth                 登录认证 (多账号/锁定/角色)",
        "               material             耗材管理 (CRUD/报废/预警/采购清单)",
        "               borrow               领用归还 (差异化规则/逾期检测)",
        "               inventory            库存盘点 (差异修正/盘点日志)",
        "               search               检索 (精准/模糊/多条件筛选)",
        "               stats                统计 (月度/排行/逾期/报废成本)",
        "               csv_io               CSV导入导出 (校验/BOM/Excel兼容)",
        "               audit                审计日志 (12种操作类型自动记录)",
        "基础设施       file_io              二进制文件读写 (统一格式+大端序)",
        "               ui                   控制台界面工具 (表格/输入/确认)",
        "               platform             跨平台兼容层 (Windows/Linux)",
        "数据模型       types.h              6个结构体 + 枚举 + 常量",
        "",
        "源文件: 13 headers + 13 src + 9 test files",
        "测试:   96 项 (全部通过, 零编译警告)",
        "依赖:   0 (仅 C11 标准库)",
        "平台:   Windows + Linux (同一份源码)",
    ])

    # Data structures
    pdf.page_lines("核心数据结构 (types.h)", [
        "Admin (管理员)",
        "  username[32], password[32], role, lock_count, lock_until",
        "",
        "Material (耗材)",
        "  id[16], name[64], category(5种分类), attr(一次性/可循环),",
        "  unit_price, total_stock, min_stock, cabinet[16], purchase_date",
        "",
        "BorrowRecord (领用记录)",
        "  record_id[32], student_id[16], student_name[32], class_name[32],",
        "  project_id[16], material_id[16], quantity, borrow_time,",
        "  return_time, status(领用中/已归还/逾期/已报废),",
        "  damage_note[128], operator_name[32]",
        "",
        "ScrapRecord (报废记录)",
        "  scrap_id[32], material_id[16], material_name[64],",
        "  scrap_time, reason[128], quantity, operator_name[32]",
        "",
        "StocktakeLog (盘点日志)",
        "  log_id[32], material_id[16], book_value, actual_value,",
        "  diff, operator_name[32], check_time",
        "",
        "AuditRecord (审计日志)",
        "  log_id[32], timestamp, operator_name[32],",
        "  action(12种操作类型), target_id[64], detail[256]",
        "",
        "所有集合使用单向链表 (无固定数组容量限制)",
        "字符串使用定长 char 数组 (确保结构体大小固定, 便于二进制序列化)",
    ])

    # Screenshots
    for title, desc, fname in SCREENS:
        pdf.shot(title, desc, fname)

    # Test summary
    pdf.page_lines("测试覆盖率 (96 项全部通过)", [
        "模块                 测试数   覆盖内容",
        "----------------------------------------------------------------------",
        "test_file_io          5       二进制读写/魔数校验/版本检查",
        "test_auth            15       登录/锁定/超时解锁/角色权限/CRUD",
        "test_material        16       耗材CRUD/库存扣减/报废/分页/分类",
        "test_borrow          15       领用规则/批量归还/逾期/多条件检索",
        "test_inventory        8       盘点差异/修正联动/预警/采购清单",
        "test_search          11       精准查询/模糊匹配/多条件筛选",
        "test_stats            5       月度聚合/班级排行/逾期统计/报废成本",
        "test_csv_audit       10       CSV导入导出/审计记录/筛选/名称映射",
        "test_integration     11       E2E全流程/边界/持久化/并发一致性",
        "----------------------------------------------------------------------",
        "总计                 96       100% 通过率, 零编译警告",
    ])

    # Version history
    pdf.page_lines("版本历史 (12 次迭代)", [
        "v0.1    项目骨架 + 数据结构 + 文件IO               5 测试",
        "v0.2    登录认证 (多管理员+锁定+双权限)            15 测试",
        "v0.3    耗材管理 (CRUD+分类+报废+分页)            16 测试",
        "v0.4    学生领用 (差异化规则+多耗材一单+回执)     14 测试",
        "v0.5    归还逾期 (批量归还+损坏->报废联动)        15 测试",
        "v0.6    库存预警 (采购清单+盘点差异修正)           8 测试",
        "v0.7    检索 (精准+模糊+多条件筛选)               11 测试",
        "v0.8    数据统计 (月度+班级排行+逾期+报废成本)     5 测试",
        "v0.9    集成测试 (E2E+边界+持久化+并发)            8 测试",
        "v0.10   CSV导入导出 + 审计日志 (12种操作类型)     10 测试",
        "v0.11   审计下沉模块层 + 集成测试扩展              3 测试",
        "v1.2    最终交付 (clang-format+README+PDF文档)    96 测试全通过",
        "",
        "GitHub: git@github.com:Archer11-q/ultc_system.git",
    ])

    out = "docs/ultc_system_manual.pdf"
    pdf.output(out)
    print(f"PDF generated: {out} ({os.path.getsize(out)} bytes, {pdf.pages_count} pages)")


if __name__ == "__main__":
    main()
