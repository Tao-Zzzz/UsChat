from app.vector.vector_store import collection
from app.vector.vector_utils import embed_text
from app.models.models import VectorMemory

def store_chat_vector(
    db,
    uid,
    friend_id,
    message_id,
    content
):
    """
    存入向量数据库
    """

    if not content.strip():
        return

    emb = embed_text(content)

    # 存向量
    collection.add(
        ids=[str(message_id)],
        embeddings=[emb],
        documents=[content],
        metadatas=[{
            "uid": uid,
            "friend_id": friend_id
        }]
    )

    # 存 metadata
    vm = VectorMemory(
        uid=uid,
        friend_id=friend_id,
        message_id=message_id,
        content=content,
        created_at=datetime.utcnow()
    )

    db.add(vm)
    db.commit()


from app.vector.vector_store import collection
from app.vector.vector_utils import embed_text
from app.models.models import VectorMemory
from datetime import datetime
import hashlib
import time

def _vector_id(uid: int, friend_id: int, message_id: int) -> str:
    # 避免不同来源 message_id 冲突（AIMessage/ChatMessage 都可能从 1 开始）
    raw = f"{uid}:{friend_id}:{message_id}"
    return hashlib.sha1(raw.encode("utf-8")).hexdigest()


def init_vectors(db):
    msgs = db.query(VectorMemory).all()

    inserted = 0
    skipped = 0
    failed = 0

    for msg in msgs:
        vid = _vector_id(msg.uid, msg.friend_id, msg.message_id)

        try:
            existing = collection.get(ids=[vid])

            if existing["ids"]:
                skipped += 1
                continue

            store_chat_vector_v2(
                db,
                msg.uid,
                msg.friend_id,
                msg.message_id,
                msg.content,
            )

            inserted += 1
            print(f"inserted {inserted}")

            time.sleep(1)

        except Exception as e:
            failed += 1
            print("failed:", e)

    print("done")
    print("inserted:", inserted)
    print("skipped:", skipped)
    print("failed:", failed)

def store_chat_vector_v2(
    db,
    uid: int,
    friend_id: int,
    message_id: int,
    content: str,
    *,
    commit: bool = False,   # 默认不提交，让上层事务控制
):
    """
    写入向量库 + 写入 VectorMemory（DB）
    - 不建议在这里 commit（除非你在离线任务里单独调用）
    - 向量库失败不应阻断主流程（可选择记录日志）
    """
    if not content or not content.strip():
        return

    vid = _vector_id(uid, friend_id, message_id)

    # 1) 写向量库（外部系统，建议容错）
    try:
        emb = embed_text(content)

        # 如果你的 collection 支持 upsert，用 upsert 更好
        # collection.upsert(...)
        collection.add(
            ids=[vid],
            embeddings=[emb],
            documents=[content],
            metadatas=[{"uid": uid, "friend_id": friend_id, "message_id": message_id}],
        )
        print("[vector inserted]", vid, content[:30])
    except Exception as e:
        # 这里建议接入 logger；先 print 也行
        print(f"[vector] add failed: {e}")

    # 2) 写 DB（VectorMemory）——交给上层事务统一提交
    vm = VectorMemory(
        uid=uid,
        friend_id=friend_id,
        message_id=message_id,
        content=content,
        created_at=datetime.utcnow(),
    )
    db.add(vm)

    if commit:
        db.commit()
