from sqlalchemy.orm import Session
from app.models.models import AIThread, AIMessage, AIModel
from openai import OpenAI
from app.core.config import get_settings
from app.services.chat_service.message_builder import build_messages
from app.services.chat_service.ai_decision import need_vector_search
from app.vector.vector_context import build_vector_context, build_vector_context_v2, build_friend_context_by_vector
from app.vector.vector_store_service import store_chat_vector, store_chat_vector_v2
from app.services.chat_service.chat_service import insert_reference_context
from app.services.chat_service.friend_service import need_friend_context, detect_friend_from_text, get_recent_chat_with_friend


settings = get_settings()

def handle_chat(db, req):
    created = False

    # ========== 1) 先创建/查找 thread + 保存 user_msg（短事务） ==========
    with db.begin():
        if req.ai_thread_id == -1:
            thread = AIThread(uid=req.uid, title=(req.content[:20].strip() or "新對話"))
            db.add(thread)
            db.flush()  # 拿到 thread.id
            created = True
            ai_thread_id = thread.id
        else:
            thread = (
                db.query(AIThread)
                .filter(
                    AIThread.id == req.ai_thread_id,
                    AIThread.uid == req.uid,
                    AIThread.is_deleted == False,
                )
                .first()
            )
            if not thread:
                raise Exception("thread not found")
            ai_thread_id = thread.id

        user_msg = AIMessage(
            ai_thread_id=ai_thread_id,
            role="user",
            content=req.content,
            model=req.model,
            tokens=estimate_tokens(req.content),  # 如果你后面要拆 tokens 字段，可先保留
        )
        db.add(user_msg)
        db.flush()  # 拿到 user_msg.id

        # 找模型（放事务里也行，反正是读）
        model_row = (
            db.query(AIModel)
            .filter(AIModel.name == req.model, AIModel.is_enabled == True)
            .first()
        )
        if not model_row:
            raise Exception("model not found")

    # ========== 2) 构建上下文 messages（事务外，不锁 DB） ==========
    client = get_llm_client(model_row.provider)

    history_msgs = build_context(db, ai_thread_id, model_row.context_length - 2000)

    system_prompt = "你是一个逻辑严谨的AI助手\n用中文回答"

    messages = build_messages(model_row, system_prompt, history_msgs)

    # ----- 2.1 好友聊天上下文（按需注入） -----
    if need_friend_context(req.content):
        friend_info = detect_friend_from_text(db, req.uid, req.content)
        if friend_info:
            chat_text = get_recent_chat_with_friend(
                db,
                req.uid,
                friend_info["friend_id"],
                friend_info["display_name"],
                limit=10,
            )
            if chat_text:
                insert_reference_context(
                    messages,
                    f"与你好友【{friend_info['display_name']}】最近聊天记录：\n\n{chat_text}",
                    title="好友聊天记录",
                )

    # ----- 2.2 向量检索（按需注入） -----
    if need_vector_search(client, model_row.model_key, req.content):
        vector_context = build_vector_context(req.uid, req.content)
        if vector_context:
            insert_reference_context(messages, vector_context, title="向量检索结果")

    # ========== 3) 调用模型 ==========
    response = client.chat.completions.create(
        model=model_row.model_key,
        messages=messages,
        temperature=0.6,
        max_tokens=2048,
    )

    reply = (response.choices[0].message.content or "").strip()

    # tokens：尽量拆开；如果你表暂时没字段，就先用 total_tokens 写回 tokens
    usage = getattr(response, "usage", None)
    prompt_tokens = getattr(usage, "prompt_tokens", None) if usage else None
    completion_tokens = getattr(usage, "completion_tokens", None) if usage else None
    total_tokens = getattr(usage, "total_tokens", None) if usage else None

    # ========== 4) 保存 AI 回复 + 向量（短事务） ==========
    with db.begin():
        ai_msg = AIMessage(
            ai_thread_id=ai_thread_id,
            role="assistant",
            content=reply,
            model=req.model,
            tokens=completion_tokens or total_tokens or estimate_tokens(reply),
        )
        db.add(ai_msg)
        db.flush()  # 拿到 ai_msg.id

        # ===== 存向量：建议分别存 user 与 assistant =====
        # 你原来是 store_chat_vector(db, uid, 0, ai_msg.id, req.content)
        # 我建议改成：分别存两次
        store_chat_vector(db, req.uid, 0, user_msg.id, req.content)  # 用户消息向量
        store_chat_vector(db, req.uid, 0, ai_msg.id, reply)          # AI 消息向量

    return (
        ai_thread_id,
        reply,
        created,
        user_msg.id,
        ai_msg.id,
        ai_msg.created_at,
        thread.title,
        req.unique_id,
    )




def get_llm_client(provider: str):
    if provider == "openrouter":
        return OpenAI(
            api_key=settings.OPENROUTER_API_KEY,
            base_url="https://openrouter.ai/api/v1"
        )
    elif provider == "openai":
        return OpenAI(
            api_key=settings.OPENAI_KEY
        )
    elif provider == "deepseek":
        return OpenAI(
            api_key=settings.DEEPSEEK_KEY,
            base_url="https://api.deepseek.com/v1"
        )
    elif provider == "siliconflow":
        return OpenAI(
            api_key=settings.SILLICONFLOW_API_KEY,
            base_url="https://api.siliconflow.cn/v1"
        )
    else:
        raise Exception(f"unsupported provider: {provider}")
    

def handle_chat_v2(db, req):
    created = False

    # ========== A) 创建/查找 thread + 保存 user_msg（短事务） ==========
    # 去掉 with db.begin(): 后的逻辑
    if req.ai_thread_id == -1:
        thread = AIThread(uid=req.uid, title=(req.content[:20].strip() or "新對話"))
        db.add(thread)
        db.flush()  # 确保 thread.id 被生成
        created = True
        ai_thread_id = thread.id
    else:
        thread = (
            db.query(AIThread)
            .filter(
                AIThread.id == req.ai_thread_id,
                AIThread.uid == req.uid,
                AIThread.is_deleted == False,
            )
            .first()
        )
        if not thread:
            raise Exception("thread not found")
        ai_thread_id = thread.id

    user_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="user",
        content=req.content,
        model=req.model,
        tokens=estimate_tokens(req.content),
    )
    if created: 
        try: 
            title = generate_title( client, model_row.model_key, req.content, reply_text ) 
            thread.title = title[:50] 
            db.commit() 
        except Exception as e: 
            print("title generation failed:", e)
    
    db.add(user_msg)
    db.flush()



    model_row = (
        db.query(AIModel)
        .filter(AIModel.name == req.model, AIModel.is_enabled == True)
        .first()
    )
    if not model_row:
        raise Exception("model not found")

    # 在函数最后建议手动 commit 一次，确保所有 flush 的内容持久化到数据库
    db.commit()
    db.flush()
    # ========== B) 组装 messages（事务外） ==========
    client = get_llm_client(model_row.provider)
    history_msgs = build_context(db, ai_thread_id, model_row.context_length - 2000)

    system_prompt = "你是一个逻辑严谨的AI助手\n用中文回答"
    messages = build_messages(model_row, system_prompt, history_msgs)

    # B1 好友上下文（需要时：先识别好友，再用向量召回）
    if need_friend_context(req.content):
        friend_info = detect_friend_from_text(db, req.uid, req.content)
        if friend_info:
            friend_ctx = build_friend_context_by_vector(db, req.uid, friend_info, req.content)
            if friend_ctx:
                insert_reference_context(messages, friend_ctx, title="好友相关上下文")

    # B2 通用向量检索（friend_id=0）
    if need_vector_search(client, model_row.model_key, req.content):
        vc = build_vector_context_v2(req.uid, req.content, friend_id=0, k=4)
        if vc:
            insert_reference_context(messages, vc, title="向量检索结果")

    # ========== C) 调模型 ==========
    response = client.chat.completions.create(
        model=model_row.model_key,
        messages=messages,
        temperature=0.6,
        max_tokens=2048,
    )

    reply = (response.choices[0].message.content or "").strip()
    usage = getattr(response, "usage", None)
    completion_tokens = getattr(usage, "completion_tokens", None) if usage else None
    total_tokens = getattr(usage, "total_tokens", None) if usage else None

    # ========== D) 保存 AI + 写向量（短事务；向量写不 commit） ==========
    # 1. 移除 with db.begin(): 并调整缩进
    ai_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="assistant",
        content=reply,
        model=req.model,
        tokens=completion_tokens or total_tokens or estimate_tokens(reply),
    )
    db.add(ai_msg)
    db.flush()  # 确保 ai_msg.id 被生成，供后续向量存储使用

    # 2. 向量存储：把 user/assistant 都存入 “通用记忆 friend_id=0”
    # 注意：这里 commit=False 是正确的，因为我们要统一在最后提交
    store_chat_vector_v2(db, req.uid, 0, user_msg.id, req.content, commit=False)
    store_chat_vector_v2(db, req.uid, 0, ai_msg.id, reply, commit=False)

    # 3. 针对特定好友的上下文处理
    if need_friend_context(req.content):
        friend_info = detect_friend_from_text(db, req.uid, req.content)
        if friend_info:
            fid = friend_info["friend_id"]
            store_chat_vector_v2(db, req.uid, fid, user_msg.id, req.content, commit=False)
            store_chat_vector_v2(db, req.uid, fid, ai_msg.id, reply, commit=False)
            
    if created:
        try:
            title = generate_title(
                client,
                model_row.model_key,
                req.content,
                reply
            )
            thread.title = title[:50]
        except Exception as e:
            print("title generation failed:", e)


    # 4. 最后统一提交所有更改（包括 AIMessage 和向量数据）
    db.commit()

    return (
        ai_thread_id,
        reply,
        created,
        user_msg.id,
        ai_msg.id,
        ai_msg.created_at,
        thread.title,
        req.unique_id,
    )

import re

def estimate_tokens(text: str) -> int:
    if not text:
        return 0
    text = text.strip()
    if not text:
        return 0

    # 粗略估算：中文/日文/韩文字符按 1 token，其他按 4 chars/token
    cjk = len(re.findall(r"[\u4e00-\u9fff\u3040-\u30ff\uac00-\ud7af]", text))
    other = len(text) - cjk
    approx = cjk + (other // 4) + 1
    return int(approx)




#裁剪
def build_context(db, thread_id, max_tokens, max_msgs=200):
    msgs = (
        db.query(AIMessage)
        .filter(AIMessage.ai_thread_id == thread_id)
        .order_by(AIMessage.id.desc())
        .limit(max_msgs)
        .all()
    )

    total = 0
    selected = []

    for msg in msgs:
        t = msg.tokens if msg.tokens is not None else estimate_tokens(msg.content)
        if total + t > max_tokens:
            break
        selected.append(msg)
        total += t

    selected.reverse()
    return selected



#生成总结
def generate_summary(client, model_key, messages, original_summary):
    text = "\n".join([
        f"{m.role}: {m.content}"
        for m in messages
    ])

    prompt = f"""
    已有总结：
    {original_summary}

    新增对话：
    {text}

    请合并总结
    """

    response = client.chat.completions.create(
        model=model_key,
        messages=[
            {"role": "user", "content": prompt}
        ],
        temperature=0.3
    )

    return response.choices[0].message.content

# 生成标题
def generate_title(client, model_key, user_msg, ai_reply):
    prompt = f"""
请为以下对话生成一个简短标题（不超过12个字）。
不要标点结尾。
不要解释。

用户：{user_msg}
AI：{ai_reply}
"""

    response = client.chat.completions.create(
        model=model_key,
        messages=[
            {"role": "user", "content": prompt}
        ],
        temperature=0.3,
        max_tokens=30
    )

    title = response.choices[0].message.content.strip()

    return title




def load_ai_threads(db, uid: int):
    threads = (
        db.query(AIThread)
        .filter(
            AIThread.uid == uid,
            AIThread.is_deleted == False
        )
        .order_by(AIThread.updated_at.desc())
        .all()
    )

    return threads

def load_enabled_models(db):
    models = (
        db.query(AIModel)
        .filter(AIModel.is_enabled == True)
        .order_by(AIModel.sort_order.asc())
        .all()
    )
    return models

def load_ai_init_data(db, uid: int):
    threads = load_ai_threads(db, uid)
    models = load_enabled_models(db)

    return threads, models



def load_ai_messages(db, uid: int, ai_thread_id: int):
    # 先校验这个会话是否属于该用户
    thread = (
        db.query(AIThread)
        .filter(
            AIThread.id == ai_thread_id,
            AIThread.uid == uid,
            AIThread.is_deleted == False
        )
        .first()
    )
    if not thread:
        raise Exception("ai_thread not found or no permission")

    messages = (
        db.query(AIMessage)
        .filter(AIMessage.ai_thread_id == ai_thread_id)
        .order_by(AIMessage.id.asc())
        .all()
    )

    return messages