from fastapi import FastAPI, Depends
from sqlalchemy.orm import Session

from db import get_db
from schemas import ChatRequest, ChatResponse, AIChatMessage, AIThreadItem
from service import handle_chat, load_ai_threads, load_ai_messages

app = FastAPI()

@app.post("/ai/chat", response_model=ChatResponse)
def chat(req: ChatRequest, db: Session = Depends(get_db)):
    (
        ai_thread_id,
        reply,
        created,
        user_msg_id,
        ai_msg_id,
        created_at,
        title,
        unique_id # 接收 handle_chat 傳回的 ID
    ) = handle_chat(db, req)

    return ChatResponse(
        ai_thread_id=ai_thread_id,
        reply=reply,
        created=created,
        user_msg_id=user_msg_id,
        ai_msg_id=ai_msg_id,
        created_at=created_at,
        title=title,
        unique_id=unique_id # 回傳給前端
    )

@app.get("/ai/load_thread", response_model=list[AIThreadItem])
def load_thread(uid: int, db: Session = Depends(get_db)):
    threads = load_ai_threads(db, uid)
    return [
        AIThreadItem(
            ai_thread_id=t.id,
            title=t.title,
            updated_at=t.updated_at
        )
        for t in threads
    ]

@app.get("/ai/load_chat_msg", response_model=list[AIChatMessage])
def load_chat_msg(
    uid: int,
    ai_thread_id: int,
    db: Session = Depends(get_db)
):
    msgs = load_ai_messages(db, uid, ai_thread_id)
    return [
        AIChatMessage(
            role=m.role,
            content=m.content,
            created_at=m.created_at
        )
        for m in msgs
    ]