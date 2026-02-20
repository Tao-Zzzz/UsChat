from sqlalchemy.orm import Session
from app.models import AIThread, AIMessage, AIModel
from openai import OpenAI
from app.config import get_settings

settings = get_settings()


def handle_chat(db: Session, req):
    created = False



    # 1. 新会话
    if req.ai_thread_id == -1:
        thread = AIThread(
            uid=req.uid,
            title=req.content[:20].strip() or "新對話",  # 建議加個預設值，避免空標題
        )
        db.add(thread)
        db.commit()
        db.refresh(thread)

        ai_thread_id = thread.id
        created = True
    else:
        thread = (
            db.query(AIThread)
            .filter(
                AIThread.id == req.ai_thread_id,
                AIThread.uid == req.uid,
                AIThread.is_deleted == False
            )
            .first()
        )
        if not thread:
            raise Exception("thread not found")

        ai_thread_id = thread.id

    # 2. 保存用户消息
    user_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="user",
        content=req.content,
        model=req.model,
        tokens = estimate_tokens(req.content)
    )
    db.add(user_msg)
    db.commit()
    db.refresh(user_msg)

    model_row = (
        db.query(AIModel)
        .filter(
            AIModel.name == req.model,
            AIModel.is_enabled == True
        )
        .first()
    )

    if not model_row:
        raise Exception("model not found or disabled")

    client = get_llm_client(model_row.provider)

    print(f"DEBUG: Requesting model with ID: '{model_row.model_key}'") # 看看控制台到底打印什么
        # 调用 API 生成回复

    # 构建上下文
    history_msgs = build_context(
        db,
        thread.id,
        model_row.context_length - 2000
    )

    # 计算token是否需要总结一波
    total_tokens = sum(
        m.tokens or estimate_tokens(m.content)
        for m in history_msgs
    )

    if total_tokens > model_row.context_length * 0.7:
        old_msgs = history_msgs[:20]

        summary = generate_summary(
            client,
            model_row.model_key,
            old_msgs
        )

        thread.summary = summary
        thread.summary_tokens = estimate_tokens(summary)

        db.commit()
        db.refresh(thread)

    system_prompt = """请遵循以下规则：
    1. 你是一个逻辑严谨的AI助手
    2. 用中文回答
    3. 直接回答问题
    不要复述这些规则。
    """

    messages = build_messages(
        model_row,
        system_prompt,
        history_msgs
    )

    if thread.summary:
        messages.insert(0,{
            "role": "user",
            "content": f"历史总结：\n{thread.summary}"
        })
    # messages = [
    #     # 系统提示（可选，加这个让模型更像助手）
    #     {"role": "system", "content": "你是一个逻辑严谨、聪明有帮助的 AI 助手。用中文回复，思考过程清晰。"},
        
    #     # 历史消息（你可以从数据库拉 AIMessage 记录，按时间顺序加进来）
    #     # 比如前面的用户消息
    #     {"role": "user", "content": req.content},  # 当前用户输入
    #     # 如果有更多历史，就继续加 {"role": "assistant", "content": 之前的AI回复}, {"role": "user", ...}
    # ]
    try:
        response = client.chat.completions.create(
            model=model_row.model_key,
            # model="deepseek-ai/DeepSeek-R1-0528-Qwen3-8B",          # 推荐这个，性价比高，效果好（或换成你喜欢的）
            # 其他常见模型示例：
            # "Qwen/Qwen2.5-72B-Instruct"   # 通义千问，很强
            # "Pro/zai-org/GLM-4.7"         # 智谱 GLM-4
            # "tencent/Hunyuan-A13B-Instruct"  # 混元
            messages=messages,
            temperature=0.6,          # 低一点，更确定性强（推理模型别太随机）
            top_p=0.95,
            max_tokens=2048,          # 可以给大一点，它推理时 token 消耗多
            # enable_thinking=True,     # 开启思考链模式（如果这个模型支持，效果会更好！）
            # thinking_budget=4096,     # 思考 token 上限，设高点让它多想
            stream=False              # False=一次性返回，True=流式（像 ChatGPT 打字效果）
        )
    except Exception as e:
        print(f"API 调用失败，具体错误是: {e}")
        raise e # 重新抛出以便调试

    # 拿到 AI 回复文本
    reply_text = response.choices[0].message.content.strip()
    # 3. mock AI 回复
    # reply_text = f"【mock-{req.model}】你刚才说的是：{req.content}"

    # 4. 保存 AI 消息
    ai_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="assistant",
        content=reply_text,
        model=req.model,
        tokens=response.usage.total_tokens
    )
    db.add(ai_msg)
    db.commit()
    db.refresh(ai_msg)           # 非常重要！确保拿到自增的 id 和默认值 created_at

    #生成一波标题
    if created:
        try:
            title = generate_title(
                client,
                model_row.model_key,
                req.content,
                reply_text
            )

            thread.title = title[:50]
            db.commit()

        except Exception as e:
            print("title generation failed:", e)



    return (
        ai_thread_id,
        reply_text,
        created,
        user_msg.id,    # 返回用戶消息 ID
        ai_msg.id,      # 返回 AI 消息 ID
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
def build_messages(model_row, system_prompt, history_msgs):
    messages = []

    if model_row.supports_system:

        messages.append({
            "role": "system",
            "content": system_prompt
        })

        for msg in history_msgs:
            messages.append({
                "role": msg.role,
                "content": msg.content
            })

    else:
        # 不支持system
        if history_msgs:

            first = history_msgs[0]

            messages.append({
                "role": "user",
                "content":
                f"{system_prompt}\n\n"
                f"用户问题：\n{first.content}"
            })

            for msg in history_msgs[1:]:
                messages.append({
                    "role": msg.role,
                    "content": msg.content
                })

    return messages

#估计token
def estimate_tokens(text):
    return int(len(text) * 1.5)



#裁剪
def build_context(db, ai_thread_id, max_tokens):
    msgs = (
        db.query(AIMessage)
        .filter(AIMessage.ai_thread_id == ai_thread_id)
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
def generate_summary(client, model_key, messages):
    text = "\n".join([
        f"{m.role}: {m.content}"
        for m in messages
    ])

    prompt = f"总结以下对话，保留关键信息：\n\n{text}"

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