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

客户端选择后端地址的优先级是：

1. `VIDEO_API_BASE_URL` 环境变量
2. `api.local.json` 本地配置文件
3. 默认值 `http://192.168.19.129:9000`

如果虚拟机 IP 变了，复制模板后只改本地配置，不用改代码：

```powershell
Copy-Item client\player\api.example.json client\player\api.local.json
notepad client\player\api.local.json
```

配置格式：

```json
{ "baseUrl": "http://192.168.19.129:9000" }
```

`api.local.json` 已被 Git 忽略，不要提交自己的本地 IP 配置。

如果要临时切回 mock server，不需要改代码，先设置环境变量：

```powershell
$env:VIDEO_API_BASE_URL = "http://127.0.0.1:8080"
python tools\mock_videos_server.py
```

然后再启动 Qt 客户端。环境变量优先级最高，所以它会覆盖 `api.local.json`。

真实后端返回的 `/uploads/...` 视频和头像资源会在客户端转换成完整 HTTP 地址；视频交给 mpv 播放，头像会由客户端异步下载后显示。

## 真实后端演示前检查清单

演示前按这个顺序走，能最大程度避免“客户端一打开就连不上”：

```powershell
ssh dev@192.168.19.129
cd /home/dev/workspace/video-on-demand-server
make dev-start
make dev-status
make dev-smoke
python3 tools/smoke_api.py --base-url http://127.0.0.1:9000 --write-checks
```

回到 Windows 后再检查宿主机能访问虚拟机服务：

```powershell
python D:\video-on-demand-server\.codex\work\tools\smoke_api.py --base-url http://192.168.19.129:9000 --write-checks
python tools\test_video_api_communication.py
cmake --build client\player\build\Desktop_Qt_6_7_3_MinGW_64_bit-Debug --target player -j 4
```

播放页仍保留本地 `D:/video-on-demand-client/test.mp4` 回退，避免页面完全空白；但这不代表真实后端播放成功。真实播放链路成功要看 `/videos/play-url` 返回 `/uploads/...`，并且这个 `/uploads/...` 地址能访问到视频文件。
