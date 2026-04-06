# main.py
from fastapi import FastAPI
import uvicorn
from schemas import RegisterRequest, SearchRequest
from face_service import FaceService

app = FastAPI(title="Face AI Microservice")

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
    # 启动命令: python main.py
    uvicorn.run("main:app", host="0.0.0.0", port=8010, reload=True)