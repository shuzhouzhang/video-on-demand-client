import json
from pathlib import Path
import threading
import time
import urllib.request
import uuid

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


def post_multipart(url, metadata, video_content, cover_content=b""):
    # 这是什么：构造通信测试使用的最小 multipart/form-data 请求。
    # 为什么能实现：boundary 分隔 JSON、视频和封面部分，格式与 QHttpMultiPart 发出的请求一致。
    # 什么时候调用：验证 POST /videos/upload 是否真的接收二进制文件时调用。
    # 和谁配合：mock server 的 MIME 解析器拆分字段并保存文件。
    boundary = f"----codex-test-{uuid.uuid4().hex}"
    chunks = []

    def add_part(headers, content):
        chunks.append(f"--{boundary}\r\n".encode("utf-8"))
        for header in headers:
            chunks.append(f"{header}\r\n".encode("utf-8"))
        chunks.append(b"\r\n")
        chunks.append(content)
        chunks.append(b"\r\n")

    add_part(
        ["Content-Disposition: form-data; name=\"metadata\"", "Content-Type: application/json"],
        json.dumps(metadata, ensure_ascii=False).encode("utf-8"),
    )
    add_part(
        ["Content-Disposition: form-data; name=\"videoFile\"; filename=\"sample.mp4\""],
        video_content,
    )
    if cover_content:
        add_part(
            ["Content-Disposition: form-data; name=\"coverFile\"; filename=\"cover.png\""],
            cover_content,
        )
    chunks.append(f"--{boundary}--\r\n".encode("utf-8"))
    body = b"".join(chunks)
    request = urllib.request.Request(
        url,
        data=body,
        headers={"Content-Type": f"multipart/form-data; boundary={boundary}"},
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

        like_success = post_json(
            f"{base_url}/videos/like",
            {"videoId": "video-001", "account": "bit-user-001"},
        )
        assert like_success["success"] is True, "like should succeed with valid videoId"
        assert like_success["liked"] is True, "like should mark video as liked"
        assert like_success["likeCount"] == "257", "like should increase like count once"

        like_repeat = post_json(
            f"{base_url}/videos/like",
            {"videoId": "video-001", "account": "bit-user-001"},
        )
        assert like_repeat["success"] is True, "repeat like should still succeed"
        assert like_repeat["liked"] is True, "repeat like should keep liked state"
        assert like_repeat["likeCount"] == "257", "repeat like should not increase count again"

        unlike_success = post_json(
            f"{base_url}/videos/unlike",
            {"videoId": "video-001", "account": "bit-user-001"},
        )
        assert unlike_success["success"] is True, "unlike should succeed after like"
        assert unlike_success["liked"] is False, "unlike should mark video as not liked"
        assert unlike_success["likeCount"] == "256", "unlike should decrease like count once"

        like_missing_video_id = post_json(
            f"{base_url}/videos/like",
            {"videoId": "", "account": "bit-user-001"},
        )
        assert like_missing_video_id["success"] is False, "like should fail without videoId"

        print("OK: /videos like communication test passed")

        with urllib.request.urlopen(
            f"{base_url}/videos/watch-progress?videoId=video-001&account=bit-user-001",
            timeout=3,
        ) as response:
            first_progress_body = response.read().decode("utf-8")
            first_progress = json.loads(first_progress_body)

        assert first_progress["success"] is True, "watch progress should load"
        assert first_progress["seconds"] == 0, "first watch progress should default to zero"

        save_progress = post_json(
            f"{base_url}/videos/watch-progress",
            {"videoId": "video-001", "account": "bit-user-001", "seconds": 12},
        )
        assert save_progress["success"] is True, "watch progress should save"
        assert save_progress["seconds"] == 12, "watch progress should echo saved seconds"

        with urllib.request.urlopen(
            f"{base_url}/videos/watch-progress?videoId=video-001&account=bit-user-001",
            timeout=3,
        ) as response:
            saved_progress_body = response.read().decode("utf-8")
            saved_progress = json.loads(saved_progress_body)

        assert saved_progress["success"] is True, "saved watch progress should load"
        assert saved_progress["seconds"] == 12, "saved watch progress should be returned"

        missing_progress = post_json(
            f"{base_url}/videos/watch-progress",
            {"videoId": "", "account": "bit-user-001", "seconds": 12},
        )
        assert missing_progress["success"] is False, "watch progress should fail without videoId"

        print("OK: /videos/watch-progress communication test passed")

        with urllib.request.urlopen(
            f"{base_url}/videos/comments?videoId=video-001",
            timeout=3,
        ) as response:
            comments_body = response.read().decode("utf-8")
            comments = json.loads(comments_body)

        assert comments["success"] is True, "comments should load with valid videoId"
        assert isinstance(comments["comments"], list), "comments should be a JSON array"
        assert len(comments["comments"]) >= 1, "comments should include seeded test data"

        sent_comment = post_json(
            f"{base_url}/videos/comments",
            {
                "videoId": "video-001",
                "userName": "BIT 用户",
                "account": "bit-user-001",
                "content": "这是一条通信测试评论",
            },
        )
        assert sent_comment["success"] is True, "logged-in user should send comment"
        assert sent_comment["comment"]["content"] == "这是一条通信测试评论", "comment should echo content"
        assert sent_comment["comment"]["videoId"] == "video-001", "comment should belong to requested video"

        with urllib.request.urlopen(
            f"{base_url}/videos/comments?videoId=video-001",
            timeout=3,
        ) as response:
            refreshed_comments = json.loads(response.read().decode("utf-8"))

        assert refreshed_comments["comments"][0]["id"] == sent_comment["comment"]["id"], "new comment should be first"

        empty_comment = post_json(
            f"{base_url}/videos/comments",
            {
                "videoId": "video-001",
                "userName": "BIT 用户",
                "account": "bit-user-001",
                "content": "",
            },
        )
        assert empty_comment["success"] is False, "empty comment should fail"

        guest_comment = post_json(
            f"{base_url}/videos/comments",
            {
                "videoId": "video-001",
                "userName": "",
                "account": "",
                "content": "游客不应该能发表评论",
            },
        )
        assert guest_comment["success"] is False, "guest comment should require login"

        missing_comment_video = post_json(
            f"{base_url}/videos/comments",
            {
                "videoId": "",
                "userName": "BIT 用户",
                "account": "bit-user-001",
                "content": "缺少视频 id",
            },
        )
        assert missing_comment_video["success"] is False, "comment should fail without videoId"

        with urllib.request.urlopen(f"{base_url}/videos/comments", timeout=3) as response:
            missing_comment_query = json.loads(response.read().decode("utf-8"))
        assert missing_comment_query["success"] is False, "comment list should fail without videoId"

        print("OK: /videos/comments communication test passed")

        with urllib.request.urlopen(
            f"{base_url}/videos/search?keyword=Mock",
            timeout=3,
        ) as response:
            title_search = json.loads(response.read().decode("utf-8"))
        assert title_search["success"] is True, "title search should succeed"
        assert len(title_search["videos"]) == 2, "Mock should match both seeded video titles"

        with urllib.request.urlopen(
            f"{base_url}/videos/search?keyword=%E7%BC%96%E7%A8%8B%E5%BC%80%E5%8F%91",
            timeout=3,
        ) as response:
            tag_search = json.loads(response.read().decode("utf-8"))
        assert tag_search["success"] is True, "tag search should succeed"
        assert [video["id"] for video in tag_search["videos"]] == ["video-001"], "tag search should match video-001"

        with urllib.request.urlopen(
            f"{base_url}/videos/search?keyword=not-found-keyword",
            timeout=3,
        ) as response:
            empty_search = json.loads(response.read().decode("utf-8"))
        assert empty_search["success"] is True, "empty search result is still a successful request"
        assert empty_search["videos"] == [], "unknown keyword should return empty list"

        with urllib.request.urlopen(f"{base_url}/videos/search", timeout=3) as response:
            missing_search = json.loads(response.read().decode("utf-8"))
        assert missing_search["success"] is False, "search should fail without keyword"

        print("OK: /videos/search communication test passed")

        with urllib.request.urlopen(
            f"{base_url}/videos/favorite-status?videoId=video-001&account=bit-user-001",
            timeout=3,
        ) as response:
            initial_favorite = json.loads(response.read().decode("utf-8"))
        assert initial_favorite["success"] is True, "favorite status should load"
        assert initial_favorite["favorited"] is False, "video should initially be unfavorited"

        favorite_success = post_json(
            f"{base_url}/videos/favorite",
            {"videoId": "video-001", "account": "bit-user-001"},
        )
        assert favorite_success["success"] is True, "favorite should succeed"
        assert favorite_success["favorited"] is True, "favorite should return final true state"

        favorite_repeat = post_json(
            f"{base_url}/videos/favorite",
            {"videoId": "video-001", "account": "bit-user-001"},
        )
        assert favorite_repeat["favorited"] is True, "repeat favorite should remain true"

        with urllib.request.urlopen(
            f"{base_url}/users/favorites?account=bit-user-001",
            timeout=3,
        ) as response:
            favorite_videos = json.loads(response.read().decode("utf-8"))
        assert favorite_videos["success"] is True, "favorite videos should load"
        assert [video["id"] for video in favorite_videos["videos"]] == ["video-001"], "favorite list should not duplicate video"

        unfavorite_success = post_json(
            f"{base_url}/videos/unfavorite",
            {"videoId": "video-001", "account": "bit-user-001"},
        )
        assert unfavorite_success["success"] is True, "unfavorite should succeed"
        assert unfavorite_success["favorited"] is False, "unfavorite should return final false state"

        guest_favorite = post_json(
            f"{base_url}/videos/favorite",
            {"videoId": "video-001", "account": ""},
        )
        assert guest_favorite["success"] is False, "guest should not favorite videos"

        print("OK: video favorite communication test passed")

        with urllib.request.urlopen(
            f"{base_url}/users/profile?account=bit-user-001",
            timeout=3,
        ) as response:
            initial_profile = json.loads(response.read().decode("utf-8"))
        assert initial_profile["success"] is True, "profile should load for known account"
        assert initial_profile["user"]["userName"] == "BIT 用户", "profile should return current name"

        updated_profile = post_json(
            f"{base_url}/users/profile",
            {
                "account": "bit-user-001",
                "userName": "BIT 新昵称",
                "description": "这是修改后的个人简介",
            },
        )
        assert updated_profile["success"] is True, "profile update should succeed"
        assert updated_profile["user"]["userName"] == "BIT 新昵称", "profile should return updated name"

        with urllib.request.urlopen(
            f"{base_url}/users/profile?account=bit-user-001",
            timeout=3,
        ) as response:
            reloaded_profile = json.loads(response.read().decode("utf-8"))
        assert reloaded_profile["user"]["description"] == "这是修改后的个人简介", "updated profile should persist in memory"

        empty_profile_name = post_json(
            f"{base_url}/users/profile",
            {"account": "bit-user-001", "userName": "", "description": "invalid"},
        )
        assert empty_profile_name["success"] is False, "empty profile name should fail"

        with urllib.request.urlopen(
            f"{base_url}/users/profile?account=missing-user",
            timeout=3,
        ) as response:
            missing_profile = json.loads(response.read().decode("utf-8"))
        assert missing_profile["success"] is False, "unknown profile should fail"

        restored_profile = post_json(
            f"{base_url}/users/profile",
            {
                "account": "bit-user-001",
                "userName": "BIT 用户",
                "description": "热爱视频，也热爱写代码。",
            },
        )
        assert restored_profile["success"] is True, "profile fixture should restore for later login tests"

        print("OK: /users/profile communication test passed")

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
        assert upload_success["video"]["title"] == "测试上传视频", "upload should return created video"

        with urllib.request.urlopen(
            f"{base_url}/users/videos?account=bit-user-001",
            timeout=3,
        ) as response:
            my_videos = json.loads(response.read().decode("utf-8"))
        assert my_videos["success"] is True, "my videos should load"
        assert len(my_videos["videos"]) == 1, "uploaded video should appear in my videos"
        assert my_videos["videos"][0]["title"] == "测试上传视频", "my videos should return uploaded metadata"

        with urllib.request.urlopen(f"{base_url}/users/videos", timeout=3) as response:
            missing_my_videos = json.loads(response.read().decode("utf-8"))
        assert missing_my_videos["success"] is False, "my videos should require account"

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

        file_upload = post_multipart(
            f"{base_url}/videos/upload",
            {
                "title": "真实文件上传测试",
                "description": "multipart upload",
                "category": "科技",
                "tags": ["编程开发"],
                "userName": "BIT 用户",
                "account": "bit-user-001",
                "videoFileName": "sample.mp4",
                "coverFileName": "cover.png",
            },
            b"fake-mp4-binary-content",
            b"fake-png-binary-content",
        )
        assert file_upload["success"] is True, "multipart file upload should succeed"
        stored_video_path = Path(file_upload["video"]["storedVideoPath"])
        stored_cover_path = Path(file_upload["video"]["storedCoverPath"])
        assert stored_video_path.read_bytes() == b"fake-mp4-binary-content", "server should save video bytes"
        assert stored_cover_path.read_bytes() == b"fake-png-binary-content", "server should save cover bytes"

        missing_file_upload = post_multipart(
            f"{base_url}/videos/upload",
            {
                "title": "缺少视频文件",
                "category": "科技",
                "userName": "BIT 用户",
                "account": "bit-user-001",
            },
            b"",
        )
        assert missing_file_upload["success"] is False, "multipart upload should require video bytes"

        stored_video_path.unlink(missing_ok=True)
        stored_cover_path.unlink(missing_ok=True)

        print("OK: /videos upload communication test passed")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
