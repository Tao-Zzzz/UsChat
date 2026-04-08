import threading
import time
import os
# 导入你获取数据库 Session 的方法
from app.core.db import SessionLocal 
from app.services.chat_service.vector_service import sync_pending_messages

SYNC_TIME_FILE = "last_vector_sync.txt"
SYNC_INTERVAL = 30 * 60  # 30分钟 = 1800秒

def get_last_sync_time():
    if os.path.exists(SYNC_TIME_FILE):
        with open(SYNC_TIME_FILE, "r") as f:
            content = f.read().strip()
            return float(content) if content else 0.0
    return 0.0

def update_last_sync_time():
    with open(SYNC_TIME_FILE, "w") as f:
        f.write(str(time.time()))

def background_vector_task():
    """
    后台守护线程：负责每 30 分钟同步一次向量
    """
    print("[Vector Task] Background vectorization thread started.")
    while True:
        try:
            last_time = get_last_sync_time()
            current_time = time.time()
            
            # 如果距离上次同步大于等于 30 分钟，或者之前从来没同步过
            if current_time - last_time >= SYNC_INTERVAL:
                print("[Vector Task] Triggering sync pending messages...")
                
                # 新开一个数据库会话
                with SessionLocal() as db:
                    processed_count = sync_pending_messages(db)
                    
                if processed_count > 0:
                    print(f"[Vector Task] Successfully vectorized {processed_count} messages.")
                
                # 更新本地时间戳
                update_last_sync_time()
                
        except Exception as e:
            print(f"[Vector Task Error]: {e}")
            
        # 线程休息 1 分钟后再检查一次条件，避免死循环占满 CPU
        time.sleep(60) 

# 在你的 FastAPI/Flask 启动事件中调用这个方法
def start_vector_scheduler():
    t = threading.Thread(target=background_vector_task, daemon=True)
    t.start()