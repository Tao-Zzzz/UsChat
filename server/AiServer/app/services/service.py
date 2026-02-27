from sqlalchemy.orm import Session
from app.models.models import AIThread, AIMessage, AIModel, ChatMessage
from openai import OpenAI
from app.core.config import get_settings
from app.services.chat_service.message_builder import build_messages
from app.services.chat_service.ai_decision import need_vector_search
from app.vector.vector_context import build_vector_context, build_vector_context_v2, build_friend_context_by_vector
from app.vector.vector_store_service import store_chat_vector, store_chat_vector_v2,init_vectors
from app.vector.vector_utils import debug_dump_vectors
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





    
def handle_chat_v2(db, req):
    created = False

    # ========== A) 创建/查找 thread + 保存 user_msg ==========
    if req.ai_thread_id == -1:
        thread = AIThread(uid=req.uid, title=(req.content[:20].strip() or "新對話"))
        db.add(thread)
        db.flush() 
        created = True
        ai_thread_id = thread.id
    else:
        thread = db.query(AIThread).filter(
            AIThread.id == req.ai_thread_id,
            AIThread.uid == req.uid,
            AIThread.is_deleted == False,
        ).first()
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
    db.add(user_msg)
    db.flush()

    # 获取模型配置
    model_row = db.query(AIModel).filter(AIModel.name == req.model, AIModel.is_enabled == True).first()
    if not model_row:
        raise Exception("model not found")

    # ========== B) 历史上下文处理与自动总结（新增插入位置） ==========
    client = get_llm_client(model_row.provider)
    
    # 1. 加载历史消息 (预留 2000 tokens 给系统提示词和当前输入)
    history_msgs = build_context(db, ai_thread_id, model_row.context_length - 2000)

    # 2. 计算当前上下文 Token 总量
    total_tokens = sum(m.tokens or estimate_tokens(m.content) for m in history_msgs)

    # 3. 如果超过阈值（如 70%），触发自动总结
    if total_tokens > model_row.context_length * 0.7:
        try:
            # 这里的 old_msgs 取前 20 条，或者根据需求取更早的消息
            old_msgs = history_msgs[:20] 
            summary = generate_summary(client, model_row.model_key, old_msgs)
            
            thread.summary = summary
            thread.summary_tokens = estimate_tokens(summary)
            db.commit() # 及时提交总结，防止后续环节出错导致总结丢失
            db.refresh(thread)
            
            # 总结后重新构建 history_msgs，此时 build_context 内部应该会优先加载 thread.summary
            history_msgs = build_context(db, ai_thread_id, model_row.context_length - 2000)
        except Exception as e:
            print(f"Summary generation failed: {e}")

    # ========== C) 组装最终 Messages ==========
    system_prompt = "你是一个逻辑严谨的AI助手\n用中文回答,并且回答精炼,不要超过300个字"
    # 注意：确保 build_messages 内部会处理 thread.summary
    messages = build_messages(model_row, system_prompt, history_msgs, summary=thread.summary)

    # debug看看向量数据库
    #init_vectors(db)
    debug_dump_vectors(100)
    # B1 好友上下文召回
    if need_friend_context(req.content):
        friend_info = detect_friend_from_text(db, req.uid, req.content)
        if friend_info:
            friend_ctx = build_friend_context_by_vector(db, req.uid, friend_info, req.content)
            if friend_ctx:
                insert_reference_context(messages, friend_ctx, title="好友相关上下文")


    vc = build_vector_context_v2(req.uid, req.content, friend_id=0, k=4)
    if vc:
        insert_reference_context(messages, vc, title="向量检索结果")

    print(messages)
    # ========== D) 调模型 ==========
    response = client.chat.completions.create(
        model=model_row.model_key,
        messages=messages,
        temperature=0.6,
        max_tokens=2048,
    )

    reply = (response.choices[0].message.content or "").strip()
    usage = getattr(response, "usage", None)
    completion_tokens = getattr(usage, "completion_tokens", None) if usage else None

    # ========== E) 保存 AI 回复 + 向量化 ==========
    ai_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="assistant",
        content=reply,
        model=req.model,
        tokens=completion_tokens or estimate_tokens(reply),
    )
    db.add(ai_msg)
    db.flush()

    # 存储向量（不 commit）
    store_chat_vector_v2(db, req.uid, 0, user_msg.id, req.content, commit=False)
    store_chat_vector_v2(db, req.uid, 0, ai_msg.id, reply, commit=False)

    # 好友特定向量存储
    friend_info = detect_friend_from_text(db, req.uid, req.content)
    if friend_info:
        fid = friend_info["friend_id"]
        store_chat_vector_v2(db, req.uid, fid, user_msg.id, req.content, commit=False)
        store_chat_vector_v2(db, req.uid, fid, ai_msg.id, reply, commit=False)
            
    # 生成标题
    if created:
        try:
            title = generate_title(client, model_row.model_key, req.content, reply)
            thread.title = title[:50]
        except Exception as e:
            print("title generation failed:", e)

    # 最终统一提交
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