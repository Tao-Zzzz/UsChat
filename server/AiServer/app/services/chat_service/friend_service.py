from app.models.models import Friend,User,ChatMessage
from sqlalchemy import or_, and_


def need_friend_context(text: str) -> bool:
    keywords = [
        "我和", "跟", "与", "聊天", "对话",
        "刚刚", "最近", "他说", "她说", "对方说",
        "帮我回", "帮我回复", "代我回复",
        "总结", "概括", "整理", "回顾",
        "他什么意思", "她什么意思", "这句话什么意思",
    ]
    return any(k in text for k in keywords)



def get_user_friends(db, uid):

    rows = db.query(Friend).filter(
        Friend.self_id == uid
    ).all()

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
            "back": back
        })
    return result


def detect_friend_from_text(db, uid, text):
    friends = get_user_friends_with_name(db, uid)

    candidates = []
    for f in friends:
        keys = [f["display_name"], f["back"], f["nick"], f["name"]]
        keys = [k for k in keys if k]
        for k in keys:
            if k and k in text:
                candidates.append((len(k), f))
                break

    if not candidates:
        return None

    candidates.sort(key=lambda x: x[0], reverse=True)
    return candidates[0][1]

def get_recent_chat_with_friend(db, uid, fid, friend_name, limit=10):
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

    lines = []
    for m in msgs:
        role = "我" if m.sender_id == uid else friend_name
        # 这里建议做一下 content 清洗（去掉超长、控制符等）
        lines.append(f"{role}: {m.content}")

    return "\n".join(lines)


def get_recent_chat_with_friend(db, uid, fid, friend_name, limit=10):

    msgs = db.query(ChatMessage).filter(
        or_(
            and_(
                ChatMessage.sender_id == uid,
                ChatMessage.recv_id == fid
            ),
            and_(
                ChatMessage.sender_id == fid,
                ChatMessage.recv_id == uid
            )
        )
    ).order_by(
        ChatMessage.message_id.desc()
    ).limit(limit).all()

    msgs.reverse()

    lines = []

    for m in msgs:
        role = "我" if m.sender_id == uid else friend_name
        lines.append(f"{role}: {m.content}")

    return "\n".join(lines)


