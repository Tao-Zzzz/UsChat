from sentence_transformers import SentenceTransformer

model = SentenceTransformer("paraphrase-multilingual-MiniLM-L12-v2")
model.save(r"D:\Code\Project\Cpp_\uschat_\server\AiServer\app\embedding_model\paraphrase-multilingual-MiniLM-L12-v2")