from pydantic import BaseModel
from datetime import datetime

class ChatRequest(BaseModel):
    uid: int
    ai_thread_id: int
    model: str
    content: str


class ChatResponse(BaseModel):
    ai_thread_id: int
    reply: str
    created: bool

class AIThreadItem(BaseModel):
    ai_thread_id: int
    title: str | None
    updated_at: datetime

class AIChatMessage(BaseModel):
    role: str
    content: str
    created_at: datetime