def build_messages(
    model_row,
    system_prompt,
    history_msgs
):

    messages = []

    if model_row.supports_system:

        messages.append({
            "role":"system",
            "content":system_prompt
        })

        for msg in history_msgs:

            messages.append({
                "role":msg.role,
                "content":msg.content
            })

    else:

        if history_msgs:

            first = history_msgs[0]

            messages.append({
                "role":"user",
                "content":
                system_prompt +
                "\n\n" +
                first.content
            })

            for msg in history_msgs[1:]:

                messages.append({
                    "role":msg.role,
                    "content":msg.content
                })

    return messages