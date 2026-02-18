from openai import OpenAI
from app.config import get_settings

def get_llm_client(provider: str):
    settings = get_settings()

    if provider == "openrouter":
        return OpenAI(
            api_key=settings.OPENROUTER_API_KEY,
            base_url="https://openrouter.ai/api/v1"
        )

    elif provider == "openai":
        return OpenAI(
            api_key=settings.OPENAI_API_KEY
        )

    else:
        raise Exception(f"Unsupported provider: {provider}")