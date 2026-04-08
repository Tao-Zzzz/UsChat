import numpy as np
from sentence_transformers import SentenceTransformer
from app.models.models import ChatMessage, ChatMessageVector
import os


current_dir = os.path.dirname(os.path.abspath(__file__))

app_dir = os.path.abspath(os.path.join(current_dir, "..", ".."))
MODEL_PATH = os.path.join(
    app_dir,
    "embedding_model",
    "paraphrase-multilingual-MiniLM-L12-v2"
)

print("MODEL_PATH =", MODEL_PATH)
encoder = SentenceTransformer(MODEL_PATH)


def sync_all_old_messages(db, uid):
    """
    一次性同步：把某个用户所有旧的聊天记录转成向量
    建议在后台脚本中运行一次
    """
    # 1. 查找所有还未转换的消息
    # 这里通过 ChatMessageVector 关联查询过滤，或者简单点：查出所有
    msgs = db.query(ChatMessage).filter(
        (ChatMessage.sender_id == uid) | (ChatMessage.recv_id == uid)
    ).all()

    for m in msgs:
        # 确定这条消息归属哪段好友关系
        # (简化逻辑：假设我们存的时候记下 uid 和对面那个 friend_id)
        fid = m.recv_id if m.sender_id == uid else m.sender_id
        
        # 生成向量
        vector = encoder.encode(m.content).tolist()
        
        # 存入向量表
        vec_obj = ChatMessageVector(
            message_id=m.message_id,
            uid=uid,
            friend_id=fid,
            content=m.content,
            embedding=vector
        )
        db.merge(vec_obj)
    db.commit()
    print(f"Finished syncing {len(msgs)} messages for user {uid}")

def search_semantic_in_mysql(db, uid, fid, query, limit=5):
    """
    在 MySQL 中手动计算余弦相似度进行搜索
    """
    # 1. 把搜索词向量化
    query_vec = encoder.encode(query)
    
    # 2. 从数据库拉取该好友的所有向量 (JSON)
    # 注意：如果消息极多，这里需要做分批处理，但 5000 条以内直接拉取没压力
    all_vectors = db.query(ChatMessageVector).filter(
        ChatMessageVector.uid == uid, 
        ChatMessageVector.friend_id == fid
    ).all()
    
    if not all_vectors:
        return []

    # 3. 计算相似度
    scored_results = []
    for item in all_vectors:
        # 将 JSON 转换回 numpy 数组进行计算
        vec = np.array(item.embedding)
        # 计算余弦相似度
        score = np.dot(query_vec, vec) / (np.linalg.norm(query_vec) * np.linalg.norm(vec))
        scored_results.append((score, item.content))
    
    # 4. 按分数排序并取前 N
    scored_results.sort(key=lambda x: x[0], reverse=True)
    return [content for score, content in scored_results[:limit]]