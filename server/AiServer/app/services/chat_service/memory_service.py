import json
import re
from app.models.models import SemanticMemory
from openai import OpenAI

# 全局的本地 Ollama 客户端（用于后台极速任务）
local_ollama_client = OpenAI(
    base_url="http://localhost:11434/v1",  # 本地 Ollama 地址
    api_key="ollama"  # 随便填
)

def extract_and_store_memory(db, client, extract_model_key: str, uid: int, recent_msgs: list):
    """
    语义记忆提取器：从最近的对话中提取事实，并存入或更新数据库
    """
    if not recent_msgs:
        return

    # 1. 把对话拼接成文本
    chat_text = "\n".join([f"{m.role}: {m.content}" for m in recent_msgs])

    system_prompt = """
    你是一个信息提取专家。请阅读对话，提取关于用户("自己")或对话中提到的其他人的客观事实（如职业、喜好、人际关系、近期重要事件）。
    如果没有，返回空数组 []。必须严格输出JSON数组。

    【示例】
    对话："jmz快要过生日了，你觉得我送他什么东西比较好？"
    输出：[{"entity": "jmz", "attribute": "近期事件", "fact": "快要过生日了"}]
    
    对话："我下个月要去北京打工了。"
    输出：[{"entity": "自己", "attribute": "所在地", "fact": "即将在北京工作"}]


    必须严格以JSON数组格式输出，例如：
    [
      {"entity": "自己", "attribute": "饮食限制", "fact": "对海鲜过敏，不能吃虾"},
      {"entity": "老王", "attribute": "关系", "fact": "是用户的大学室友，最近借了钱"}
    ]
    """

    try:
        response = local_ollama_client.chat.completions.create(
            model="qwen2.5:1.5b",
            messages=[
                {"role": "system", "content": system_prompt},
                {"role": "user", "content": chat_text}
            ],
            temperature=0.1,
        )
        
        reply = response.choices[0].message.content or "[]"
        
        # 使用正则安全提取 JSON 数组 (应对 Qwen/DeepSeek 输出的 Markdown 格式)
        json_match = re.search(r'\[.*\]', reply, re.DOTALL)
        if json_match:
            reply = json_match.group(0)
            
        facts = json.loads(reply)
        
        # 2. 存入数据库（带去重/更新逻辑）
        for item in facts:
            entity = item.get("entity", "").strip()
            attribute = item.get("attribute", "").strip()
            fact = item.get("fact", "").strip()
            
            if not entity or not attribute or not fact:
                continue
                
            # 查找是否已经存在该实体和属性的记忆
            existing_memory = db.query(SemanticMemory).filter(
                SemanticMemory.uid == uid,
                SemanticMemory.entity == entity,
                SemanticMemory.attribute == attribute
            ).first()

            if existing_memory:
                # 如果事实发生了变化，则更新；比如本来在“北京”，现在说搬到了“上海”
                if existing_memory.fact != fact:
                    existing_memory.fact = fact
            else:
                # 新增记忆
                new_mem = SemanticMemory(
                    uid=uid,
                    entity=entity,
                    attribute=attribute,
                    fact=fact
                )
                db.add(new_mem)
                
        db.commit()
        print(f"[Memory Extractor] Successfully extracted {len(facts)} facts.")

    except Exception as e:
        print(f"[Memory Extractor Error]: {e}")
        db.rollback()