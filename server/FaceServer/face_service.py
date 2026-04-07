import json
import numpy as np
import faiss
from database import get_db_conn
from config import Config

class FaceService:
    # 全局单例的 FAISS 索引
    index = None
    # 你的模型特征维度 (SFace 是 128，如果是其他模型请修改)
    DIMENSION = 128 

    @classmethod
    def init_faiss(cls):
        """
        服务启动时调用：从数据库全量加载特征到 FAISS 内存中
        """
        # 使用内积 (Inner Product) 建立索引
        # 当向量经过 L2 归一化后，内积等价于余弦相似度
        base_index = faiss.IndexFlatIP(cls.DIMENSION)
        # 包装一层 IDMap，这样我们可以直接用数据库的 uid 作为 FAISS 的内部 ID, 这个包装器允许我们绑定自定义的 uid
        cls.index = faiss.IndexIDMap(base_index)

        conn = get_db_conn()
        try:
            with conn.cursor() as cursor:
                cursor.execute("SELECT uid, face_feature FROM user_face_feature")
                users = cursor.fetchall()

            if not users:
                print("FAISS Init: No faces in database.")
                return

            uids = []
            features = []
            for u in users:
                # 将json转换成 32 位浮点数的 Numpy 数组
                vec = np.array(json.loads(u['face_feature']), dtype=np.float32)
                uids.append(u['uid'])
                features.append(vec)

            # 转换为 FAISS 需要的格式并进行 L2 归一化
            features_np = np.array(features, dtype=np.float32)
            faiss.normalize_L2(features_np)
            uids_np = np.array(uids, dtype=np.int64) # FAISS 的 ID 必须是 int64

            # 批量添加到索引
            cls.index.add_with_ids(features_np, uids_np)
            print(f"FAISS Init Success: Loaded {cls.index.ntotal} faces into memory.")

        except Exception as e:
            print(f"FAISS Init Error: {e}")
        finally:
            conn.close()

    @classmethod
    def register_face(cls, uid: int, feature: list) -> bool:
        """保存特征到数据库，并同步到 FAISS 索引"""
        conn = get_db_conn()
        try:
            # 1. 写入数据库持久化
            with conn.cursor() as cursor:
                feature_str = json.dumps(feature)
                sql = """
                    INSERT INTO user_face_feature (uid, face_feature) 
                    VALUES (%s, %s)
                    ON DUPLICATE KEY UPDATE face_feature = VALUES(face_feature)
                """
                cursor.execute(sql, (uid, feature_str))
            conn.commit()

            # 2. 同步到 FAISS 内存索引
            if cls.index is not None:
                vec_np = np.array([feature], dtype=np.float32)
                faiss.normalize_L2(vec_np) # 必须归一化
                uid_np = np.array([uid], dtype=np.int64)
                
                # 如果是覆盖更新(ON DUPLICATE KEY)，需要先从 FAISS 删掉旧的再加新的
                cls.index.remove_ids(uid_np) 
                cls.index.add_with_ids(vec_np, uid_np)

            return True
        except Exception as e:
            print(f"Face Register Error: {e}")
            return False
        finally:
            conn.close()

    @classmethod
    def search_face(cls, query_feature: list) -> dict:
        """使用 FAISS 进行 1:N 极速搜索"""
        if cls.index is None or cls.index.ntotal == 0:
            return {"error": 1006, "msg": "人脸库为空或未初始化"}

        # 1. 准备查询向量并归一化
        query_vec = np.array([query_feature], dtype=np.float32)
        faiss.normalize_L2(query_vec)

        # 2. FAISS 搜索 (k=1 表示寻找最相似的 1 个人)
        # D 是距离矩阵(余弦相似度分数)，I 是对应的 ID 矩阵(即我们的 uid)
        D, I = cls.index.search(query_vec, k=1)

        # 第0张脸(输入可以多), 最接近的第0个(输出可以多)
        best_score = float(D[0][0])
        best_uid = int(I[0][0])

        # 3. 结果判断
        # 因为我们用了 IndexFlatIP 并归一化，这里的 distance 就是 Cosine Similarity (范围 -1 到 1)
        if best_uid != -1 and best_score > Config.FACE_MATCH_THRESHOLD:
            return {"error": 0, "uid": best_uid, "score": best_score}
        else:
            return {"error": 1007, "msg": "人脸不匹配"}