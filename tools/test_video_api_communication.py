import json
import threading
import time
import urllib.request

from mock_videos_server import create_server


def main():
    # 这是什么：一个最小通信测试；为什么这样做：不用启动 Qt 界面也能验证 /videos 能通。
    # 什么时候调用：改 ApiClient 或 mock server 后运行；和谁配合：ApiClient 请求同一个 URL。
    server = create_server(port=8080)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    time.sleep(0.2)

    try:
        with urllib.request.urlopen("http://127.0.0.1:8080/videos", timeout=3) as response:
            body = response.read().decode("utf-8")
            videos = json.loads(body)

        assert isinstance(videos, list), "response should be a JSON array"
        assert len(videos) >= 1, "response should contain at least one video"
        first = videos[0]
        for key in ("title", "userName", "date", "duration", "playCount", "likeCount", "category", "tags"):
            assert key in first, f"missing field: {key}"
        assert isinstance(first["tags"], list), "tags should be a JSON array"

        print("OK: /videos communication test passed")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
