from app.vector.vector_store import embed_client

def embed_text(text: str):
    """
    调用 embedding API
    """

    resp = embed_client.embeddings.create(
        model="text-embedding-3-small",
        input=text
    )

    return resp.data[0].embedding