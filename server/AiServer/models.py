from sqlalchemy import Column, Integer, String, Text, DateTime, BigInteger, ForeignKey, Boolean
from datetime import datetime
from db import Base


class AIThread(Base):
    __tablename__ = "ai_thread"

    id = Column(Integer, primary_key=True, autoincrement=True)
    uid = Column(BigInteger, index=True, nullable=False)
    title = Column(String(128))
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    is_deleted = Column(Boolean, default=False)


class AIMessage(Base):
    __tablename__ = "ai_message"

    id = Column(Integer, primary_key=True, autoincrement=True)
    ai_thread_id = Column(Integer, ForeignKey("ai_thread.id"), index=True)
    role = Column(String(16))
    content = Column(Text)
    model = Column(String(64))
    tokens = Column(Integer)
    created_at = Column(DateTime, default=datetime.utcnow)
