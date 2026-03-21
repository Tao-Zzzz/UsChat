from sqlalchemy.orm import Session
from openai import OpenAI
import re

from app.core.config import get_settings
from app.models.models import AIThread, AIMessage, AIModel
from app.services.chat_service.chat_service import insert_reference_context
from app.services.chat_service.friend_service import (
    build_friend_chat_context,
    detect_friend_from_text,
    need_friend_context,
)
from app.services.chat_service.message_builder import build_messages

settings = get_settings()


def handle_chat(db, req):
    created = False

    with db.begin():
        if req.ai_thread_id == -1:
            thread = AIThread(uid=req.uid, title=(req.content[:20].strip() or "新對話"))
            db.add(thread)
            db.flush()
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
        db.add(user_msg)
        db.flush()

        model_row = (
            db.query(AIModel)
            .filter(AIModel.name == req.model, AIModel.is_enabled == True)
            .first()
        )
        if not model_row:
            raise Exception("model not found")

    client = get_llm_client(model_row.provider)
    history_msgs = build_context(db, ai_thread_id, model_row.context_length - 2000)

    system_prompt = (
        "你是一个逻辑严谨的AI助手。\n"
        "用中文回答，并且回答精炼。\n"
        "如果用户在问某个好友相关的问题，请优先参考提供的好友最近聊天记录来回答；"
        "如果参考内容不足以支撑结论，要明确说明不确定。"
    )
    messages = build_messages(model_row, system_prompt, history_msgs)

    if need_friend_context(req.content):
        friend_info = detect_friend_from_text(db, req.uid, req.content)
        if friend_info:
            friend_ctx = build_friend_chat_context(db, req.uid, friend_info, limit=12)
            if friend_ctx:
                insert_reference_context(messages, friend_ctx, title="好友最近聊天")

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

    with db.begin():
        ai_msg = AIMessage(
            ai_thread_id=ai_thread_id,
            role="assistant",
            content=reply,
            model=req.model,
            tokens=completion_tokens or total_tokens or estimate_tokens(reply),
        )
        db.add(ai_msg)
        db.flush()

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

    if req.ai_thread_id == -1:
        thread = AIThread(uid=req.uid, title=(req.content[:20].strip() or "新對話"))
        db.add(thread)
        db.flush()
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
    db.add(user_msg)
    db.flush()

    model_row = (
        db.query(AIModel)
        .filter(AIModel.name == req.model, AIModel.is_enabled == True)
        .first()
    )
    if not model_row:
        raise Exception("model not found")

    client = get_llm_client(model_row.provider)
    history_msgs = build_context(db, ai_thread_id, model_row.context_length - 2000)
    total_tokens = sum(m.tokens or estimate_tokens(m.content) for m in history_msgs)

    # 上下文过长时，先对历史消息做摘要，避免越聊越长
    if total_tokens > model_row.context_length * 0.7:
        try:
            old_msgs = history_msgs[:20]
            summary = generate_summary(
                client,
                model_row.model_key,
                old_msgs,
                thread.summary or "",
            )
            thread.summary = summary
            thread.summary_tokens = estimate_tokens(summary)
            db.commit()
            db.refresh(thread)
            history_msgs = build_context(db, ai_thread_id, model_row.context_length - 2000)
        except Exception as e:
            print(f"Summary generation failed: {e}")
            db.rollback()

    system_prompt = (
        "你是一个逻辑严谨的AI助手。\n"
        "用中文回答，并且回答精炼，不要超过300个字。\n"
        "如果用户在问某个好友相关的问题，请优先参考提供的好友最近聊天记录来回答；\n"
        "如果参考内容不足以支撑结论，要明确说明不确定，不要编造。\n"
        "如果用户是在让你帮忙回复某个好友，请结合最近聊天语气，给出自然、贴合上下文的回复建议。"
    )

    messages = build_messages(
        model_row,
        system_prompt,
        history_msgs,
        summary=thread.summary,
    )

    # 主链路：好友识别 + 最近聊天记录
    if need_friend_context(req.content):
        friend_info = detect_friend_from_text(db, req.uid, req.content)
        if friend_info:
            friend_ctx = build_friend_chat_context(db, req.uid, friend_info, limit=12)
            if friend_ctx:
                insert_reference_context(messages, friend_ctx, title="好友最近聊天")

    response = client.chat.completions.create(
        model=model_row.model_key,
        messages=messages,
        temperature=0.6,
        max_tokens=2048,
    )

    reply = (response.choices[0].message.content or "").strip()
    usage = getattr(response, "usage", None)
    completion_tokens = getattr(usage, "completion_tokens", None) if usage else None

    ai_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="assistant",
        content=reply,
        model=req.model,
        tokens=completion_tokens or estimate_tokens(reply),
    )
    db.add(ai_msg)
    db.flush()

    if created:
        try:
            title = generate_title(client, model_row.model_key, req.content, reply)
            thread.title = title[:50]
        except Exception as e:
            print("title generation failed:", e)

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
            base_url="https://openrouter.ai/api/v1",
        )
    elif provider == "openai":
        return OpenAI(api_key=settings.OPENAI_KEY)
    elif provider == "deepseek":
        return OpenAI(
            api_key=settings.DEEPSEEK_KEY,
            base_url="https://api.deepseek.com/v1",
        )
    elif provider == "siliconflow":
        return OpenAI(
            api_key=settings.SILLICONFLOW_API_KEY,
            base_url="https://api.siliconflow.cn/v1",
        )
    else:
        raise Exception(f"unsupported provider: {provider}")


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

请将“已有总结”和“新增对话”合并成一份更完整但简洁的对话总结。
要求：
1. 使用中文
2. 保留关键信息、上下文和用户偏好
3. 不要超过300字
"""

    response = client.chat.completions.create(
        model=model_key,
        messages=[{"role": "user", "content": prompt}],
        temperature=0.3,
    )

    return (response.choices[0].message.content or "").strip()


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
        messages=[{"role": "user", "content": prompt}],
        temperature=0.3,
        max_tokens=30,
    )

    title = (response.choices[0].message.content or "").strip()
    return title


def load_ai_threads(db, uid: int):
    threads = (
        db.query(AIThread)
        .filter(
            AIThread.uid == uid,
            AIThread.is_deleted == False,
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
    thread = (
        db.query(AIThread)
        .filter(
            AIThread.id == ai_thread_id,
            AIThread.uid == uid,
            AIThread.is_deleted == False,
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