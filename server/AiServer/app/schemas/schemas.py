from pydantic import BaseModel
from datetime import datetime

class ChatRequest(BaseModel):
    uid: int
    ai_thread_id: int
    model: str
    content: str
    unique_id: str  # 新增：由前端生成的請求唯一識別碼（如 UUID）

class ChatResponse(BaseModel):
    ai_thread_id: int
    reply: str
    created: bool
    user_msg_id: int          # 新增：用戶消息的 ID
    ai_msg_id: int            # 原 message_id，改名後更直觀
    created_at: datetime
    title: str | None
    unique_id: str  # 新增：由前端生成的請求唯一識別碼（如 UUID）

class AIThreadItem(BaseModel):
    ai_thread_id: int
    title: str | None
    updated_at: datetime

class AIModelItem(BaseModel):
    id: int
    name: str

class LoadThreadResp(BaseModel):
    threads: list[AIThreadItem]
    models: list[AIModelItem]



class AIChatMessage(BaseModel):
    msg_id: int
    role: str
    content: str
    created_at: datetime

class AIChatMessageResp(BaseModel):
    ai_thread_id: int
    messages: list[AIChatMessage]


