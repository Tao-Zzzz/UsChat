from app.vector.vector_search import search_vectors

def build_vector_context(uid, text):

    docs = search_vectors(uid, text)

    if not docs:
        return ""

    return "相关历史记录:\n" + "\n".join(docs)