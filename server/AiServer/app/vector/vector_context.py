from app.vector.vector_search import search_vectors
from app.services.chat_service.friend_service import get_recent_chat_with_friend
from app.vector.vector_store import collection
from app.vector.vector_utils import embed_text


def build_vector_context(uid, text):

    docs = search_vectors(uid, text)

    if not docs:
        return ""

    return "相关历史记录:\n" + "\n".join(docs)

def build_vector_context_v2(uid: int, query: str, *, friend_id: int = 0, k: int = 4) -> str:
    """
    从向量库召回与 query 最相关的片段
    friend_id:
      - 0 代表“AI对话/通用记忆”
      - >0 代表“某个好友相关记忆/聊天”
    """
    if not query or not query.strip():
        return ""

    try:
        qemb = embed_text(query)

        res = collection.query(
            query_embeddings=[qemb],
            n_results=k,
            where={"uid": uid, "friend_id": friend_id},
            include=["documents", "metadatas", "distances"],
        )

        docs = (res.get("documents") or [[]])[0]
        metas = (res.get("metadatas") or [[]])[0]

        chunks = []
        for d, m in zip(docs, metas):
            mid = m.get("message_id")
            chunks.append(f"- (message_id={mid}) {d}")

        if not chunks:
            return ""

        return "召回到的相关内容如下：\n" + "\n".join(chunks)

    except Exception as e:
        print(f"[vector] query failed: {e}")
        return ""

def build_friend_context_by_vector(db, uid: int, friend_info: dict, user_query: str) -> str:
    """
    优先走向量召回好友相关片段；
    如果召回不到，再 fallback 到 recent N 条。
    """
    fid = friend_info["friend_id"]
    fname = friend_info["display_name"]

    # 1) 向量召回
    vc = build_vector_context_v2(uid, user_query, friend_id=fid, k=4)
    if vc:
        return f"与你好友【{fname}】的相关聊天/记忆（向量召回）：\n{vc}"

    # 2) fallback：最近聊天
    chat_text = get_recent_chat_with_friend(db, uid, fid, fname, limit=10)
    if chat_text:
        return f"与你好友【{fname}】最近聊天记录（fallback）：\n\n{chat_text}"

    return ""
