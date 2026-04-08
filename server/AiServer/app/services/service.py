from app.models.models import AIThread, AIMessage, AIModel, SemanticMemory
from app.services.chat_service.friend_service import (
    build_friend_chat_context,
    detect_friend_from_text,
    need_friend_context,
    detect_friend_by_entity,
)
from app.services.chat_service.chat_service import (
    build_messages,
    insert_reference_context,
    estimate_tokens,
    build_context,
    generate_summary,
    generate_title,
    get_llm_client,
    analyze_user_intent
)
from app.services.chat_service.memory_service import (
    extract_and_store_memory
)
from app.services.chat_service.vector_utils import (
    search_semantic_in_mysql,
)

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

# 引入相关的模块依赖，保留原有的导入...

def handle_chat_v2(db, req):
    created = False

    # 1. 寻找或创建 Thread
    if req.ai_thread_id == -1:
        thread = AIThread(uid=req.uid, title=(req.content[:20].strip() or "新对话"))
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

    # 2. 插入用户消息
    user_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="user",
        content=req.content,
        model=req.model,
        tokens=estimate_tokens(req.content),
        is_summarized=False # 默认False
    )
    db.add(user_msg)
    db.flush()

    # 3. 获取模型配置
    model_row = (
        db.query(AIModel)
        .filter(AIModel.name == req.model, AIModel.is_enabled == True)
        .first()
    )
    if not model_row:
        raise Exception("model not found")

    client = get_llm_client(model_row.provider)
    
    # 4. 获取上下文（此时查出来的都是 is_summarized == False 的消息）
    history_msgs = build_context(db, ai_thread_id, model_row.context_length - 2000)
    total_tokens = sum(m.tokens or estimate_tokens(m.content) for m in history_msgs)

    # 5. 【关键修复区】：上下文过长时的摘要处理
    if total_tokens > model_row.context_length * 0.7:
        try:
            # 取最老的 20 条消息进行摘要（注意不要把刚插进去的 user_msg 算进去，通常它在列表末尾）
            old_msgs = history_msgs[:20] 
            
            # 只有当确实有老消息时才做摘要
            if old_msgs:
                summary = generate_summary(
                    client,
                    model_row.model_key,
                    old_msgs,
                    thread.summary or "",
                )
                
                # 更新 Thread 的摘要字段
                thread.summary = summary
                thread.summary_tokens = estimate_tokens(summary)
                
                # 【关键修复】：将这批老消息标记为已摘要
                for m in old_msgs:
                    m.is_summarized = True
                
                db.flush() # 刷入数据库，保证接下来的重查能够过滤掉它们
                
                # 重新拉取一次上下文，此时由于标记了 is_summarized，那些老消息就不会再出现了
                history_msgs = build_context(db, ai_thread_id, model_row.context_length - 2000)
                
        except Exception as e:
            # 【关键修复】：遇到网络请求或摘要错误，千万不要 db.rollback()！
            # 否则会把上面 db.add(user_msg) 给撤销掉，导致数据库只剩AI回复没有用户提问。
            # 我们只需要打印错误，让这一次带着偏长的上下文硬扛过去即可。
            print(f"[Warning] Summary generation failed, skipping for this turn: {e}")

   # ================= 1. 意图路由 (必须提前！) =================
    # 使用你本地的 Qwen 模型作为路由大脑
    routing_model_key = "qwen2.5:1.5b" # 或者 model_row.model_key
    intent_data = analyze_user_intent(client, routing_model_key, req.content)
    print(f"[Intent Router] Parsed Intent: {intent_data}")

    # ================= 2. 提取该用户的专属语义记忆 =================
    target_entities = ["自己"]
    mentioned_entity = intent_data.get("mentioned_entity")
    if mentioned_entity:
        target_entities.append(mentioned_entity)
        
    user_memories = (
        db.query(SemanticMemory)
        .filter(SemanticMemory.uid == req.uid, SemanticMemory.entity.in_(target_entities))
        .all()
    )
    
    memory_text = ""
    if user_memories:
        memory_lines = [f"- {m.entity}的{m.attribute}: {m.fact}" for m in user_memories]
        memory_text = "\n【已知事实/记忆】\n" + "\n".join(memory_lines) + "\n"

    # ================= 3. 构造系统提示与消息体 =================
    system_prompt = (
        "你是一个逻辑严谨的AI助手，也是用户的数字大脑。\n"
        "用中文回答，并且回答精炼，不要超过300个字。\n"
        f"{memory_text}"
        "如果用户在问某个好友相关的问题，请优先参考提供的好友最近聊天记录来回答；\n"
        "如果参考内容不足以支撑结论，要明确说明不确定，不要编造。"
    )

    messages = build_messages(
        model_row,
        system_prompt,
        history_msgs,
        summary=thread.summary,
    )


    # ================= 4. 注入好友历史记录 (情景记忆 + 语义记忆) =================
    if intent_data.get("intent") == "friend_query" and mentioned_entity:
        friend_info = detect_friend_by_entity(db, req.uid, mentioned_entity)
        if friend_info:
            # A. 原有的：获取最近 10 条（保证即时感）
            recent_ctx = build_friend_chat_context(db, req.uid, friend_info, limit=10)
            
            # B. 新增的：从 MySQL 向量表中搜索最相关的 5 条（翻旧账）
            # 调用我们刚才写的 search_semantic_in_mysql
            related_msgs = search_semantic_in_mysql(
                db, req.uid, friend_info['friend_id'], req.content, limit=5
            )
            
            semantic_ctx = ""
            if related_msgs:
                semantic_ctx = "\n【搜寻到的相关历史片段】\n" + "\n".join([f"- {m}" for m in related_msgs])

            # C. 组合注入
            final_ctx = recent_ctx + "\n" + semantic_ctx
            if final_ctx.strip():
                insert_reference_context(messages, final_ctx, title=f"关于{mentioned_entity}的记忆参考")

    # ================= 5. 请求主模型 =================
    response = client.chat.completions.create(
        model=model_row.model_key,
        messages=messages,
        temperature=0.6,
        max_tokens=2048,
    )

    reply = (response.choices[0].message.content or "").strip()
    usage = getattr(response, "usage", None)
    completion_tokens = getattr(usage, "completion_tokens", None) if usage else None

    # 9. 插入 AI 的回复
    ai_msg = AIMessage(
        ai_thread_id=ai_thread_id,
        role="assistant",
        content=reply,
        model=req.model,
        tokens=completion_tokens or estimate_tokens(reply),
        is_summarized=False # 新消息默认未摘要
    )
    db.add(ai_msg)
    db.flush()

    # 10. 异步/附加操作：新对话生成标题
    if created:
        try:
            title = generate_title(client, model_row.model_key, req.content, reply)
            thread.title = title[:50]
        except Exception as e:
            print("title generation failed:", e)

    # 最后统一提交事务
    db.commit()

    # ================= 新增：异步触发记忆提取 =================
    # 为了避免每次聊天都提取浪费算力，我们可以设置一个小策略：
    # 例如：当前对话有最新的两回合（也就是 history_msgs 最后几条 + 当前 user_msg + 当前 ai_msg）
    # 我们把刚刚产生的这些消息扔给记忆提取器
    try:
        import threading
        
        # 把刚才新产生的消息单独提取出来（一问一答）
        recent_exchange = [user_msg, ai_msg] 
        
        # 注意：这里需要重新开一个数据库 session 或者传当前内容过去，
        # 为了避免线程间的数据库 session 冲突，最安全的做法是提取出纯文本字典。
        extraction_data = [{"role": m.role, "content": m.content} for m in recent_exchange]
        
        # 使用你本地的 Qwen 模型作为提取大脑
        extract_model_key = "qwen2.5:1.5b" # 替换为你的本地模型名
        
        def run_extraction_task(uid, msg_data):
            # 在新线程里新建一个临时的 DB Session
            from app.core.db import SessionLocal # 替换为你实际获取 session 的方式
            with SessionLocal() as background_db:
                # 转换回对象结构以便 extract_and_store_memory 使用
                class DummyMsg:
                    def __init__(self, r, c):
                        self.role, self.content = r, c
                msgs = [DummyMsg(m["role"], m["content"]) for m in msg_data]
                
                extract_and_store_memory(background_db, client, extract_model_key, uid, msgs)

        # 启动后台线程执行，不会阻塞当前 API 返回
        t = threading.Thread(target=run_extraction_task, args=(req.uid, extraction_data))
        t.start()
        
    except Exception as e:
        print(f"[Async Memory Task Failed]: {e}")
    # ==========================================================

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