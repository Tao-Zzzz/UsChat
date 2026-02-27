def build_messages(model_row, system_prompt, history_msgs, summary=None):
    def safe_role(r: str) -> str:
        r = (r or "").strip()
        # 确保角色合法，默认为 user
        return r if r in ("user", "assistant", "system") else "user"

    messages = []
    
    # 构造摘要前缀（如果存在）
    summary_content = f"【历史对话总结】：\n{summary}\n\n" if summary else ""

    if model_row.supports_system:
        # 情况 A: 支持 system 角色
        messages.append({"role": "system", "content": system_prompt})
        
        # 如果有总结，单独作为一条 user 消息或拼入第一条（这里建议作为 user 消息告知 AI 背景）
        if summary:
            messages.append({"role": "user", "content": summary_content + "以上是之前的对话概要，请知悉。"})
        
        for msg in history_msgs:
            messages.append({"role": safe_role(msg.role), "content": msg.content or ""})

    else:
        # 情况 B: 不支持 system 角色
        # 这种情况下，我们需要把 system_prompt、summary 和第一条消息全揉在一起
        combined_prefix = system_prompt + "\n\n" + summary_content
        
        if history_msgs:
            first = history_msgs[0]
            first_content = first.content or ""
            messages.append({
                "role": "user", 
                "content": combined_prefix + first_content
            })
            for msg in history_msgs[1:]:
                messages.append({"role": safe_role(msg.role), "content": msg.content or ""})
        else:
            # 如果没有历史消息，就只发前缀
            messages.append({"role": "user", "content": combined_prefix.strip()})

    return messages