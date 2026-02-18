import sys
import os

# 1. 强制定位到 AiServer 根目录
current_file_path = os.path.abspath(__file__) # 脚本的绝对路径
scripts_dir = os.path.dirname(current_file_path) # scripts 文件夹路径
root_dir = os.path.dirname(scripts_dir) # AiServer 根路径

# 2. 插入到搜索路径的最前面
if root_dir not in sys.path:
    sys.path.insert(0, root_dir)

# 调试用：看看打印出来的是不是你的 AiServer 文件夹
print(f"DEBUG: Root dir is {root_dir}") 

try:
    from app.db import SessionLocal
    from app.models import AIModel
    print("DEBUG: Import successful!")
except ImportError as e:
    print(f"DEBUG: Import failed! Error: {e}")
    sys.exit(1)

# ... 后面接你的 insert_gemma 函数
from sqlalchemy.orm import Session
from app.db import SessionLocal
from app.models import AIModel

def insert_gemma():
    db: Session = SessionLocal()

    exists = db.query(AIModel).filter(
        AIModel.name == "gemma-3-12b"
    ).first()

    if exists:
        print("Model already exists")
        return

    model = AIModel(
        name="gemma-3-12b",
        model_key="google/gemma-3-12b",
        provider="openrouter",
        is_enabled=True,
        sort_order=1
    )

    db.add(model)
    db.commit()
    print("Inserted gemma-3-12b")



insert_gemma()