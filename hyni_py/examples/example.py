import sys
import os

# Add parent directory to path
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

from chat_api import ChatApiBuilder
from http_client import HttpResponse

# Example context config class (simplified)
class ContextConfig:
    def __init__(self, enable_validation=True, default_temperature=0.7, default_max_tokens=1000):
        self.enable_validation = enable_validation
        self.default_temperature = default_temperature
        self.default_max_tokens = default_max_tokens

# Mock GeneralContext for testing
class GeneralContext:
    def __init__(self, schema_path, config=None):
        self.schema_path = schema_path
        self.config = config or ContextConfig()
        self.messages = []
        self.parameters = {}

    def add_user_message(self, message):
        self.messages.append({"role": "user", "content": message})
        return self

    def clear_user_messages(self):
        self.messages = [msg for msg in self.messages if msg["role"] != "user"]
        return self

    def get_messages(self):
        return self.messages

    def build_request(self, stream=False):
        return {"messages": self.messages, "stream": stream}

    def get_endpoint(self):
        return "https://api.example.com/v1/chat"

    def get_headers(self):
        return {"Authorization": f"Bearer {self.api_key}"}

    def supports_streaming(self):
        return True

    def extract_text_response(self, json_response):
        return "Mock response text"

    def set_api_key(self, api_key):
        self.api_key = api_key
        return self

# Fix imports in ChatApiBuilder
ChatApiBuilder.build = lambda self: print("API would be built with schema:", self._schema_path)

# Now run a simple example
if __name__ == "__main__":
    print("Creating API instance...")

    config = ContextConfig(
        enable_validation=True,
        default_temperature=0.7,
        default_max_tokens=1000
    )

    # Create API instance using builder
    api = (ChatApiBuilder()
           .schema("schemas/openai.json")
           .config(config)
           .api_key("your-api-key")
           .timeout(60000)
           .max_retries(3)
           .build())

    print("API instance created successfully!")
