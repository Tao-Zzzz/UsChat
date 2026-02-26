def build_messages(model_row, system_prompt, history_msgs):
    def safe_role(r: str) -> str:
        r = (r or "").strip()
        return r if r in ("user", "assistant", "system") else "user"

    messages = []

    if model_row.supports_system:
        messages.append({"role": "system", "content": system_prompt})

        for msg in history_msgs:
            messages.append({"role": safe_role(msg.role), "content": msg.content or ""})

    else:
        # 不支持 system：把 system_prompt 放在第一条 user 里
        if history_msgs:
            first = history_msgs[0]
            first_content = first.content or ""
            messages.append({
                "role": "user",
                "content": system_prompt + "\n\n" + first_content
            })

            for msg in history_msgs[1:]:
                messages.append({"role": safe_role(msg.role), "content": msg.content or ""})
        else:
            messages.append({"role": "user", "content": system_prompt})

    return messages
