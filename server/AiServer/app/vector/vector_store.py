import chromadb
from openai import OpenAI
from app.core.config import get_settings

# 持久化目录
chroma_client = chromadb.PersistentClient(
    path="./chroma_db"
)

collection = chroma_client.get_or_create_collection(
    name="chat_memory"
)

embed_client = OpenAI(
    api_key=get_settings().SILLICONFLOW_API_KEY,
    base_url="https://api.siliconflow.cn/v1"
)