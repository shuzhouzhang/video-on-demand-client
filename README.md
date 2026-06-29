# video-on-demand-client

Qt 视频点播客户端。当前客户端通过 `ApiClient` 统一访问后端 HTTP/JSON 接口。

## 后端联调

默认后端地址为：

```text
http://192.168.19.129:9000
```

也就是本地开发虚拟机里的 `video-on-demand-server`。启动客户端前，请先确认后端服务可用：

```powershell
python D:\video-on-demand-server\.codex\work\tools\smoke_api.py --base-url http://192.168.19.129:9000
```

如果要临时切回 mock server，不需要改代码，先设置环境变量：

```powershell
$env:VIDEO_API_BASE_URL = "http://127.0.0.1:8080"
python tools\mock_videos_server.py
```

然后再启动 Qt 客户端。`ApiClient` 会优先读取 `VIDEO_API_BASE_URL`，没有设置时才使用默认真实后端地址。
