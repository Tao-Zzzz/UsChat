import json
import numpy as np
from database import get_db_conn
from config import Config

class FaceService:
    @staticmethod
    def register_face(uid: int, feature: list) -> bool:
        """
        保存或更新人脸特征。
        由于取消了物理外键，建议在业务层确保 uid 是有效的。
        """
        conn = get_db_conn()
        try:
            with conn.cursor() as cursor:
                # 1. (可选) 逻辑检查：确保用户确实存在于 user 表
                # cursor.execute("SELECT 1 FROM user WHERE uid = %s", (uid,))
                # if not cursor.fetchone(): return False

                # 2. 插入或更新
                feature_str = json.dumps(feature)
                sql = """
                    INSERT INTO user_face_feature (uid, face_feature) 
                    VALUES (%s, %s)
                    ON DUPLICATE KEY UPDATE face_feature = VALUES(face_feature)
                """
                cursor.execute(sql, (uid, feature_str))
            conn.commit()
            return True
        except Exception as e:
            print(f"Face Register DB Error: {e}")
            return False
        finally:
            conn.close()

    @staticmethod
    def search_face(query_feature: list) -> dict:
        """1:N 人脸比对搜索"""
        conn = get_db_conn()
        try:
            with conn.cursor() as cursor:
                # 扫描 user_face_feature 表
                cursor.execute("SELECT uid, face_feature FROM user_face_feature")
                users = cursor.fetchall()

            if not users:
                return {"error": 1006, "msg": "人脸库为空"}

            query_vec = np.array(query_feature, dtype=np.float32)
            
            # 这里的计算逻辑保持不变
            best_uid = -1
            max_score = -1.0
            norm_query = np.linalg.norm(query_vec)

            for u in users:
                db_vec = np.array(json.loads(u['face_feature']), dtype=np.float32)
                denom = norm_query * np.linalg.norm(db_vec)
                if denom == 0: continue
                
                score = np.dot(query_vec, db_vec) / denom
                if score > max_score:
                    max_score = score
                    best_uid = u['uid']

            if max_score > Config.FACE_MATCH_THRESHOLD:
                # 找到匹配，返回对应的 uid
                return {"error": 0, "uid": best_uid, "score": float(max_score)}
            else:
                return {"error": 1007, "msg": "人脸不匹配"}
        finally:
            conn.close()