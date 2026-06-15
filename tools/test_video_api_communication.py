import json
import threading
import time
import urllib.request

from mock_videos_server import create_server


def post_json(url, payload):
    # 这是什么：测试脚本里发送 JSON POST 请求的辅助函数。
    # 为什么能实现：urllib.request.Request 可以指定 method、headers 和请求体，mock server 会按 JSON 解析。
    # 什么时候调用：通信测试需要验证 /login 成功和失败场景时调用。
    # 和谁配合：mock_videos_server.py 的 do_POST 处理同一个 /login 接口。
    body = json.dumps(payload).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=3) as response:
        return json.loads(response.read().decode("utf-8"))


def main():
    # 这是什么：一个最小通信测试；为什么这样做：不用启动 Qt 界面也能验证 /videos 能通。
    # 什么时候调用：改 ApiClient 或 mock server 后运行；和谁配合：ApiClient 请求同一个 URL。
    server = create_server(port=0)
    base_url = f"http://127.0.0.1:{server.server_port}"
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    time.sleep(0.2)

    try:
        with urllib.request.urlopen(f"{base_url}/videos", timeout=3) as response:
            body = response.read().decode("utf-8")
            videos = json.loads(body)

        assert isinstance(videos, list), "response should be a JSON array"
        assert len(videos) >= 1, "response should contain at least one video"
        first = videos[0]
        for key in ("id", "title", "userName", "date", "duration", "playCount", "likeCount", "category", "tags"):
            assert key in first, f"missing field: {key}"
        assert isinstance(first["tags"], list), "tags should be a JSON array"

        print("OK: /videos communication test passed")

        with urllib.request.urlopen(f"{base_url}/videos/detail?id=video-001", timeout=3) as response:
            detail_body = response.read().decode("utf-8")
            detail = json.loads(detail_body)

        assert detail["success"] is True, "video detail should succeed with valid id"
        assert detail["video"]["id"] == "video-001", "video detail should return requested id"
        assert detail["video"]["description"], "video detail should return description"

        with urllib.request.urlopen(f"{base_url}/videos/detail?id=missing-video", timeout=3) as response:
            missing_detail_body = response.read().decode("utf-8")
            missing_detail = json.loads(missing_detail_body)

        assert missing_detail["success"] is False, "video detail should fail with missing id"
        assert missing_detail["message"], "missing detail should return message"

        print("OK: /videos/detail communication test passed")

        with urllib.request.urlopen(f"{base_url}/videos/play-url", timeout=3) as response:
            play_url_body = response.read().decode("utf-8")
            play_url = json.loads(play_url_body)

        assert play_url["success"] is True, "play-url should succeed"
        assert play_url["playUrl"], "play-url should return non-empty playUrl"

        print("OK: /videos/play-url communication test passed")

        with urllib.request.urlopen(f"{base_url}/videos/barrages", timeout=3) as response:
            barrage_body = response.read().decode("utf-8")
            barrage_list = json.loads(barrage_body)

        assert barrage_list["success"] is True, "barrages request should succeed"
        assert isinstance(barrage_list["barrages"], list), "barrages should be a JSON array"

        barrage_send_success = post_json(
            f"{base_url}/videos/barrages",
            {
                "videoKey": "D:/video-on-demand-client/test.mp4",
                "seconds": 5,
                "text": "测试发送弹幕",
                "userName": "BIT 用户",
                "account": "bit-user-001",
            },
        )
        assert barrage_send_success["success"] is True, "barrage send should succeed"
        assert barrage_send_success["text"] == "测试发送弹幕", "barrage send should echo text"

        barrage_send_empty = post_json(
            f"{base_url}/videos/barrages",
            {
                "videoKey": "D:/video-on-demand-client/test.mp4",
                "seconds": 5,
                "text": "",
            },
        )
        assert barrage_send_empty["success"] is False, "empty barrage should fail"

        print("OK: /videos/barrages communication test passed")

        login_success = post_json(
            f"{base_url}/login",
            {"account": "bit-user-001", "password": "bit123456"},
        )
        assert login_success["success"] is True, "login should succeed with valid password"
        assert login_success["userName"] == "BIT 用户", "login should return userName"
        assert login_success["account"] == "bit-user-001", "login should return account"

        login_failed = post_json(
            f"{base_url}/login",
            {"account": "bit-user-001", "password": "wrong-password"},
        )
        assert login_failed["success"] is False, "login should fail with invalid password"
        assert login_failed["message"], "failed login should return message"

        print("OK: /login communication test passed")

        upload_success = post_json(
            f"{base_url}/videos",
            {
                "title": "测试上传视频",
                "description": "这是一次上传接口联调",
                "category": "科技",
                "tags": ["编程开发", "软件工具"],
                "userName": "BIT 用户",
                "account": "bit-user-001",
                "videoFileName": "demo.mp4",
                "coverFileName": "cover.png",
            },
        )
        assert upload_success["success"] is True, "upload should succeed with required metadata"

        upload_missing_title = post_json(
            f"{base_url}/videos",
            {
                "title": "",
                "category": "科技",
                "account": "bit-user-001",
                "videoFileName": "demo.mp4",
            },
        )
        assert upload_missing_title["success"] is False, "upload should fail without title"

        upload_missing_account = post_json(
            f"{base_url}/videos",
            {
                "title": "测试上传视频",
                "category": "科技",
                "account": "",
                "videoFileName": "demo.mp4",
            },
        )
        assert upload_missing_account["success"] is False, "upload should fail without account"

        print("OK: /videos upload communication test passed")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
