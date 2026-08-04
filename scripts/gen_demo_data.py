#!/usr/bin/env python3
"""演示数据捕获 — 每个场景独立运行。末尾追加冗余 0 确保退出。"""
import subprocess, os, re, sys

EXE = "./build/ultc_system"
OUT = "docs/screenshots"
os.makedirs(OUT, exist_ok=True)

# 确保每次干净启动
def clean():
    for f in ["data/admin.dat","data/material.dat","data/scrap.dat",
              "data/borrow.dat","data/stocktake.dat","data/audit.dat"]:
        if os.path.exists(f): os.remove(f)

# 末尾填充大量 0 + 换行以确保程序完全退出
EXIT_PAD = ["0"] * 20

def cap(name, *lines):
    clean()
    in_text = "\n".join(str(x) for x in list(lines) + EXIT_PAD) + "\n"
    try:
        r = subprocess.run([EXE], input=in_text, capture_output=True,
                          text=True, timeout=20)
    except subprocess.TimeoutExpired:
        print(f"  [WARN] {name} timeout, skipping")
        return ""
    clean_text = re.sub(r'\x1b\[[0-9;]*[a-zA-Z]', '', r.stdout)
    clean_text = re.sub(r'\x1b\]0;.*?\x07', '', clean_text)
    path = os.path.join(OUT, f"{name}.txt")
    with open(path, "w", encoding="utf-8") as f:
        f.write(clean_text)
    print(f"  [OK] {name} ({len(clean_text)} chars)")
    return clean_text

# CSV文件
csv_path = "/mnt/d/CLion/ultc_system/demo_import.csv"
with open(csv_path, "w", encoding="utf-8-sig") as f:
    f.write("编号,名称,分类,属性,单价,库存,预警,柜号,采购日期\n")
    f.write("R001,电阻 10kΩ 1/4W,电子元器件,一次性,0.05,500,50,A-01,2024-09-01\n")
    f.write("R002,电阻 100Ω 1/2W,电子元器件,一次性,0.10,300,30,A-01,2024-09-01\n")
    f.write("CAP001,电解电容 100μF,电子元器件,一次性,0.20,200,40,A-02,2024-09-01\n")
    f.write("DEV001,Arduino Uno R3,开发板,可循环,68.00,20,5,B-01,2024-08-15\n")
    f.write("DEV002,STM32F103C8T6,开发板,可循环,25.00,15,5,B-02,2024-08-15\n")
    f.write("TOOL01,数字万用表,电工工具,可循环,120.00,8,3,C-01,2024-07-20\n")
    f.write("TOOL02,电烙铁 60W,电工工具,可循环,45.00,10,3,C-02,2024-07-20\n")
    f.write("WIRE01,杜邦线 公母 20cm,机械零件,一次性,1.50,300,50,D-01,2024-06-01\n")
    f.write("CHEM01,焊锡丝 0.8mm,化学耗材,一次性,8.50,150,30,E-01,2024-06-15\n")
    f.write("CHEM02,助焊剂,化学耗材,一次性,12.00,80,20,E-02,2024-06-15\n")

# ===== 1. 登录界面 =====
cap("01-login", 1, "admin", "admin123")

# ===== 2. CSV批量导入 =====
cap("02-csv-import",
    1, "admin", "admin123",
    14, csv_path,     # CSV导入
    "",               # pause
    "",               # another pause
    "",               # yet another
    "", "", "", "", "")

# ===== 3. 新增耗材 =====
cap("03-add-material",
    1, "admin", "admin123",
    1, "BATT01", "锂电池 3.7V 2000mAh", "1", "1", "15.00", "60", "15", "F-01", "2024-10-01",
    "",
    1, "SERVO01", "SG90 舵机", "4", "1", "8.00", "40", "10", "F-02", "2024-10-01",
    "")

# ===== 4. 耗材列表分页 =====
cap("04-material-list",
    1, "admin", "admin123",
    4,              # 耗材列表
    "n",            # 下一页
    "n",            # 再下一页
    "q")            # 返回

# ===== 5. 学生领用 =====
cap("05-borrow",
    1, "admin", "admin123",
    5,              # 学生领用
    "2021001", "张三", "计科2101", "PRJ-EMBEDDED",
    "1", "R001", "100",  # 领用电阻
    "",
    "1", "DEV001", "1",  # 领用Arduino
    "",
    "1", "WIRE01", "20", # 领用杜邦线
    "",
    "2")            # 完成领用

# ===== 6. 库存预警+采购清单 =====
cap("06-alert",
    1, "admin", "admin123",
    8,              # 库存预警
    "y")            # 生成采购清单

# ===== 7. 精准检索 =====
cap("07-search-exact",
    1, "admin", "admin123",
    10,             # 检索
    1, "DEV001",    # 精准查询
    "",
    0)              # 返回

# ===== 8. 模糊搜索 =====
cap("08-search-fuzzy",
    1, "admin", "admin123",
    10,             # 检索
    2, "电阻",      # 模糊搜索
    "q",            # 退出搜索结果
    "",
    0)              # 返回

# ===== 9. 数据统计概览 =====
cap("09-stats",
    1, "admin", "admin123",
    12,             # 统计
    5)              # 全部概览

# ===== 10. 助教登录+受限菜单 =====
# 先新增助教
cap("10-admin-manage",
    1, "admin", "admin123",
    13,             # 管理员管理
    1, "ta01", "pass123", "2",  # 新增助教
    "",
    0)              # 返回

# 助教登录
cap("11-ta-menu",
    1, "ta01", "pass123")

# ===== 11. 审计日志 =====
cap("12-audit",
    1, "admin", "admin123",
    16,             # 审计日志
    "q")            # 返回

# ===== 12. CSV导出 =====
cap("13-csv-export",
    1, "admin", "admin123",
    15,             # CSV导出
    1,              # 导出耗材
    "",             # 使用默认文件名
    "",
    0)              # 返回

# ===== 13. 逾期管理 =====
cap("14-overdue",
    1, "admin", "admin123",
    7)              # 逾期管理

# 清理
for f in ["demo_import.csv", "demo_export.csv", "materials_" + "*" + ".csv"]:
    try:
        if os.path.exists(f): os.remove(f)
    except: pass
if os.path.exists(csv_path): os.remove(csv_path)

print("\n全部截图保存到", OUT)
