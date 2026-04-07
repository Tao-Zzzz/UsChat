import uvicorn
from app.main import app

if __name__ == "__main__":
    # 注意：千万不要在这里加 reload=True
    uvicorn.run(app, host="0.0.0.0", port=8000)