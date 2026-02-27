from app.vector.vector_store import embed_client
from app.vector.vector_store import collection

def embed_text(text: str):
    """
    调用 embedding API
    """

    resp = embed_client.embeddings.create(
        model="BAAI/bge-large-zh-v1.5",
        input=text
    )

    return resp.data[0].embedding


def debug_dump_vectors(limit=10):
    res = collection.get(
        limit=limit,
        include=["documents", "metadatas", "embeddings"]
    )

    print("=== IDS ===")
    print(res["ids"])

    print("\n=== DOCUMENTS ===")
    for d in res["documents"]:
        print(d)

    print("\n=== METADATA ===")
    for m in res["metadatas"]:
        print(m)