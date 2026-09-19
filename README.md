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


## 登录会话与鉴权回归

密码登录 `/login` 和验证码登录 `/login/email` 都必须返回
`success=true`、`userName`、`account`、`token`。ApiClient 校验后先调用
`DataCenter::saveSession()`，再发送登录成功信号；页面不能只凭账号创建登录态。
Token 仅存于进程内存，所有 ApiClient 实例共享，不写配置或日志。
本地只判断凭证是否存在及格式合法，是否过期仍由服务端校验。

`ApiClient::apiRequest()` 为业务 GET、JSON POST、multipart 上传统一添加 Bearer 头，
且只使用同源会话（协议、主机、有效端口必须一致）。登录接口不带旧凭证；
头像下载和 mpv 播放使用独立路径，不携带 Token。API 自动重定向被禁用，避免凭证外发及写操作重放。

受保护请求的 401 清空对应会话，主窗口只显示一次失效提示；403、5xx 和网络错误保留会话。
请求携带会话版本，因此旧请求不会清掉新登录，也不会在退出后恢复旧资料。
主动退出先用旧 Token 发 `/logout`，随即清空本机状态；远端注销失败会明确提示，且不会自动重试。
此时服务端旧 Token 可能持续有效直到过期。

Windows Qt 6.7.3 / MinGW 回归命令：

```powershell
$env:PATH = "D:\qt673\Tools\mingw1120_64\bin;D:\qt673\6.7.3\mingw_64\bin;" + $env:PATH
cmake -S client/player -B client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug -DBUILD_TESTING=ON
cmake --build client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug --target player token_auth_test -j 4
ctest --test-dir client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug --output-on-failure
python tools/test_video_api_communication.py
```

`token_auth_test` 直接编译真实 ApiClient/DataCenter，以本地 HTTP 替身验证两种登录、
跨实例共享、26 条业务路径的请求头（含两种文件上传）、并发 401、403/503、
迟到响应、退出清理、来源隔离和重定向。测试凭证随机生成，仅在内存中断言，不输出其内容。
原 Python 演示 mock 仅补齐登录 Token 字段，并不实施 Redis 鉴权；其通信测试不能代替 Qt 鉴权测试或真实联调。

2026-09-16 验证时，SSH `dev@192.168.19.129:2222` 可用，但虚拟机本机
`127.0.0.1:9000` 连接失败，真实登录→受保护请求→注销链路未验证；服务端源码及鉴权配置未修改。
