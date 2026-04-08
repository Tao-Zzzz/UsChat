from sqlalchemy import Column, Integer, String, Text, DateTime, BigInteger, ForeignKey, Boolean, TIMESTAMP, func
from datetime import datetime
from app.core.db import Base
from sqlalchemy.dialects.mysql import INTEGER


class AIThread(Base):
    __tablename__ = "ai_thread"

    id = Column(Integer, primary_key=True, autoincrement=True)
    uid = Column(BigInteger, index=True, nullable=False)
    title = Column(String(128))
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)
    is_deleted = Column(Boolean, default=False)
    summary = Column(Text)            # 新增
    summary_tokens = Column(Integer)  # 新增

class AIMessage(Base):
    __tablename__ = "ai_message"

    id = Column(Integer, primary_key=True, autoincrement=True)
    ai_thread_id = Column(Integer, ForeignKey("ai_thread.id"), index=True)
    role = Column(String(16))
    content = Column(Text)
    model = Column(String(64))
    tokens = Column(Integer)
    created_at = Column(DateTime, default=datetime.utcnow)

    # 【新增字段】：标记该消息是否已经被折叠进会话摘要中
    is_summarized = Column(Boolean, default=False, index=True)

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
    supports_system = Column(Boolean, default=True)  # 新增
    context_length = Column(Integer, default=8192)   # 新增


class ChatMessage(Base):
    __tablename__ = "chat_message"

    # 主键，对应图片中的 message_id (BIGINT UNSIGNED, AUTO_INCREMENT)
    message_id = Column(BigInteger, primary_key=True, autoincrement=True, comment="消息唯一ID")
    
    # 外键/索引，对应 thread_id (BIGINT UNSIGNED)
    thread_id = Column(BigInteger, index=True, nullable=False, comment="会话ID")
    
    # 发送者ID (BIGINT UNSIGNED)
    sender_id = Column(BigInteger, nullable=False, comment="发送者UID")
    
    # 接收者ID (BIGINT UNSIGNED)
    recv_id = Column(BigInteger, nullable=False, comment="接收者UID")
    
    # 消息内容 (TEXT)
    content = Column(Text, nullable=False, comment="消息内容")
    
    # 创建时间 (TIMESTAMP, 默认 CURRENT_TIMESTAMP)
    created_at = Column(TIMESTAMP, server_default=func.current_timestamp(), index=True, comment="创建时间")
    
    # 更新时间 (TIMESTAMP, 默认 CURRENT_TIMESTAMP)
    updated_at = Column(TIMESTAMP, server_default=func.current_timestamp(), onupdate=func.current_timestamp())
    
    # 消息状态 (TINYINT, 默认 0)
    # 0=未读 1=已读 2=...
    status = Column(Integer, default=0, comment="0=未读 1=已读 2=撤回")
    
    # 消息类型 (TINYINT, 默认 0)
    # 0=文本 1=图片 2=...
    msg_type = Column(Integer, default=0, comment="0=文本 1=图片 2=视频 3=文件")

class Friend(Base):
    __tablename__ = "friend"

    # 主键 ID (INT UNSIGNED, AUTO_INCREMENT)
    id = Column(INTEGER(unsigned=True), primary_key=True, autoincrement=True)
    
    # 用户自己的 ID (INT)
    self_id = Column(Integer, nullable=False, index=True)
    
    # 好友的 ID (INT)
    friend_id = Column(Integer, nullable=False, index=True)
    
    # 备注/背景 (VARCHAR 255)，允许为空，默认为空字符串
    back = Column(String(255), nullable=True, server_default="")


class User(Base):
    __tablename__ = "user"

    # 逻辑主键 (INT, AUTO_INCREMENT)
    id = Column(Integer, primary_key=True, autoincrement=True)
    
    # 业务唯一标识 UID (INT)，默认 '0'
    uid = Column(Integer, nullable=False, unique=True, server_default="0")
    
    # 用户名 (VARCHAR 255)
    name = Column(String(255), nullable=False, server_default="")
    
    # 邮箱 (VARCHAR 255)
    email = Column(String(255), nullable=False, server_default="")
    
    # 密码 HASH (VARCHAR 255)
    pwd = Column(String(255), nullable=False, server_default="")
    
    # 昵称 (VARCHAR 255)
    nick = Column(String(255), nullable=False, server_default="")
    
    # 个人签名/描述 (VARCHAR 255)
    desc = Column(String(255), nullable=False, server_default="")
    
    # 性别 (INT)，默认 '0'
    sex = Column(Integer, nullable=False, server_default="0")
    
    # 头像路径/URL (VARCHAR 255)
    icon = Column(String(255), nullable=False, server_default="")

class SemanticMemory(Base):
    __tablename__ = "semantic_memory"

    id = Column(Integer, primary_key=True, autoincrement=True)
    uid = Column(Integer, index=True, nullable=False) # 关联用户的 uid
    entity = Column(String(128), index=True, comment="实体，例如 '自己' 或 '老王'")
    attribute = Column(String(128), comment="属性，例如 '职业', '喜好', '人际关系'")
    fact = Column(String(255), comment="具体事实，例如 '是一名Python后端开发'")
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)