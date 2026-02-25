from app.vector.vector_store import collection
from app.vector.vector_utils import embed_text
from app.models.models import VectorMemory
from datetime import datetime

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