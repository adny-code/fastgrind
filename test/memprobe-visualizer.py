import json
import sys

# 1. 打开 JSON 文件并读取内容
with open(sys.argv[1], 'r', encoding='utf-8') as file:
    data = json.load(file)  # 解析 JSON 为 Python 字典

# 2. 打印字典内容
print(data)  # 直接打印整个字典

# 3. 格式化打印（可选）
print("\nFormatted output:")
print(json.dumps(data, indent=4, ensure_ascii=False))  # 缩进+中文支持
