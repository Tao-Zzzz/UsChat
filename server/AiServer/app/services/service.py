from sqlalchemy.orm import Session
from app.models.models import AIThread, AIMessage, AIModel
from openai import OpenAI
from app.core.config import get_settings
from app.services.chat_service.message_builder import build_messages
from app.services.chat_service.ai_decision import need_vector_search
from app.vector.vector_context import build_vector_context
from app.vector.vector_store_service import store_chat_vector


settings = get_settings()

def handle_chat(db, req):

    created = False

    # 新会话
    if req.ai_thread_id == -1:

        thread = AIThread(
            uid=req.uid,
            title=req.content[:20] or "新對話"
        )

        db.add(thread)
        db.commit()
        db.refresh(thread)

        created = True
        ai_thread_id = thread.id

    else:

        thread = db.query(AIThread).filter(
            AIThread.id == req.ai_thread_id,
            AIThread.uid == req.uid,
            AIThread.is_deleted == False
        ).first()

        if not thread:
            raise Exception("thread not found")

        ai_thread_id = thread.id

    # 存用户消息
    user_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="user",
        content=req.content,
        model=req.model,
        tokens=estimate_tokens(req.content)
    )

    db.add(user_msg)
    db.commit()
    db.refresh(user_msg)

    # 找模型
    model_row = db.query(AIModel).filter(
        AIModel.name == req.model,
        AIModel.is_enabled == True
    ).first()

    if not model_row:
        raise Exception("model not found")

    client = get_llm_client(model_row.provider)

    # 构建历史
    history_msgs = build_context(
        db,
        thread.id,
        model_row.context_length - 2000
    )

    system_prompt = """
你是一个逻辑严谨的AI助手
用中文回答
"""

    messages = build_messages(
        model_row,
        system_prompt,
        history_msgs
    )

    # ===== VECTOR SEARCH =====
    if need_vector_search(
        client,
        model_row.model_key,
        req.content
    ):

        vector_context = build_vector_context(
            req.uid,
            req.content
        )

        if vector_context:

            messages.insert(0,{
                "role":"system",
                "content":vector_context
            })

    # ===== 调用模型 =====

    response = client.chat.completions.create(
        model=model_row.model_key,
        messages=messages,
        temperature=0.6,
        max_tokens=2048
    )

    reply = response.choices[0].message.content.strip()

    # 保存 AI
    ai_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="assistant",
        content=reply,
        model=req.model,
        tokens=response.usage.total_tokens
    )

    db.add(ai_msg)
    db.commit()
    db.refresh(ai_msg)

    # ===== 存向量 =====
    store_chat_vector(
        db,
        req.uid,
        0,
        ai_msg.id,
        req.content
    )

    return (
        ai_thread_id,
        reply,
        created,
        user_msg.id,
        ai_msg.id,
        ai_msg.created_at,
        thread.title,
        req.unique_id
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
    

#得到历史记录
# def build_messages(model_row, system_prompt, history_msgs):
#     messages = []

#     if model_row.supports_system:

#         messages.append({
#             "role": "system",
#             "content": system_prompt
#         })

#         for msg in history_msgs:
#             messages.append({
#                 "role": msg.role,
#                 "content": msg.content
#             })

#     else:
#         # 不支持system
#         if history_msgs:

#             first = history_msgs[0]

#             messages.append({
#                 "role": "user",
#                 "content":
#                 f"{system_prompt}\n\n"
#                 f"用户问题：\n{first.content}"
#             })

#             for msg in history_msgs[1:]:
#                 messages.append({
#                     "role": msg.role,
#                     "content": msg.content
#                 })

#     return messages

#估计token
def estimate_tokens(text):
    return int(len(text))



#裁剪
def build_context(db, thread_id, max_tokens):

    msgs = (
        db.query(AIMessage)
        .filter(AIMessage.ai_thread_id == thread_id)
        .order_by(AIMessage.id.desc())
        .all()
    )

    total = 0
    selected = []

    for msg in msgs:

        t = msg.tokens or estimate_tokens(msg.content)

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