def need_vector_search(client, model, text):

    prompt = f"""
判断用户问题是否需要查询历史聊天记录。

用户问题:
{text}

如果涉及:
过去
之前
答应
发生
聊天
关系
谁
做过

回答 YES 或 NO
"""

    resp = client.chat.completions.create(
        model=model,
        messages=[{
            "role":"user",
            "content":prompt
        }],
        temperature=0
    )

    ans = resp.choices[0].message.content.upper()

    return "YES" in ans