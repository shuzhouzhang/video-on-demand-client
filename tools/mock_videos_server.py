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


class MockVideosHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path != "/videos":
            self.send_response(404)
            self.end_headers()
            return

        body = json.dumps(VIDEOS, ensure_ascii=False).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)


def create_server(host="127.0.0.1", port=8080):
    return HTTPServer((host, port), MockVideosHandler)


if __name__ == "__main__":
    server = create_server()
    print("Mock video server running at http://127.0.0.1:8080/videos")
    server.serve_forever()
