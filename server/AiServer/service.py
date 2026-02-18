from sqlalchemy.orm import Session
from models import AIThread, AIMessage, AIModel
from openai import OpenAI


# 初始化客户端（只改这里两行）
client = OpenAI(
    api_key="sk-wwncxncooxrgvevddiziufxlhqoygbsetsbawdhyyfwlcjwp",          # 替换成 sk-xxx...
    base_url="https://api.siliconflow.cn/v1"     # 固定这个
)



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
        model=req.model
    )
    db.add(user_msg)
    db.commit()
    db.refresh(user_msg)


    messages = [
        # 系统提示（可选，加这个让模型更像助手）
        {"role": "system", "content": "你是一个逻辑严谨、聪明有帮助的 AI 助手。用中文回复，思考过程清晰。"},
        
        # 历史消息（你可以从数据库拉 AIMessage 记录，按时间顺序加进来）
        # 比如前面的用户消息
        {"role": "user", "content": req.content},  # 当前用户输入
        # 如果有更多历史，就继续加 {"role": "assistant", "content": 之前的AI回复}, {"role": "user", ...}
    ]

        # 调用 API 生成回复
    response = client.chat.completions.create(
        model="deepseek-ai/DeepSeek-R1-0528-Qwen3-8B",          # 推荐这个，性价比高，效果好（或换成你喜欢的）
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

    # 拿到 AI 回复文本
    reply_text = response.choices[0].message.content.strip()
    # 3. mock AI 回复
    # reply_text = f"【mock-{req.model}】你刚才说的是：{req.content}"

    # 4. 保存 AI 消息
    ai_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="assistant",
        content=reply_text,
        model=req.model
        # tokens 可以在这里填，如果后面有统计的话
    )
    db.add(ai_msg)
    db.commit()
    db.refresh(ai_msg)           # 非常重要！确保拿到自增的 id 和默认值 created_at

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