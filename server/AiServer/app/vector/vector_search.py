from app.vector.vector_store import collection
from app.vector.vector_utils import embed_text

def search_vectors(
    uid,
    text,
    k=5
):
    """
    搜索用户历史语义相关聊天
    """

    emb = embed_text(text)

    result = collection.query(
        query_embeddings=[emb],
        n_results=k,
        where={
            "uid": uid
        }
    )

    if not result["documents"]:
        return []

    return result["documents"][0]