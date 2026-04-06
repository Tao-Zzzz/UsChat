# config.py
class Config:
    # 你的 MySQL 配置
    DB_HOST = '127.0.0.1'
    DB_PORT = 3306
    DB_USER = 'root'
    DB_PASSWORD = '905049' # 替换为你的密码
    DB_NAME = 'uschat'      # 替换为你的数据库名

    # AI 识别阈值 (SFace 模型余弦相似度推荐阈值)
    FACE_MATCH_THRESHOLD = 0.363