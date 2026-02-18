from sqlalchemy import Column, Integer, String, Text, DateTime, BigInteger, ForeignKey, Boolean
from datetime import datetime
from app.db import Base


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

class AIModel(Base):
    __tablename__ = "ai_model"

    id = Column(Integer, primary_key=True, autoincrement=True)

    name = Column(String(64), nullable=False, unique=True)

    # 真正调用模型时使用的标识，比如 openai:gpt-4
    model_key = Column(String(128), nullable=False)

    # 模型类型，比如 openai / deepseek / local
    provider = Column(String(32), nullable=False)

    # 是否启用
    is_enabled = Column(Boolean, default=True)

    # 排序字段，方便控制前端显示顺序
    sort_order = Column(Integer, default=0)

    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime,
                        default=datetime.utcnow,
                        onupdate=datetime.utcnow)
