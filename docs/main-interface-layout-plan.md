# 主界面布局方案

## 目标

主界面定位为桌面端视频点播首页。第一版优先做出稳定、可扩展、好看的信息架构，而不是一次性塞满业务逻辑。界面应支持后续接入影片列表、搜索、用户收藏、播放记录和详情页跳转。

参考图已放在：

`client/player/images/mainInterface/main-layout-reference.png`

## 页面结构

建议保持当前 `960 x 640` 的窗口基准尺寸，继续沿用无边框窗口和外层阴影。主界面内部拆为三层：

1. 外层窗口容器
   - 继续使用 `contentWidget` 作为白色主面板。
   - 外边距保持 `28px`，圆角建议从 `24px` 降到 `18px` 或 `20px`，更接近桌面工具质感。

2. 主工作区
   - 左侧导航栏：固定宽度 `176px`。
   - 中间内容区：自适应伸缩。
   - 右侧信息栏：固定宽度 `220px`，窗口小于 `900px` 时可以隐藏或移动到内容区下方。

3. 内容模块
   - 顶部工具栏：搜索、筛选、通知、用户入口。
   - 推荐横幅：当前主推影片/剧集。
   - 内容分区：继续观看、热门推荐、最新上线、收藏。
   - 右侧信息栏：播放进度、观看记录、下载/缓存状态。

## 推荐控件树

```text
player
└── contentWidget: QFrame
    └── rootLayout: QHBoxLayout
        ├── sidebarFrame: QFrame
        │   └── sidebarLayout: QVBoxLayout
        │       ├── logoArea: QWidget
        │       ├── navHomeButton: QPushButton
        │       ├── navMovieButton: QPushButton
        │       ├── navSeriesButton: QPushButton
        │       ├── navFavoriteButton: QPushButton
        │       ├── navDownloadButton: QPushButton
        │       └── userMiniPanel: QFrame
        ├── centerFrame: QFrame
        │   └── centerLayout: QVBoxLayout
        │       ├── topBarFrame: QFrame
        │       ├── heroFrame: QFrame
        │       ├── continueSection: QWidget
        │       ├── popularSection: QWidget
        │       └── newSection: QWidget
        └── rightPanelFrame: QFrame
            └── rightPanelLayout: QVBoxLayout
                ├── nowWatchingCard: QFrame
                ├── watchProgressList: QWidget
                └── cacheStatusCard: QFrame
```

## 视觉规范

主色不要做成单一深蓝或单一紫色。建议：

- 背景：`#F5F7FA`
- 主面板：`#FFFFFF`
- 左侧导航：`#111827`
- 主文字：`#111827`
- 次级文字：`#6B7280`
- 边框：`#E5E7EB`
- 强调青色：`#19B6C8`
- 强调珊瑚色：`#FF6B5A`
- 成功/进度色：`#2FBF71`

圆角建议：

- 外层面板：`18px - 20px`
- 卡片/海报：`8px`
- 按钮/输入框：`8px`

字体层级：

- 页面标题：`22px / 600`
- 分区标题：`16px / 600`
- 卡片标题：`13px - 14px / 600`
- 辅助信息：`11px - 12px / 400`

## 关键区域设计

### 左侧导航栏

导航栏承载全局入口，建议固定宽度，深色背景。顶部放产品名或简洁图标，中间是导航按钮，底部是用户状态。

第一版导航项：

- 首页
- 电影
- 剧集
- 收藏
- 下载
- 设置

按钮状态：

- 默认：透明背景，浅灰文字。
- 悬停：`rgba(255,255,255,0.08)`。
- 选中：青色左边指示条 + 半透明背景。

### 顶部工具栏

顶部工具栏不需要很高，建议 `48px`。左侧搜索框，右侧放筛选、通知、头像。

搜索框建议占中间内容区宽度的 `45% - 55%`，占位文案为 `搜索影片、剧集或演员`。

### 推荐横幅

横幅是首页第一视觉重点，建议高度 `150px - 176px`。左侧放影片信息，右侧可放海报/背景图。第一版没有真实影片数据时，可以使用渐变背景和占位海报，但不要做纯装饰。

横幅内容：

- 类型标签：如 `今日推荐`
- 标题
- 简介，两行以内
- 主按钮：立即播放
- 次按钮：加入收藏

### 内容卡片

视频卡片建议分两类：

- 横向继续观看卡：展示封面、标题、进度条、剩余时间。
- 竖向海报卡：展示封面、标题、评分、清晰度标签。

第一版可以每行放 `4` 张卡片，卡片宽度自适应，最小宽度 `112px`。

卡片交互：

- 悬停时轻微上移或边框高亮。
- 点击进入详情页或播放器页。
- 卡片右上角预留收藏按钮。

### 右侧信息栏

右侧信息栏用于让首页更像真实产品，而不是简单列表页。第一版放三个模块：

- 继续观看：显示当前最近一部影片和进度。
- 我的片单：显示收藏数量、待看数量。
- 缓存状态：显示下载中/已缓存数量。

## 数据模型建议

第一版可以先写静态数据，后续再替换为接口返回：

```cpp
struct VideoCardData {
    QString title;
    QString category;
    QString coverPath;
    QString duration;
    double rating;
    int progress;
};
```

后续可以扩展：

- `id`
- `description`
- `year`
- `episodeCount`
- `isFavorite`
- `playUrl`

## 实现顺序

1. 修复当前源文件中文编码，统一保存为 UTF-8。
2. 在 `player.ui` 中替换占位 `mainLabel`，先搭出三栏布局。
3. 新增样式入口，建议先放在 `player.cpp` 的 `initUI()` 中，后续再拆成 `.qss`。
4. 新增静态数据和卡片创建函数，例如 `createPosterCard()`、`createContinueCard()`。
5. 将参考图加入资源文件，或只作为设计参考保留在 `images/mainInterface`。
6. 跑一遍 Debug 构建，确认无边框、阴影、圆角、窗口尺寸都正常。

## 文件拆分建议

当前阶段可以先保守一点：

- `player.ui`：负责整体布局骨架。
- `player.cpp`：负责样式、静态数据、动态卡片创建。
- `player.h`：声明初始化函数和卡片工厂函数。

当卡片逻辑变多后再拆：

- `videocardwidget.h/.cpp`
- `sidebarwidget.h/.cpp`
- `homepagewidget.h/.cpp`

## 注意事项

- 当前代码里的中文注释显示为乱码，说明文件编码或读取编码不一致。正式改主界面前建议先统一为 UTF-8。
- `player.ui` 里 `<string>杩欐槸涓荤晫闈?/string>` 看起来已经破损，下一步改 UI 时应一并修掉。
- 首页第一版不建议直接接入复杂接口。先把布局、状态和卡片组件稳定下来，再接业务数据会顺很多。
