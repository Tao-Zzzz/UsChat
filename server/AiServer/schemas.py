from pydantic import BaseModel


class ChatRequest(BaseModel):
    uid: int
    ai_thread_id: int
    model: str
    content: str


class ChatResponse(BaseModel):
    ai_thread_id: int
    reply: str
    created: bool
