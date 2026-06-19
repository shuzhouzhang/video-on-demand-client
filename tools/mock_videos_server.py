from http.server import BaseHTTPRequestHandler, HTTPServer
from datetime import datetime
import json
from urllib.parse import parse_qs, urlparse


VIDEOS = [
    {
        "id": "video-001",
        "title": "Mock 接口返回的视频",
        "userName": "Mock 用户",
        "date": "6-9",
        "duration": "09:18",
        "playCount": "3.6万",
        "likeCount": "256",
        "category": "科技",
        "tags": ["编程开发", "软件工具"],
        "description": "这条视频来自 mock server，用来验证播放页能通过 videoId 拉取详情。",
    },
    {
        "id": "video-002",
        "title": "Mock 美食探店视频",
        "userName": "接口测试员",
        "date": "6-9",
        "duration": "12:08",
        "playCount": "1.8万",
        "likeCount": "88",
        "category": "美食",
        "tags": ["美食测评", "探店"],
        "description": "一条用于测试分类、标签和详情展示的美食探店视频。",
    },
]

USERS = {
    "bit-user-001": {
        "password": "bit123456",
        "userName": "BIT 用户",
    },
}

BARRAGES = {
    "D:/video-on-demand-client/test.mp4": {
        1: ["欢迎来到测试视频"],
        3: ["这条弹幕来自接口"],
    },
}

VIDEO_LIKES = {}
WATCH_PROGRESS = {}
COMMENTS = {
    "video-001": [
        {
            "id": "comment-002",
            "videoId": "video-001",
            "userName": "接口测试员",
            "account": "tester-002",
            "content": "评论列表接口已经成功返回啦",
            "createdAt": "2026-06-20 10:20",
        },
        {
            "id": "comment-001",
            "videoId": "video-001",
            "userName": "Mock 用户",
            "account": "mock-user-001",
            "content": "这是一条来自 mock server 的初始评论",
            "createdAt": "2026-06-20 10:00",
        },
    ],
    "video-002": [],
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
        parsed_url = urlparse(self.path)
        path = parsed_url.path

        if path == "/videos":
            self.write_json(200, VIDEOS)
            return

        if path == "/videos/play-url":
            self.handle_play_url()
            return

        if path == "/videos/barrages":
            self.handle_get_barrages()
            return

        if path == "/videos/detail":
            self.handle_video_detail(parsed_url.query)
            return

        if path == "/videos/watch-progress":
            self.handle_get_watch_progress(parsed_url.query)
            return

        if path == "/videos/comments":
            self.handle_get_comments(parsed_url.query)
            return

        if path == "/videos/search":
            self.handle_search_videos(parsed_url.query)
            return

        self.send_response(404)
        self.end_headers()

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(length).decode("utf-8")
        try:
            payload = json.loads(raw_body) if raw_body else {}
        except json.JSONDecodeError:
            self.write_json(200, {"success": False, "message": "请求格式错误"})
            return

        if self.path == "/login":
            self.handle_login(payload)
            return

        if self.path == "/videos":
            self.handle_upload_video(payload)
            return

        if self.path == "/videos/barrages":
            self.handle_send_barrage(payload)
            return

        if self.path == "/videos/like":
            self.handle_video_like(payload, True)
            return

        if self.path == "/videos/unlike":
            self.handle_video_like(payload, False)
            return

        if self.path == "/videos/watch-progress":
            self.handle_save_watch_progress(payload)
            return

        if self.path == "/videos/comments":
            self.handle_send_comment(payload)
            return

        self.send_response(404)
        self.end_headers()

    def handle_login(self, payload):
        # 这是什么：处理临时登录接口 POST /login。
        # 为什么能实现：从请求体读取 account/password，和 USERS 里的临时账号做匹配。
        # 什么时候调用：Qt 登录页或通信测试向 mock server 发送登录请求时调用。
        # 和谁配合：ApiClient::login() 发送 JSON，Login 根据 success/message 展示登录结果。
        account = str(payload.get("account", "")).strip()
        password = str(payload.get("password", ""))
        user = USERS.get(account)
        if user is None or user["password"] != password:
            self.write_json(200, {"success": False, "message": "账号或密码错误"})
            return

        self.write_json(200, {"success": True, "userName": user["userName"], "account": account})

    def handle_upload_video(self, payload):
        # 这是什么：处理上传视频元数据接口 POST /videos。
        # 为什么能实现：第一版只校验 JSON 元数据，不接收真实视频二进制，mock server 可以直接返回发布结果。
        # 什么时候调用：Qt 上传页点击发布并通过 ApiClient::uploadVideo() 发送请求时调用。
        # 和谁配合：UploadVideoPage 收集表单字段，ApiClient 发送 JSON，通信测试验证成功和失败场景。
        title = str(payload.get("title", "")).strip()
        account = str(payload.get("account", "")).strip()
        category = str(payload.get("category", "")).strip()
        video_file_name = str(payload.get("videoFileName", "")).strip()

        if not title:
            self.write_json(200, {"success": False, "message": "视频标题不能为空"})
            return
        if not account:
            self.write_json(200, {"success": False, "message": "请先登录后再发布"})
            return
        if not category:
            self.write_json(200, {"success": False, "message": "请选择视频分类"})
            return
        if not video_file_name:
            self.write_json(200, {"success": False, "message": "请先选择视频文件"})
            return

        self.write_json(200, {"success": True, "message": "发布成功"})

    def handle_play_url(self):
        # 这是什么：处理最小版播放地址接口 GET /videos/play-url。
        # 为什么能实现：第一版不区分视频 id，固定返回本地 test.mp4 路径即可验证“播放页从接口拿地址”。
        # 什么时候调用：Qt 播放页打开后通过 ApiClient::fetchPlayUrl() 请求播放地址时调用。
        # 和谁配合：PlayerPage 收到 playUrl 后交给 MpvPlayer 播放，失败时回退本地路径。
        self.write_json(200, {"success": True, "playUrl": "D:/video-on-demand-client/test.mp4"})

    def handle_get_barrages(self):
        # 这是什么：处理第一版弹幕列表接口 GET /videos/barrages。
        # 为什么能实现：mock server 用内存字典按 videoKey 和秒数保存弹幕，不依赖数据库。
        # 什么时候调用：播放页拿到 m_videoKey 后，通过 ApiClient::fetchBarrages() 拉取弹幕时调用。
        # 和谁配合：ApiClient 解析返回的 barrages，PlayerPage 按播放秒数触发显示。
        video_key = "D:/video-on-demand-client/test.mp4"
        items = []
        for seconds, texts in BARRAGES.get(video_key, {}).items():
            for text in texts:
                items.append({"seconds": seconds, "text": text})

        self.write_json(200, {"success": True, "barrages": items})

    def handle_video_detail(self, query):
        # 这是什么：处理第一版视频详情接口 GET /videos/detail?id=...。
        # 为什么能实现：mock server 的 VIDEOS 已经保存了 id 和详情字段，按 id 查到后直接包装成 JSON 返回。
        # 什么时候调用：Qt 播放页打开后通过 ApiClient::fetchVideoDetail() 请求当前视频详情时调用。
        # 和谁配合：VideoBox 把 videoId 传给 PlayerPage，PlayerPage 请求详情后刷新标题、作者、播放量和简介。
        video_id = parse_qs(query).get("id", [""])[0].strip()
        if not video_id:
            self.write_json(200, {"success": False, "message": "视频 id 不能为空"})
            return

        for video in VIDEOS:
            if video.get("id") == video_id:
                self.write_json(200, {"success": True, "video": video})
                return

        self.write_json(200, {"success": False, "message": "视频不存在"})

    def handle_send_barrage(self, payload):
        # 这是什么：处理第一版发送弹幕接口 POST /videos/barrages。
        # 为什么能实现：从 JSON 里读取 videoKey、seconds、text，校验后追加到 BARRAGES 内存字典。
        # 什么时候调用：用户在播放页输入弹幕并点击发送时调用。
        # 和谁配合：ApiClient::sendBarrage() 发请求，PlayerPage 成功后立即展示并写入本地缓存。
        video_key = str(payload.get("videoKey", "")).strip()
        text = str(payload.get("text", "")).strip()
        try:
            seconds = int(payload.get("seconds", -1))
        except (TypeError, ValueError):
            seconds = -1

        if not video_key:
            self.write_json(200, {"success": False, "message": "视频标识不能为空"})
            return
        if seconds < 0:
            self.write_json(200, {"success": False, "message": "弹幕时间非法"})
            return
        if not text:
            self.write_json(200, {"success": False, "message": "弹幕内容不能为空"})
            return

        BARRAGES.setdefault(video_key, {}).setdefault(seconds, []).append(text[:30])
        self.write_json(200, {"success": True, "message": "发送成功", "seconds": seconds, "text": text[:30]})

    def handle_video_like(self, payload, should_like):
        # 这是什么：处理第一版点赞/取消点赞接口。
        # 为什么能实现：mock server 用 VIDEO_LIKES 在内存里记录“某个账号是否点赞了某个 videoId”。
        # 什么时候调用：Qt 播放页点击点赞按钮，通过 ApiClient::likeVideo()/unlikeVideo() 发送请求时调用。
        # 和谁配合：PlayerPage 根据返回的 liked 和 likeCount 更新按钮状态和点赞数。
        video_id = str(payload.get("videoId", "")).strip()
        account = str(payload.get("account", "")).strip() or "guest"
        if not video_id:
            self.write_json(200, {"success": False, "message": "视频 id 不能为空"})
            return

        video = next((item for item in VIDEOS if item.get("id") == video_id), None)
        if video is None:
            self.write_json(200, {"success": False, "message": "视频不存在"})
            return

        liked_accounts = VIDEO_LIKES.setdefault(video_id, set())
        was_liked = account in liked_accounts
        if should_like and not was_liked:
            liked_accounts.add(account)
            video["likeCount"] = str(int(video.get("likeCount", "0")) + 1)
        elif not should_like and was_liked:
            liked_accounts.remove(account)
            video["likeCount"] = str(max(0, int(video.get("likeCount", "0")) - 1))

        self.write_json(
            200,
            {
                "success": True,
                "liked": account in liked_accounts,
                "likeCount": video["likeCount"],
            },
        )

    def handle_get_watch_progress(self, query):
        # 这是什么：处理第一版读取播放记录接口 GET /videos/watch-progress。
        # 为什么能实现：mock server 用 WATCH_PROGRESS 按 account + videoId 保存秒数，读取时没有记录就返回 0。
        # 什么时候调用：Qt 播放页打开后，通过 ApiClient::fetchWatchProgress() 查询上次看到哪里时调用。
        # 和谁配合：PlayerPage 收到 seconds 后，在 mpv 加载播放地址后 seek 到对应位置。
        params = parse_qs(query)
        video_id = params.get("videoId", [""])[0].strip()
        account = params.get("account", [""])[0].strip() or "guest"
        if not video_id:
            self.write_json(200, {"success": False, "message": "视频 id 不能为空"})
            return

        seconds = WATCH_PROGRESS.get((account, video_id), 0)
        self.write_json(200, {"success": True, "seconds": seconds, "message": "读取成功"})

    def handle_save_watch_progress(self, payload):
        # 这是什么：处理第一版保存播放记录接口 POST /videos/watch-progress。
        # 为什么能实现：从 JSON 读取 videoId/account/seconds，校验后写入 WATCH_PROGRESS 内存字典。
        # 什么时候调用：播放页定时保存、暂停或关闭时，通过 ApiClient::saveWatchProgress() 提交进度。
        # 和谁配合：下次 handle_get_watch_progress() 读取同一个 account + videoId 的秒数。
        video_id = str(payload.get("videoId", "")).strip()
        account = str(payload.get("account", "")).strip() or "guest"
        try:
            seconds = int(payload.get("seconds", -1))
        except (TypeError, ValueError):
            seconds = -1

        if not video_id:
            self.write_json(200, {"success": False, "message": "视频 id 不能为空"})
            return
        if seconds < 0:
            self.write_json(200, {"success": False, "message": "播放秒数非法"})
            return

        WATCH_PROGRESS[(account, video_id)] = seconds
        self.write_json(200, {"success": True, "seconds": seconds, "message": "保存成功"})

    def handle_get_comments(self, query):
        # 这是什么：处理第一版视频评论列表接口 GET /videos/comments。
        # 为什么能实现：COMMENTS 按 videoId 保存评论数组，查询时直接返回对应列表。
        # 什么时候调用：用户打开播放页评论窗口，ApiClient::fetchComments() 发请求时调用。
        # 和谁配合：ApiClient 把 JSON 数组解析成 CommentInfo，CommentDialog 按最新优先展示。
        video_id = parse_qs(query).get("videoId", [""])[0].strip()
        if not video_id:
            self.write_json(200, {"success": False, "message": "视频 id 不能为空"})
            return
        if not any(video.get("id") == video_id for video in VIDEOS):
            self.write_json(200, {"success": False, "message": "视频不存在"})
            return

        self.write_json(200, {"success": True, "comments": COMMENTS.get(video_id, [])})

    def handle_send_comment(self, payload):
        # 这是什么：处理第一版发表评论接口 POST /videos/comments。
        # 为什么能实现：校验登录身份和正文后，mock server 生成 id/时间并写入 COMMENTS 内存列表顶部。
        # 什么时候调用：已登录用户在 CommentDialog 点击发送，ApiClient::sendComment() 发请求时调用。
        # 和谁配合：返回完整 comment 对象，让 Qt 页面无需重新拉取即可立即显示。
        video_id = str(payload.get("videoId", "")).strip()
        user_name = str(payload.get("userName", "")).strip()
        account = str(payload.get("account", "")).strip()
        content = str(payload.get("content", "")).strip()
        if not video_id:
            self.write_json(200, {"success": False, "message": "视频 id 不能为空"})
            return
        if not any(video.get("id") == video_id for video in VIDEOS):
            self.write_json(200, {"success": False, "message": "视频不存在"})
            return
        if not account or not user_name:
            self.write_json(200, {"success": False, "message": "请先登录后再发表评论"})
            return
        if not content or len(content) > 200:
            self.write_json(200, {"success": False, "message": "评论内容需为 1 到 200 个字符"})
            return

        comment_count = sum(len(items) for items in COMMENTS.values())
        comment = {
            "id": f"comment-{comment_count + 1:03d}",
            "videoId": video_id,
            "userName": user_name,
            "account": account,
            "content": content,
            "createdAt": datetime.now().strftime("%Y-%m-%d %H:%M"),
        }
        COMMENTS.setdefault(video_id, []).insert(0, comment)
        self.write_json(200, {"success": True, "message": "评论成功", "comment": comment})

    def handle_search_videos(self, query):
        # 这是什么：处理第一版视频搜索接口 GET /videos/search。
        # 为什么能实现：把关键词转成小写后，在标题、作者、分类、标签和简介拼成的文本中匹配。
        # 什么时候调用：Qt 首页通过 ApiClient::searchVideos() 提交非空关键词时调用。
        # 和谁配合：返回字段与 /videos 一致，player.cpp 可继续用 VideoBox 展示结果。
        keyword = parse_qs(query).get("keyword", [""])[0].strip()
        if not keyword:
            self.write_json(200, {"success": False, "message": "搜索关键词不能为空"})
            return

        normalized_keyword = keyword.casefold()
        matched_videos = []
        for video in VIDEOS:
            searchable_text = " ".join(
                [
                    str(video.get("title", "")),
                    str(video.get("userName", "")),
                    str(video.get("category", "")),
                    " ".join(video.get("tags", [])),
                    str(video.get("description", "")),
                ]
            ).casefold()
            if normalized_keyword in searchable_text:
                matched_videos.append(video)

        self.write_json(200, {"success": True, "videos": matched_videos})


def create_server(host="127.0.0.1", port=8080):
    return HTTPServer((host, port), MockVideosHandler)


if __name__ == "__main__":
    server = create_server()
    print("Mock server running at videos/login/play-url/barrages/detail/like/watch-progress/comments/search mock endpoints")
    server.serve_forever()
