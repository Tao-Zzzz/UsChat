from sqlalchemy import or_, and_

from app.models.models import Friend, User, ChatMessage


FRIEND_CONTEXT_KEYWORDS = [
    "我和", "跟", "与", "聊天", "对话",
    "刚刚", "最近", "他说", "她说", "对方说",
    "帮我回", "帮我回复", "代我回复",
    "总结", "概括", "整理", "回顾",
    "他什么意思", "她什么意思", "这句话什么意思",
]


def need_friend_context(text: str) -> bool:
    text = (text or "").strip()
    if not text:
        return False
    return any(k in text for k in FRIEND_CONTEXT_KEYWORDS)


def get_user_friends(db, uid):
    rows = db.query(Friend).filter(Friend.self_id == uid).all()
    return [r.friend_id for r in rows]


def get_user_friends_with_name(db, uid):
    rows = (
        db.query(Friend, User)
        .join(User, Friend.friend_id == User.uid)
        .filter(Friend.self_id == uid)
        .all()
    )

    result = []
    for fr, u in rows:
        back = (fr.back or "").strip()
        display_name = back or (u.nick or u.name or f"用户{u.uid}")
        result.append({
            "friend_id": fr.friend_id,
            "display_name": display_name,
            "nick": u.nick or "",
            "name": u.name or "",
            "back": back,
        })
    return result


def detect_friend_from_text(db, uid, text):
    text = text or ""
    friends = get_user_friends_with_name(db, uid)

    candidates = []
    for f in friends:
        keys = [f["display_name"], f["back"], f["nick"], f["name"]]
        keys = [k for k in keys if k]
        for k in keys:
            if k in text:
                candidates.append((len(k), f))
                break

    if not candidates:
        return None

    candidates.sort(key=lambda x: x[0], reverse=True)
    return candidates[0][1]


def _clean_chat_text(text: str, max_len: int = 200) -> str:
    text = (text or "").replace("\r", " ").replace("\n", " ").strip()
    text = " ".join(text.split())
    if len(text) > max_len:
        return text[:max_len] + "..."
    return text


def get_recent_chat_messages(db, uid, fid, limit=10):
    msgs = (
        db.query(ChatMessage)
        .filter(
            or_(
                and_(ChatMessage.sender_id == uid, ChatMessage.recv_id == fid),
                and_(ChatMessage.sender_id == fid, ChatMessage.recv_id == uid),
            )
        )
        .order_by(ChatMessage.message_id.desc())
        .limit(limit)
        .all()
    )
    msgs.reverse()
    return msgs


def get_recent_chat_with_friend(db, uid, fid, friend_name, limit=10):
    msgs = get_recent_chat_messages(db, uid, fid, limit=limit)

    lines = []
    for m in msgs:
        role = "我" if m.sender_id == uid else friend_name
        lines.append(f"{role}: {_clean_chat_text(m.content)}")

    return "\n".join(lines)


def build_friend_chat_context(db, uid, friend_info, limit=12) -> str:
    """
    构建给 AI 的好友聊天参考上下文。
    当前版本不依赖向量检索，只基于最近聊天，保证稳定和可控。
    """
    fid = friend_info["friend_id"]
    friend_name = friend_info["display_name"]
    msgs = get_recent_chat_messages(db, uid, fid, limit=limit)

    if not msgs:
        return ""

    lines = []
    my_count = 0
    friend_count = 0
    last_speaker = ""

    for m in msgs:
        is_me = m.sender_id == uid
        role = "我" if is_me else friend_name
        if is_me:
            my_count += 1
        else:
            friend_count += 1
        last_speaker = role
        lines.append(f"{role}: {_clean_chat_text(m.content)}")

    header = [
        f"你正在帮助用户处理与好友【{friend_name}】相关的问题。",
        f"以下是双方最近 {len(msgs)} 条聊天记录，请优先基于这些内容理解上下文。",
        f"统计：我发了 {my_count} 条，{friend_name} 发了 {friend_count} 条，最后一条来自：{last_speaker}。",
        "如果下面的聊天不足以支持确定结论，请明确说明不确定，不要编造。",
        "",
        "最近聊天：",
    ]

    return "\n".join(header + lines)