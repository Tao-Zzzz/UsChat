# schemas.py
from pydantic import BaseModel
from typing import List

class RegisterRequest(BaseModel):
    uid: int
    feature: List[float]  # 接收 C++ 传来的 float 数组

class SearchRequest(BaseModel):
    feature: List[float]