from fastapi import FastAPI
import uvicorn
from schemas import RegisterRequest, SearchRequest
from face_service import FaceService
from contextlib import asynccontextmanager

# 1. 先定义 lifespan
@asynccontextmanager
async def lifespan(app: FastAPI):
    # 服务启动时加载 FAISS 索引
    FaceService.init_faiss()
    yield

# 2. 创建唯一的 app 实例，并传入 lifespan
app = FastAPI(title="Face AI Microservice", lifespan=lifespan)

# 3. 挂载路由到这个唯一的 app 上
@app.post("/api/face/register")
async def api_register(req: RegisterRequest):
    success = FaceService.register_face(req.uid, req.feature)
    if success:
        return {"error": 0, "msg": "人脸录入成功"}
    return {"error": 1, "msg": "数据库写入失败"}

@app.post("/api/face/search")
async def api_search(req: SearchRequest):
    result = FaceService.search_face(req.feature)
    return result

if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8010, reload=True)