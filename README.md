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
cmake --build client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug --target player token_auth_test login_test playerpage_test mpvplayer_test -j 4
ctest --test-dir client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug --output-on-failure
python tools/test_video_api_communication.py
```

`token_auth_test` 直接编译真实 ApiClient/DataCenter，以本地 HTTP 替身验证两种登录、
跨实例共享、26 条业务路径的请求头（含两种文件上传）、并发 401、403/503、
迟到响应、退出清理、来源隔离和重定向。测试凭证随机生成，仅在内存中断言，不输出其内容。
原 Python 演示 mock 仅补齐登录 Token 字段，并不实施 Redis 鉴权；其通信测试不能代替 Qt 鉴权测试或真实联调。

`playerpage_test` 使用真实 ApiClient 和本地 HTTP 替身，并用可控播放器事件验证：
进度先返回/文件先加载两种顺序、尚未加载时关闭不覆盖历史、关闭后保存请求仍完成、
本地视频回退及账号切换不误写观看记录、EOF 后点击播放重新加载。`login_test` 操作真实登录控件，
验证非空既有凭证送达后端、空密码仍被阻止，避免前端将注册规则套在登录上。`mpvplayer_test` 编译真实 MpvPlayer，
以 C API 替身验证播放时间不叠加原始时间戳、文件加载通知及 shutdown 句柄释放。
这些测试不包含真实 libmpv 解码或后端联调。

Qt 测试报告保存在构建目录的 `token-auth-results.txt`、`playerpage-results.txt`、
`mpvplayer-results.txt`、`login-results.txt`。若受限环境中 CTest 批量启动后续进程超时，可以分别运行：

```powershell
ctest --test-dir client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug -R '^token_auth$' --output-on-failure
ctest --test-dir client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug -R '^playerpage$' --output-on-failure
ctest --test-dir client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug -R '^mpvplayer$' --output-on-failure
ctest --test-dir client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug -R '^login$' --output-on-failure
```

2026-09-16 验证时，SSH `dev@192.168.19.129:2222` 可用，但虚拟机本机
`127.0.0.1:9000` 连接失败，真实登录→受保护请求→注销链路未验证；服务端源码及鉴权配置未修改。

## 手动运行真实后端联调

当前 reference runtime 的网关端口是 `10000`。本机通过 `192.168.19.129:2222`
进入后端容器（容器地址 `172.18.0.10`），Windows 使用 SSH 隧道访问网关。
在后端启动已有基础设施及业务服务：

```bash
cd /home/dev/workspace/video-on-demand-server
make reference-infra-start
make dev-start-ms CMAKE_BUILD_DIR=/tmp/video-on-demand-cmake-build
```

在 Windows 建立隧道，并在客户端 `api.local.json` 中使用
`{ "baseUrl": "http://127.0.0.1:10000" }`。Qt Creator 从构建目录运行时，
还需将同一配置复制到 `player.exe` 旁边。

```powershell
ssh -N -o ExitOnForwardFailure=yes -o ServerAliveInterval=30 -p 2222 -L 127.0.0.1:10000:127.0.0.1:10000 dev@192.168.19.129
```

`live_backend_test` 编译真实 ApiClient、DataCenter 和 libmpv，覆盖登录、跨实例共享会话、
列表/详情、HTTP 媒体读取、无窗口播放与跳转、进度写入/读回/恢复、注销及旧 Token 的 401。
它只适用于开发环境，会临时修改指定账号的视频进度并恢复原值，不加入默认 CTest。

```powershell
$env:PATH = "D:\qt673\Tools\mingw1120_64\bin;D:\qt673\6.7.3\mingw_64\bin;" + $env:PATH
$env:VIDEO_API_BASE_URL = "http://127.0.0.1:10000"
$env:VOD_TEST_ACCOUNT = "bit-user-001"
$env:VOD_TEST_VIDEO_ID = "<已审核通过的 HTTP 视频 ID>"
$env:VOD_TEST_PASSWORD = [System.Net.NetworkCredential]::new("", (Read-Host "开发测试账号密码" -AsSecureString)).Password
cmake --build client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug --target live_backend_test -j 4
try {
    & client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug/live_backend_test.exe -o client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug/live-backend-results.txt,txt
} finally {
    Remove-Item Env:VOD_TEST_PASSWORD
}
```

如果选择其他视频，需确认它已经发布、播放地址为 HTTP(S)，且时长至少两秒。
Token 不会写入测试报告。

2026-10-05 已完成真实联调：Linux 主仓库 `8341f12` 的五个服务健康检查通过，
临时视频上传、转码 `SUCCEEDED` 和审核通过后，Windows 的 `live_backend_test`
验证了上述全部链路。测试进度已恢复，临时视频记录、原文件和本次转码缓存已清理。
验证结束时原有公开视频仍是本地 `D:/.../test.mp4` 地址，因此后续重复联调需另选或上传
一个已审核的 HTTP 视频。此次不是 Qt 界面点击验收。

### 真实 Qt 界面回归

`live_ui_test` 使用真实 Login、PlayerPage、ApiClient 和 libmpv，通过 QtTest 控件事件验证
密码登录、播放/暂停、拖动进度、关窗保存、重开续播和 EOF 后从头重播。它会短暂打开窗口，
只适用于开发账号；测试结束恢复原观看秒数并注销，不加入默认 CTest。
沿用上面的四项环境变量，指定可播放且至少四秒的视频后运行（不要设置 offscreen）：

```powershell
cmake --build client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug --target live_ui_test -j 4
$build = (Resolve-Path client/player/build/Desktop_Qt_6_7_3_MinGW_64_bit-Debug).Path
try {
    $test = Start-Process -FilePath "$build/live_ui_test.exe" -ArgumentList '-o', "$build/live-ui-results.txt,txt" -WindowStyle Hidden -PassThru -Wait
    Get-Content "$build/live-ui-results.txt"
    if ($test.ExitCode -ne 0) { throw "真实界面回归失败" }
} finally {
    Remove-Item Env:VOD_TEST_PASSWORD
}
```

2026-10-05 补充验证：`live_ui_test` 以开发种子视频 `video-002` 完成全部检查（3 passed / 0 failed），
原观看进度已恢复、会话已注销。本次视频由后端返回本地媒体路径，因此这项结果证明真实窗口和
播放状态协作；HTTP 媒体链路以上一节的临时上传测试为准。原生 Windows 窗口中还复现了
“播完后再点播放仍黑屏”的问题；重新编译后实点确认密码登录、注销、拖动到第 12 秒、
EOF 后重播均正常，重播画面恢复且进度从零增长到第 7 秒。空搜索及刷新恢复也通过。

若已用 `windeployqt` 将 Qt 部署在构建目录旁，本地 offscreen 回归还需要该目录的
`platforms/qoffscreen.dll`（来自当前 Qt 安装的 `plugins/platforms`）。
