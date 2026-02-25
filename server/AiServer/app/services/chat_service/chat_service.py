from app.models.models import ChatMessage
from sqlalchemy import or_, and_

def get_recent_chat_with_friend(
    db,
    uid,
    fid,
    limit=10
):

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

        role = "我" if m.sender_id == uid else "好友"

        lines.append(f"{role}: {m.content}")

    return "\n".join(lines)