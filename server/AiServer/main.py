from fastapi import FastAPI, Depends
from sqlalchemy.orm import Session

from db import get_db
from schemas import ChatRequest, ChatResponse
from service import handle_chat

app = FastAPI()


@app.post("/ai/chat", response_model=ChatResponse)
def chat(req: ChatRequest, db: Session = Depends(get_db)):
    ai_thread_id, reply, created = handle_chat(db, req)
    return ChatResponse(
        ai_thread_id=ai_thread_id,
        reply=reply,
        created=created
    )
