
import time
import numpy as np
from sentence_transformers import SentenceTransformer
from sqlalchemy.orm import Session
# 根据你的实际路径导入模型
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
encoder = SentenceTransformer(MODEL_PATH, backend="onnx")

def sync_pending_messages(db: Session):
    """
    扫描所有未向量化的单聊消息，进行批量向量化
    """
    # 1. 查找未向量化，且不是群聊 (recv_id != 0) 的消息
    # 为了避免一次查太多导致内存溢出，可以加个 limit=1000 循环处理
    pending_msgs = (
        db.query(ChatMessage)
        .filter(ChatMessage.is_vector == 0, ChatMessage.recv_id != 0)
        .limit(1000)
        .all()
    )

    if not pending_msgs:
        return 0

    count = 0
    for m in pending_msgs:
        try:
            # 跳过纯图片、视频等非文本消息 (根据你的 msg_type 过滤)
            if m.msg_type != 0 or not m.content.strip():
                m.is_vector = 1 # 标记为已处理（即使不生成向量也标记，避免死循环）
                continue

            vector = encoder.encode(m.content).tolist()

            # 视角1：对于发送者而言，friend_id 是接收者
            vec_sender = ChatMessageVector(
                message_id=m.message_id,
                uid=m.sender_id,
                friend_id=m.recv_id,
                content=m.content,
                embedding=vector
            )
            
            # 视角2：对于接收者而言，friend_id 是发送者
            vec_recv = ChatMessageVector(
                message_id=m.message_id,
                uid=m.recv_id,
                friend_id=m.sender_id,
                content=m.content,
                embedding=vector
            )

            db.add(vec_sender)
            db.add(vec_recv)
            
            # 标记原消息为已向量化
            m.is_vector = 1 
            count += 1
            
        except Exception as e:
            print(f"[Vector Sync] Error processing msg {m.message_id}: {e}")
            # 可以选择不标记，下次重试

    db.commit()
    return count

def sync_message_to_vector(db, uid, fid, msg_id, content):
    """将新产生的聊天记录转换并存入数据库"""
    vector = encoder.encode(content).tolist()
    
    vec_obj = ChatMessageVector(
        message_id=msg_id,
        uid=uid,
        friend_id=fid,
        embedding=vector,
        content=content
    )
    db.merge(vec_obj) # 使用 merge 防止重复插入
    db.commit()

def semantic_search_messages(db, uid, fid, query, limit=5):
    """语义搜索：寻找最相关的聊天记录"""
    query_vector = encoder.encode(query).tolist()
    
    # 使用 pgvector 的 L2 距离 (<->) 或 余弦相似度 (<=>) 进行排序
    results = (
        db.query(ChatMessageVector)
        .filter(ChatMessageVector.uid == uid, ChatMessageVector.friend_id == fid)
        .order_by(ChatMessageVector.embedding.cosine_distance(query_vector))
        .limit(limit)
        .all()
    )
    return [r.content for r in results]

    
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