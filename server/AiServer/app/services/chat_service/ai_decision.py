def need_vector_search(client, model, text: str) -> bool:
    if not text or not text.strip():
        return False

    text = text.strip()

    # 1) 先用规则快速判断（命中就不调用模型，省钱省延迟）
    rule_keywords = [
        "过去", "之前", "以前", "上次", "刚才", "昨天", "前天",
        "答应", "承诺", "说过", "提过", "发生", "聊过",
        "关系", "谁", "做过", "还记得", "记得吗", "你记得"
    ]
    if any(k in text for k in rule_keywords):
        return True

    # 2) 再用模型兜底（可选）
    prompt = f"""
你只回答 YES 或 NO。
判断用户问题是否需要查询历史聊天记录/历史对话上下文来回答。

用户问题：
{text}

规则：
- 涉及过去发生的事、之前说过的话、承诺/答应、上次聊天内容、人物关系、谁做过什么 -> YES
- 纯知识问答、当前提问不依赖历史 -> NO
"""

    resp = client.chat.completions.create(
        model=model,
        messages=[{"role": "user", "content": prompt}],
        temperature=0,
        max_tokens=5,
    )

    ans = (resp.choices[0].message.content or "").strip().upper()
    return ans.startswith("YES")
