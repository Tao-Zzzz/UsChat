from app.models.models import Friend

def get_user_friends(db, uid):

    rows = db.query(Friend).filter(
        Friend.self_id == uid
    ).all()

    return [r.friend_id for r in rows]