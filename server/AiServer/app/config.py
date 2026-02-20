from pydantic_settings import BaseSettings
from functools import lru_cache

class Settings(BaseSettings):
    OPENROUTER_API_KEY: str
    OPENAI_API_KEY: str | None = None
    SILLICONFLOW_API_KEY: str
    
    class Config:
        env_file = ".env"
        extra = "ignore"


@lru_cache()
def get_settings():
    return Settings()