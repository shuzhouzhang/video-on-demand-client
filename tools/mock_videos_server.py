from http.server import BaseHTTPRequestHandler, HTTPServer
import json


VIDEOS = [
    {
        "title": "Mock 接口返回的视频",
        "userName": "Mock 用户",
        "date": "6-9",
        "duration": "09:18",
        "playCount": "3.6万",
        "likeCount": "256",
        "category": "科技",
        "tags": ["编程开发", "软件工具"],
    },
    {
        "title": "Mock 美食探店视频",
        "userName": "接口测试员",
        "date": "6-9",
        "duration": "12:08",
        "playCount": "1.8万",
        "likeCount": "88",
        "category": "美食",
        "tags": ["美食测评", "探店"],
    },
]

USERS = {
    "bit-user-001": {
        "password": "bit123456",
        "userName": "BIT 用户",
    },
}


class MockVideosHandler(BaseHTTPRequestHandler):
    def write_json(self, status, payload):
        # 这是什么：把 Python 数据统一写成 JSON HTTP 响应。
        # 为什么能实现：json.dumps 负责序列化，send_header 设置响应格式，wfile.write 发给客户端。
        # 什么时候调用：/videos、/login 或错误响应需要返回 JSON 时调用。
        # 和谁配合：Qt ApiClient 和 Python 通信测试都会按 application/json 读取响应体。
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path != "/videos":
            self.send_response(404)
            self.end_headers()
            return

        self.write_json(200, VIDEOS)

    def do_POST(self):
        # 这是什么：处理临时登录接口 POST /login。
        # 为什么能实现：从请求体读取 account/password，和 USERS 里的临时账号做匹配。
        # 什么时候调用：Qt 登录页或通信测试向 mock server 发送登录请求时调用。
        # 和谁配合：ApiClient::login() 发送 JSON，Login 根据 success/message 展示登录结果。
        if self.path != "/login":
            self.send_response(404)
            self.end_headers()
            return

        length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(length).decode("utf-8")
        try:
            payload = json.loads(raw_body) if raw_body else {}
        except json.JSONDecodeError:
            self.write_json(200, {"success": False, "message": "请求格式错误"})
            return

        account = str(payload.get("account", "")).strip()
        password = str(payload.get("password", ""))
        user = USERS.get(account)
        if user is None or user["password"] != password:
            self.write_json(200, {"success": False, "message": "账号或密码错误"})
            return

        self.write_json(200, {"success": True, "userName": user["userName"], "account": account})


def create_server(host="127.0.0.1", port=8080):
    return HTTPServer((host, port), MockVideosHandler)


if __name__ == "__main__":
    server = create_server()
    print("Mock server running at http://127.0.0.1:8080/videos and POST http://127.0.0.1:8080/login")
    server.serve_forever()
