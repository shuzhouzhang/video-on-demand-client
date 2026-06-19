from http.server import BaseHTTPRequestHandler, HTTPServer
from datetime import datetime
from email.parser import BytesParser
from email.policy import default
import json
from pathlib import Path
import re
import tempfile
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
        "description": "热爱视频，也热爱写代码。",
    },
}

BARRAGES = {
    "video-001": {
        1: ["欢迎来到测试视频"],
        3: ["这条弹幕来自接口"],
    },
    "video-002": {},
}

VIDEO_LIKES = {}
VIDEO_FAVORITES = {}
WATCH_PROGRESS = {}
EMAIL_CODES = {}
ADMIN_REVIEWS = [
    {"videoId": "video-001", "title": "Mock 接口返回的视频", "userId": "mock-user-001", "status": "待审核", "uploadTime": "2026-06-09 10:00"},
    {"videoId": "video-002", "title": "Mock 美食探店视频", "userId": "tester-002", "status": "审核通过", "uploadTime": "2026-06-09 11:00"},
]
ADMIN_USERS = {
    "admin@bit.com": {"userName": "系统管理员", "role": "超级管理员", "status": "启用", "createdAt": "2026-05-01 10:00"},
    "bit-user-001": {"userName": "BIT 用户", "role": "普通用户", "status": "启用", "createdAt": "2026-06-01 09:00"},
    "review@bit.com": {"userName": "审核员", "role": "管理员", "status": "启用", "createdAt": "2026-05-12 14:30"},
}
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

UPLOAD_DIR = Path(tempfile.gettempdir()) / "video-on-demand-client-mock-uploads"


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
            self.handle_play_url(parsed_url.query)
            return

        if path == "/videos/barrages":
            self.handle_get_barrages(parsed_url.query)
            return

        if path == "/videos/like-status":
            self.handle_like_status(parsed_url.query)
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

        if path == "/videos/favorite-status":
            self.handle_favorite_status(parsed_url.query)
            return

        if path == "/users/favorites":
            self.handle_favorite_videos(parsed_url.query)
            return

        if path == "/users/profile":
            self.handle_get_user_profile(parsed_url.query)
            return

        if path == "/users/videos":
            self.handle_get_user_videos(parsed_url.query)
            return
        if path == "/admin/reviews":
            self.write_json(200, {"success": True, "reviews": ADMIN_REVIEWS})
            return
        if path == "/admin/users":
            self.handle_get_admin_users()
            return

        self.send_response(404)
        self.end_headers()

    def do_POST(self):
        if self.path == "/videos/upload":
            self.handle_upload_video_files()
            return
        if self.path == "/users/avatar":
            self.handle_upload_avatar()
            return

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

        if self.path == "/login/email-code":
            self.handle_email_code(payload)
            return

        if self.path == "/login/email":
            self.handle_email_login(payload)
            return
        if self.path == "/logout":
            account = str(payload.get("account", "")).strip()
            self.write_json(200, {"success": bool(account), "message": "退出成功" if account else "账号不能为空"})
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

        if self.path == "/videos/favorite":
            self.handle_video_favorite(payload, True)
            return

        if self.path == "/videos/unfavorite":
            self.handle_video_favorite(payload, False)
            return

        if self.path == "/users/profile":
            self.handle_update_user_profile(payload)
            return
        if self.path == "/admin/reviews/action":
            self.handle_admin_review_action(payload)
            return
        if self.path == "/admin/users/action":
            self.handle_admin_user_action(payload)
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

    def handle_email_code(self, payload):
        # 这是什么：创建邮箱验证码登录会话。
        # 为什么能实现：mock 按 authcodeId 保存邮箱和验证码，后续登录必须同时匹配。
        # 什么时候调用：Qt 点击“获取验证码”并 POST /login/email-code 时调用。
        # 和谁配合：debugCode 只供本地联调展示，真实后端应通过邮件发送而不返回明文。
        email = str(payload.get("email", "")).strip().lower()
        if not re.fullmatch(r"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}", email):
            self.write_json(200, {"success": False, "message": "邮箱格式错误"})
            return
        authcode_id = f"email-code-{len(EMAIL_CODES) + 1:03d}"
        debug_code = "246810"
        EMAIL_CODES[authcode_id] = {"email": email, "code": debug_code}
        self.write_json(
            200,
            {"success": True, "message": "验证码已发送", "authcodeId": authcode_id, "debugCode": debug_code},
        )

    def handle_email_login(self, payload):
        # 这是什么：校验邮箱验证码并完成登录或首次注册。
        # 为什么能实现：authcodeId 定位服务端会话，再核对邮箱和验证码；成功后立即删除会话防止复用。
        # 什么时候调用：Qt 邮箱登录表单 POST /login/email 时调用。
        # 和谁配合：返回字段与密码登录一致，客户端可复用 loginSucceeded。
        email = str(payload.get("email", "")).strip().lower()
        authcode_id = str(payload.get("authcodeId", "")).strip()
        authcode = str(payload.get("authcode", "")).strip()
        code_session = EMAIL_CODES.get(authcode_id)
        if code_session is None or code_session["email"] != email or code_session["code"] != authcode:
            self.write_json(200, {"success": False, "message": "验证码错误或已失效"})
            return
        EMAIL_CODES.pop(authcode_id, None)
        if email not in USERS:
            USERS[email] = {
                "password": "",
                "userName": email.split("@", 1)[0],
                "description": "",
            }
        self.write_json(200, {"success": True, "userName": USERS[email]["userName"], "account": email})

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

        video = {
            "id": f"video-{len(VIDEOS) + 1:03d}",
            "title": title,
            "userName": str(payload.get("userName", "")).strip() or account,
            "ownerAccount": account,
            "date": datetime.now().strftime("%m-%d"),
            "duration": "00:00",
            "playCount": "0",
            "likeCount": "0",
            "category": category,
            "tags": payload.get("tags", []),
            "description": str(payload.get("description", "")).strip(),
            "videoFileName": video_file_name,
            "coverFileName": str(payload.get("coverFileName", "")).strip(),
        }
        VIDEOS.append(video)
        COMMENTS[video["id"]] = []
        BARRAGES[video["id"]] = {}
        self.write_json(200, {"success": True, "message": "发布成功", "video": video})

    def handle_upload_video_files(self):
        # 这是什么：处理真实视频和封面文件的 multipart 上传。
        # 为什么能实现：email MIME 解析器能按 boundary 拆出 metadata、videoFile 和 coverFile 三个表单部分。
        # 什么时候调用：Qt ApiClient::uploadVideo() POST /videos/upload 时调用。
        # 和谁配合：保存文件后创建 VideoInfo 数据，后续 /users/videos 可立即查询到作品。
        content_type = self.headers.get("Content-Type", "")
        if "multipart/form-data" not in content_type:
            self.write_json(200, {"success": False, "message": "上传格式必须是 multipart/form-data"})
            return

        length = int(self.headers.get("Content-Length", "0"))
        raw_body = self.rfile.read(length)
        mime_message = BytesParser(policy=default).parsebytes(
            f"Content-Type: {content_type}\r\nMIME-Version: 1.0\r\n\r\n".encode("utf-8")
            + raw_body
        )

        metadata = {}
        files = {}
        for part in mime_message.iter_parts():
            field_name = part.get_param("name", header="content-disposition")
            if not field_name:
                continue
            content = part.get_payload(decode=True) or b""
            if field_name == "metadata":
                try:
                    metadata = json.loads(content.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError):
                    self.write_json(200, {"success": False, "message": "视频元数据格式错误"})
                    return
            else:
                files[field_name] = {
                    "filename": Path(part.get_filename() or "").name,
                    "content": content,
                }

        title = str(metadata.get("title", "")).strip()
        account = str(metadata.get("account", "")).strip()
        category = str(metadata.get("category", "")).strip()
        video_part = files.get("videoFile")
        if not title or not account or not category:
            self.write_json(200, {"success": False, "message": "标题、账号和分类不能为空"})
            return
        if not video_part or not video_part["filename"] or not video_part["content"]:
            self.write_json(200, {"success": False, "message": "视频文件不能为空"})
            return

        video_id = f"video-{len(VIDEOS) + 1:03d}"
        UPLOAD_DIR.mkdir(parents=True, exist_ok=True)
        stored_video_name = f"{video_id}-{video_part['filename']}"
        video_path = UPLOAD_DIR / stored_video_name
        video_path.write_bytes(video_part["content"])

        cover_part = files.get("coverFile")
        stored_cover_name = ""
        if cover_part and cover_part["filename"] and cover_part["content"]:
            stored_cover_name = f"{video_id}-{cover_part['filename']}"
            (UPLOAD_DIR / stored_cover_name).write_bytes(cover_part["content"])

        video = {
            "id": video_id,
            "title": title,
            "userName": str(metadata.get("userName", "")).strip() or account,
            "ownerAccount": account,
            "date": datetime.now().strftime("%m-%d"),
            "duration": "00:00",
            "playCount": "0",
            "likeCount": "0",
            "category": category,
            "tags": metadata.get("tags", []),
            "description": str(metadata.get("description", "")).strip(),
            "videoFileName": video_part["filename"],
            "coverFileName": cover_part["filename"] if cover_part else "",
            "storedVideoPath": str(video_path),
            "storedCoverPath": str(UPLOAD_DIR / stored_cover_name) if stored_cover_name else "",
        }
        VIDEOS.append(video)
        COMMENTS[video_id] = []
        BARRAGES[video_id] = {}
        self.write_json(200, {"success": True, "message": "文件上传成功", "video": video})

    def handle_play_url(self, query):
        # 这是什么：按 videoId 返回视频播放地址。
        # 为什么能实现：先确认 VIDEOS 中存在该 id，再返回它对应的测试播放路径。
        # 什么时候调用：Qt 播放页打开后通过 ApiClient::fetchPlayUrl() 请求播放地址时调用。
        # 和谁配合：PlayerPage 收到 playUrl 后交给 MpvPlayer 播放，失败时回退本地路径。
        video_id = parse_qs(query).get("videoId", [""])[0].strip()
        video = next((item for item in VIDEOS if item.get("id") == video_id), None)
        if video is None:
            self.write_json(200, {"success": False, "message": "视频不存在"})
            return
        play_url = video.get("storedVideoPath") or "D:/video-on-demand-client/test.mp4"
        self.write_json(200, {"success": True, "videoId": video_id, "playUrl": play_url})

    def handle_get_barrages(self, query):
        # 这是什么：处理按视频查询弹幕列表的 GET /videos/barrages。
        # 为什么能实现：mock server 用内存字典按 videoId 和秒数保存弹幕，不依赖数据库。
        # 什么时候调用：播放页拿到 m_videoId 后，通过 ApiClient::fetchBarrages() 拉取弹幕时调用。
        # 和谁配合：ApiClient 解析返回的 barrages，PlayerPage 按播放秒数触发显示。
        video_id = parse_qs(query).get("videoId", [""])[0].strip()
        if not any(video.get("id") == video_id for video in VIDEOS):
            self.write_json(200, {"success": False, "message": "视频不存在"})
            return
        items = []
        for seconds, texts in BARRAGES.get(video_id, {}).items():
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
        # 这是什么：处理发送弹幕接口 POST /videos/barrages。
        # 为什么能实现：从 JSON 里读取 videoId、seconds、text，校验后追加到对应视频的 BARRAGES 集合。
        # 什么时候调用：用户在播放页输入弹幕并点击发送时调用。
        # 和谁配合：ApiClient::sendBarrage() 发请求，PlayerPage 成功后立即展示并写入本地缓存。
        video_id = str(payload.get("videoId", "")).strip()
        text = str(payload.get("text", "")).strip()
        try:
            seconds = int(payload.get("seconds", -1))
        except (TypeError, ValueError):
            seconds = -1

        if not video_id:
            self.write_json(200, {"success": False, "message": "视频标识不能为空"})
            return
        if not any(video.get("id") == video_id for video in VIDEOS):
            self.write_json(200, {"success": False, "message": "视频不存在"})
            return
        if seconds < 0:
            self.write_json(200, {"success": False, "message": "弹幕时间非法"})
            return
        if not text:
            self.write_json(200, {"success": False, "message": "弹幕内容不能为空"})
            return

        BARRAGES.setdefault(video_id, {}).setdefault(seconds, []).append(text[:30])
        self.write_json(200, {"success": True, "message": "发送成功", "seconds": seconds, "text": text[:30]})

    def handle_like_status(self, query):
        # 这是什么：读取当前账号对视频的点赞状态和视频点赞数。
        # 为什么能实现：VIDEO_LIKES 保存关系，VIDEOS 保存 likeCount，两者组合就是完整初始状态。
        # 什么时候调用：播放页初始化点赞按钮时调用。
        # 和谁配合：ApiClient::fetchVideoLikeStatus() 将结果交给 PlayerPage。
        params = parse_qs(query)
        video_id = params.get("videoId", [""])[0].strip()
        account = params.get("account", ["guest"])[0].strip() or "guest"
        video = next((item for item in VIDEOS if item.get("id") == video_id), None)
        if video is None:
            self.write_json(200, {"success": False, "message": "视频不存在"})
            return
        self.write_json(
            200,
            {
                "success": True,
                "liked": account in VIDEO_LIKES.get(video_id, set()),
                "likeCount": video.get("likeCount", "0"),
            },
        )

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

    def handle_favorite_status(self, query):
        # 这是什么：查询某个账号是否收藏了某个视频。
        # 为什么能实现：VIDEO_FAVORITES 按 videoId 保存账号集合，判断 account 是否在集合中即可。
        # 什么时候调用：播放页初始化收藏星标状态时调用。
        # 和谁配合：ApiClient::fetchFavoriteStatus() 解析 favorited 并更新 PlayerPage。
        params = parse_qs(query)
        video_id = params.get("videoId", [""])[0].strip()
        account = params.get("account", [""])[0].strip()
        if not video_id or not account:
            self.write_json(200, {"success": False, "message": "视频 id 和账号不能为空"})
            return
        if not any(video.get("id") == video_id for video in VIDEOS):
            self.write_json(200, {"success": False, "message": "视频不存在"})
            return
        self.write_json(
            200,
            {"success": True, "favorited": account in VIDEO_FAVORITES.get(video_id, set())},
        )

    def handle_video_favorite(self, payload, should_favorite):
        # 这是什么：处理收藏和取消收藏视频的关系写入。
        # 为什么能实现：账号集合天然去重，重复收藏不会产生多份关系，discard 也能安全取消。
        # 什么时候调用：Qt 播放页 POST /videos/favorite 或 /videos/unfavorite 时调用。
        # 和谁配合：我的收藏接口再按账号反查这些关系并返回完整视频。
        video_id = str(payload.get("videoId", "")).strip()
        account = str(payload.get("account", "")).strip()
        if not video_id:
            self.write_json(200, {"success": False, "message": "视频 id 不能为空"})
            return
        if not account:
            self.write_json(200, {"success": False, "message": "请先登录后再收藏视频"})
            return
        if not any(video.get("id") == video_id for video in VIDEOS):
            self.write_json(200, {"success": False, "message": "视频不存在"})
            return

        accounts = VIDEO_FAVORITES.setdefault(video_id, set())
        if should_favorite:
            accounts.add(account)
        else:
            accounts.discard(account)
        self.write_json(200, {"success": True, "favorited": account in accounts})

    def handle_favorite_videos(self, query):
        # 这是什么：返回当前账号收藏的完整视频列表。
        # 为什么能实现：遍历 VIDEOS，并保留收藏账号集合中包含当前 account 的视频。
        # 什么时候调用：用户点击“我的收藏”入口时调用。
        # 和谁配合：ApiClient 转成 QList<VideoInfo>，player.cpp 渲染 VideoBox。
        account = parse_qs(query).get("account", [""])[0].strip()
        if not account:
            self.write_json(200, {"success": False, "message": "请先登录后查看收藏"})
            return
        videos = [
            video
            for video in VIDEOS
            if account in VIDEO_FAVORITES.get(video.get("id", ""), set())
        ]
        self.write_json(200, {"success": True, "videos": videos})

    def handle_get_user_profile(self, query):
        # 这是什么：读取当前账号的个人资料。
        # 为什么能实现：USERS 以 account 为键保存昵称和简介，可直接组装成安全的公开资料响应。
        # 什么时候调用：登录成功后 Qt 通过 ApiClient::fetchUserProfile() 刷新“我的”页面时调用。
        # 和谁配合：DataCenter 保存响应，player.cpp 更新昵称、账号和简介。
        account = parse_qs(query).get("account", [""])[0].strip()
        user = USERS.get(account)
        if not account or user is None:
            self.write_json(200, {"success": False, "message": "用户不存在"})
            return
        self.write_json(
            200,
            {
                "success": True,
                "user": {
                    "account": account,
                    "userName": user["userName"],
                    "description": user.get("description", ""),
                    "avatarPath": user.get("avatarPath", ""),
                },
            },
        )

    def handle_update_user_profile(self, payload):
        # 这是什么：修改当前账号的昵称和简介。
        # 为什么能实现：账号定位 USERS 中的记录，校验长度后更新内存字段并返回最终资料。
        # 什么时候调用：ProfileDialog 通过 ApiClient::updateUserProfile() 提交表单时调用。
        # 和谁配合：Qt 使用返回值同步 DataCenter 和“我的”页面。
        account = str(payload.get("account", "")).strip()
        user_name = str(payload.get("userName", "")).strip()
        description = str(payload.get("description", "")).strip()
        user = USERS.get(account)
        if not account or user is None:
            self.write_json(200, {"success": False, "message": "用户不存在"})
            return
        if not user_name or len(user_name) > 20:
            self.write_json(200, {"success": False, "message": "昵称需为 1 到 20 个字符"})
            return
        if len(description) > 100:
            self.write_json(200, {"success": False, "message": "个人简介不能超过 100 个字符"})
            return
        user["userName"] = user_name
        user["description"] = description
        self.write_json(
            200,
            {
                "success": True,
                "message": "保存成功",
                "user": {
                    "account": account,
                    "userName": user_name,
                    "description": description,
                    "avatarPath": user.get("avatarPath", ""),
                },
            },
        )

    def handle_upload_avatar(self):
        # 这是什么：处理用户头像 multipart 文件上传。
        # 为什么能实现：按表单字段读取 account 和 avatarFile，保存后把路径写回 USERS。
        # 什么时候调用：Qt “我的”页面通过 ApiClient::uploadAvatar() 上传头像时调用。
        # 和谁配合：个人资料 GET 返回 avatarPath，客户端下次登录可以恢复头像。
        content_type = self.headers.get("Content-Type", "")
        if "multipart/form-data" not in content_type:
            self.write_json(200, {"success": False, "message": "头像上传格式错误"})
            return
        length = int(self.headers.get("Content-Length", "0"))
        if length <= 0 or length > 5 * 1024 * 1024 + 64 * 1024:
            self.write_json(200, {"success": False, "message": "头像大小不能超过 5MB"})
            return
        message = BytesParser(policy=default).parsebytes(
            f"Content-Type: {content_type}\r\nMIME-Version: 1.0\r\n\r\n".encode("utf-8")
            + self.rfile.read(length)
        )
        account = ""
        avatar_name = ""
        avatar_content = b""
        for part in message.iter_parts():
            field_name = part.get_param("name", header="content-disposition")
            if field_name == "account":
                account = (part.get_payload(decode=True) or b"").decode("utf-8").strip()
            elif field_name == "avatarFile":
                avatar_name = Path(part.get_filename() or "").name
                avatar_content = part.get_payload(decode=True) or b""
        if account not in USERS:
            self.write_json(200, {"success": False, "message": "用户不存在"})
            return
        if Path(avatar_name).suffix.lower() not in {".png", ".jpg", ".jpeg"} or not avatar_content:
            self.write_json(200, {"success": False, "message": "请选择 PNG 或 JPG 头像"})
            return
        avatar_dir = UPLOAD_DIR / "avatars"
        avatar_dir.mkdir(parents=True, exist_ok=True)
        avatar_path = avatar_dir / f"{account.replace('@', '_at_')}-{avatar_name}"
        avatar_path.write_bytes(avatar_content)
        USERS[account]["avatarPath"] = str(avatar_path)
        self.write_json(200, {"success": True, "message": "头像上传成功", "avatarPath": str(avatar_path)})

    def handle_get_user_videos(self, query):
        # 这是什么：返回当前账号发布的视频列表。
        # 为什么能实现：上传接口会把 account 写进 ownerAccount，按该字段筛选即可确定作品归属。
        # 什么时候调用：用户点击“我的视频”或上传成功后刷新作品时调用。
        # 和谁配合：ApiClient::fetchMyVideos() 解析结果，player.cpp 展示 VideoBox。
        account = parse_qs(query).get("account", [""])[0].strip()
        if not account:
            self.write_json(200, {"success": False, "message": "请先登录后查看作品"})
            return
        videos = [video for video in VIDEOS if video.get("ownerAccount") == account]
        self.write_json(200, {"success": True, "videos": videos})

    def handle_admin_review_action(self, payload):
        # 这是什么：修改某个视频审核记录的最终状态。
        # 为什么能实现：videoId 定位 ADMIN_REVIEWS 中唯一记录，status 只允许通过或拒绝。
        # 什么时候调用：后台审核表格点击“通过”或“拒绝”时调用。
        # 和谁配合：客户端成功后重新 GET /admin/reviews 刷新表格。
        video_id = str(payload.get("videoId", "")).strip()
        status = str(payload.get("status", "")).strip()
        review = next((item for item in ADMIN_REVIEWS if item["videoId"] == video_id), None)
        if review is None or status not in {"审核通过", "审核拒绝"}:
            self.write_json(200, {"success": False, "message": "审核参数错误"})
            return
        review["status"] = status
        self.write_json(200, {"success": True, "message": "审核状态已更新"})

    def handle_get_admin_users(self):
        users = [
            {"account": account, **info}
            for account, info in ADMIN_USERS.items()
        ]
        self.write_json(200, {"success": True, "users": users})

    def handle_admin_user_action(self, payload):
        # 这是什么：处理后台用户角色、状态和删除操作。
        # 为什么能实现：account 定位 ADMIN_USERS，action 映射到确定字段变化。
        # 什么时候调用：角色表格操作按钮或“添加管理员”提交账号时调用。
        # 和谁配合：客户端成功后重新 GET /admin/users 刷新表格。
        account = str(payload.get("account", "")).strip()
        action = str(payload.get("action", "")).strip()
        user = ADMIN_USERS.get(account)
        if user is None:
            self.write_json(200, {"success": False, "message": "用户不存在"})
            return
        if action == "set-admin":
            user["role"] = "管理员"
        elif action == "disable":
            user["status"] = "禁用"
        elif action == "enable":
            user["status"] = "启用"
        elif action == "delete":
            ADMIN_USERS.pop(account, None)
        else:
            self.write_json(200, {"success": False, "message": "角色操作不支持"})
            return
        self.write_json(200, {"success": True, "message": "角色操作成功"})


def create_server(host="127.0.0.1", port=8080):
    return HTTPServer((host, port), MockVideosHandler)


if __name__ == "__main__":
    server = create_server()
    print("Mock server running with videos, login, playback, social and user-library mock endpoints")
    server.serve_forever()
