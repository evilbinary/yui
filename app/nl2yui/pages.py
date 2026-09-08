"""Common YUI page archetypes for NL→JSON data synthesis."""

from __future__ import annotations

import copy
import random
from typing import Any, Iterator

# Shared style tokens (Catppuccin-ish, matches existing seeds)
BG = "#1e1e2e"
FG = "#cdd6f4"
ACCENT = "#89b4fa"
OK = "#4caf50"
ERR = "#f44336"
MUTED = "#a6adc8"
BTN_FG = "#1e1e2e"


def _label(cid: str, text: str, *, size: int = 14, color: str = FG) -> dict[str, Any]:
    return {
        "id": cid,
        "type": "Label",
        "text": text,
        "style": {"color": color, "fontSize": size},
    }


def _button(
    cid: str,
    text: str,
    *,
    handler: str = "@onClick",
    bg: str = ACCENT,
    w: int = 120,
) -> dict[str, Any]:
    return {
        "id": cid,
        "type": "Button",
        "text": text,
        "size": [w, 36],
        "style": {"bgColor": bg, "color": BTN_FG, "borderRadius": 6},
        "events": {"onClick": handler},
    }


def _input(cid: str, placeholder: str, *, password: bool = False) -> dict[str, Any]:
    node: dict[str, Any] = {
        "id": cid,
        "type": "Input",
        "placeholder": placeholder,
        "size": [240, 36],
    }
    if password:
        node["password"] = True
    return node


def _view(cid: str, children: list[dict[str, Any]], *, layout: str = "vertical", spacing: int = 8) -> dict[str, Any]:
    return {
        "id": cid,
        "type": "View",
        "layout": {"type": layout, "spacing": spacing, "padding": [12]},
        "style": {"bgColor": BG},
        "children": children,
    }


def page_login() -> dict[str, Any]:
    return _view(
        "loginPage",
        [
            _label("loginTitle", "欢迎登录", size=20),
            _input("emailInput", "请输入邮箱"),
            _input("pwdInput", "请输入密码", password=True),
            _label("loginHint", "", size=12, color=ERR),
            _button("loginBtn", "登录", handler="@onLogin", w=240),
            _button("registerLink", "注册账号", handler="@onRegister", bg=MUTED, w=240),
        ],
        spacing=10,
    )


def page_settings() -> dict[str, Any]:
    return _view(
        "settingsPage",
        [
            _label("settingsTitle", "设置", size=18),
            _view(
                "themeRow",
                [
                    _label("themeLbl", "外观"),
                    _label("themeVal", "当前：深色"),
                    _button("themeToggleBtn", "切换", handler="@onToggleTheme", w=80),
                ],
                layout="horizontal",
                spacing=8,
            ),
            _view(
                "batteryRow",
                [
                    _label("batteryLbl", "电量"),
                    _label("batteryVal", "100%"),
                ],
                layout="horizontal",
                spacing=8,
            ),
            _label("aboutVer", "版本 v0.1", size=12, color=MUTED),
        ],
    )


def page_messages() -> dict[str, Any]:
    return _view(
        "messagesPage",
        [
            _label("msgTitle", "消息", size=18),
            _view(
                "msgRow0",
                [
                    _label("msg0Name", "项目组", size=14),
                    _label("msg0Preview", "明天见", size=12, color=MUTED),
                    _label("msg0Time", "09:30", size=12, color=MUTED),
                ],
                layout="horizontal",
                spacing=8,
            ),
            _label("msgStatus", "未读 1 条", size=12, color=ACCENT),
            _view(
                "msgThread",
                [
                    _label("msgThreadTitle", "项目组", size=16),
                    _label("msgThreadBody", "三点开会", size=14),
                ],
                spacing=6,
            ),
            _view(
                "msgActions",
                [
                    _button("msgReplyBtn", "回复", handler="@onReply", w=100),
                    _button("msgMarkBtn", "已读", handler="@onMarkRead", w=100),
                ],
                layout="horizontal",
                spacing=8,
            ),
        ],
    )


def page_form() -> dict[str, Any]:
    return _view(
        "formPage",
        [
            _label("formTitle", "资料填写", size=18),
            _label("nameLbl", "姓名"),
            _input("nameInput", "请输入姓名"),
            _label("emailLbl", "邮箱"),
            _input("emailInput", "请输入邮箱"),
            {
                "id": "agreeCb",
                "type": "Checkbox",
                "label": "同意用户协议",
                "data": False,
            },
            _label("formHint", "", size=12, color=ERR),
            _view(
                "formActions",
                [
                    _button("saveBtn", "保存", handler="@onSave", w=100),
                    _button("cancelBtn", "取消", handler="@onCancel", bg=MUTED, w=100),
                ],
                layout="horizontal",
                spacing=8,
            ),
        ],
    )


def page_chat() -> dict[str, Any]:
    return _view(
        "chatPage",
        [
            _label("chatTitle", "助手", size=18),
            _view(
                "chatThread",
                [
                    _label("chatUserMsg", "你：你好", size=14),
                    _label("chatBotMsg", "助手：有什么可以帮你？", size=14, color=ACCENT),
                ],
                spacing=6,
            ),
            _label("chatStatus", "就绪", size=12, color=MUTED),
            _view(
                "chatQuick",
                [
                    _button("chatWeatherBtn", "天气", handler="@onWeather", w=80),
                    _button("chatTimeBtn", "时间", handler="@onTime", w=80),
                ],
                layout="horizontal",
                spacing=8,
            ),
            _input("chatInput", "输入消息…"),
            _button("chatSendBtn", "发送", handler="@onSend", w=100),
        ],
    )


def page_launcher() -> dict[str, Any]:
    return {
        "id": "launcherPage",
        "type": "View",
        "layout": {"type": "vertical", "spacing": 10, "padding": [12]},
        "style": {"bgColor": BG},
        "children": [
            _label("launcherTitle", "应用", size=18),
            _label("launcherCount", "4 个应用", size=12, color=MUTED),
            {
                "id": "launcherGrid",
                "type": "Grid",
                "layout": {"type": "grid", "columns": 4, "gap": 10},
                "children": [
                    _button("appCalc", "计算", handler="@openCalc", w=72),
                    _button("appMsg", "消息", handler="@openMsg", w=72),
                    _button("appSet", "设置", handler="@openSet", w=72),
                    _button("appPhoto", "相册", handler="@openPhoto", w=72),
                ],
            },
        ],
    }


def page_loading_overlay() -> dict[str, Any]:
    return {
        "id": "screenRoot",
        "type": "View",
        "layout": {"type": "vertical", "spacing": 8, "padding": [12]},
        "style": {"bgColor": BG},
        "children": [
            _label("contentTitle", "内容区", size=16),
            _button("contentRefreshBtn", "刷新", handler="@onRefresh", w=100),
            {
                "id": "loadMask",
                "type": "View",
                "visible": False,
                "layout": {"type": "center", "padding": [12]},
                "style": {"bgColor": "#000000"},
                "opacity": 0.6,
                "children": [
                    {
                        "id": "overlayLoader",
                        "type": "Loading",
                        "size": [56, 56],
                        "variant": "spinner",
                        "text": "加载中...",
                        "color": ACCENT,
                        "trackColor": "#45475a",
                        "strokeWidth": 4,
                        "speed": 1.0,
                    }
                ],
            },
        ],
    }


def page_dialog() -> dict[str, Any]:
    return {
        "id": "dialogHost",
        "type": "View",
        "layout": {"type": "vertical", "spacing": 8, "padding": [12]},
        "style": {"bgColor": BG},
        "children": [
            _label("pageTitle", "文件", size=16),
            _button("openDialogBtn", "打开确认框", handler="@onOpenDialog", w=140),
            {
                "id": "confirmDialog",
                "type": "View",
                "visible": False,
                "layout": {"type": "vertical", "spacing": 10, "padding": [16]},
                "style": {"bgColor": "#313244", "borderRadius": 8},
                "children": [
                    _label("confirmTitle", "确认删除", size=16),
                    _label("confirmMsg", "此操作不可撤销", size=14, color=MUTED),
                    _view(
                        "confirmActions",
                        [
                            _button("confirmCancelBtn", "取消", handler="@onCancel", bg=MUTED, w=100),
                            _button("confirmOkBtn", "删除", handler="@onConfirm", bg=ERR, w=100),
                        ],
                        layout="horizontal",
                        spacing=8,
                    ),
                ],
            },
        ],
    }


def page_product() -> dict[str, Any]:
    return _view(
        "productPage",
        [
            _label("productTitle", "商品详情", size=18),
            _label("productName", "无线耳机", size=16),
            _label("productPrice", "¥299", size=20, color=ACCENT),
            _label("productStock", "库存 12", size=12, color=MUTED),
            {
                "id": "productProgress",
                "type": "Progress",
                "value": 40,
                "size": [240, 8],
            },
            _view(
                "productActions",
                [
                    _button("addCartBtn", "加入购物车", handler="@onAddCart", w=140),
                    _button("buyBtn", "立即购买", handler="@onBuy", bg=OK, w=120),
                ],
                layout="horizontal",
                spacing=8,
            ),
        ],
    )


def page_empty_state() -> dict[str, Any]:
    return _view(
        "emptyPage",
        [
            _label("emptyTitle", "暂无数据", size=18),
            _label("emptyHint", "点击下方按钮刷新", size=14, color=MUTED),
            _button("emptyRetryBtn", "重试", handler="@onRetry", w=100),
        ],
        spacing=12,
    )


PAGE_BUILDERS = {
    "login": page_login,
    "settings": page_settings,
    "messages": page_messages,
    "form": page_form,
    "chat": page_chat,
    "launcher": page_launcher,
    "loading": page_loading_overlay,
    "dialog": page_dialog,
    "product": page_product,
    "empty": page_empty_state,
}


def collect_ids(node: dict[str, Any]) -> list[tuple[str, str, Any]]:
    """Flatten id/type/text for context strings."""
    out: list[tuple[str, str, Any]] = []

    def walk(n: Any) -> None:
        if not isinstance(n, dict):
            return
        cid = n.get("id")
        ctype = n.get("type")
        if isinstance(cid, str) and isinstance(ctype, str):
            text = n.get("text")
            if text is None and n.get("placeholder"):
                text = n.get("placeholder")
            out.append((cid, ctype, text if isinstance(text, str) and text != "" else None))
        for child in n.get("children") or []:
            walk(child)

    walk(node)
    return out


def context_of(page: dict[str, Any], rng: random.Random | None = None, k: int | None = None) -> str:
    ids = collect_ids(page)
    if rng is not None and k is not None and k < len(ids):
        # Always keep root + sample others for shorter prompts
        root, rest = ids[0], ids[1:]
        sample = [root] + rng.sample(rest, k=min(k, len(rest)))
        ids = sample
    parts = []
    for cid, ctype, text in ids:
        parts.append(f"{cid}:{ctype}" + (f":{text}" if text is not None else ""))
    return ", ".join(parts)


def full_page_update(page: dict[str, Any]) -> dict[str, Any]:
    return {
        "updates": [
            {
                "target": "canvas",
                "change": {"children": [copy.deepcopy(page)]},
            }
        ]
    }


def page_scenarios(rng: random.Random) -> Iterator[dict[str, Any]]:
    """Yield NL→update examples grounded in page archetypes."""
    # --- full page creates ---
    name = rng.choice(list(PAGE_BUILDERS.keys()))
    page = PAGE_BUILDERS[name]()
    titles = {
        "login": ["做一个登录页", "生成登录界面，邮箱密码和登录按钮", "Create a login page"],
        "settings": ["做一个设置页", "生成设置界面，含外观和电量", "Create a settings page"],
        "messages": ["做一个消息列表页", "生成收件箱，带一条消息和详情", "Create a messages inbox page"],
        "form": ["做一个资料表单页", "生成表单：姓名邮箱协议和保存取消", "Create a profile form page"],
        "chat": ["做一个聊天助手页", "生成对话界面，带快捷按钮和发送", "Create a chat assistant page"],
        "launcher": ["做一个应用启动器网格", "生成四宫格应用列表", "Create an app launcher grid"],
        "loading": ["做一个带加载遮罩的页面", "生成内容页并带 Loading", "Create a page with loading overlay"],
        "dialog": ["做一个带确认对话框的页面", "生成删除确认弹层", "Create a page with confirm dialog"],
        "product": ["做一个商品详情页", "生成商品页含价格和购买按钮", "Create a product detail page"],
        "empty": ["做一个空状态页", "生成暂无数据提示和重试按钮", "Create an empty state page"],
    }
    yield {
        "mode": "full",
        "context": "(none)",
        "message": rng.choice(titles[name]),
        "output": full_page_update(page),
        "meta": {"page": name, "kind": "full"},
    }

    # --- login ops ---
    login = page_login()
    ctx = context_of(login, rng, 8)
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["把登录标题改成欢迎回来", "loginTitle 改成欢迎回来"]),
        "output": {"updates": [{"target": "loginTitle", "change": {"text": "欢迎回来"}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["邮箱占位改成请输入邮箱", "emailInput placeholder 设为请输入邮箱"]),
        "output": {"updates": [{"target": "emailInput", "change": {"placeholder": "请输入邮箱"}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["登录按钮改成登录中并禁用", "正在登录：按钮文案改登录中，禁用"]),
        "output": {
            "updates": [{"target": "loginBtn", "change": {"text": "登录中", "enabled": False}}]
        },
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["提示改成密码错误，红色", "登录失败提示密码错误"]),
        "output": {
            "updates": [{"target": "loginHint", "change": {"text": "密码错误", "color": ERR}}]
        },
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["隐藏注册入口", "不要显示注册账号"]),
        "output": {"updates": [{"target": "registerLink", "change": {"visible": None}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["登录成功：提示改成成功绿色，恢复登录按钮"]),
        "output": {
            "updates": [
                {"target": "loginHint", "change": {"text": "登录成功", "color": OK}},
                {"target": "loginBtn", "change": {"text": "登录", "enabled": True}},
            ]
        },
    }

    # --- settings ---
    settings = page_settings()
    ctx = context_of(settings, rng, 8)
    pct = rng.randint(5, 99)
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"电量改成 {pct}%", f"batteryVal 设为 {pct}%"]),
        "output": {"updates": [{"target": "batteryVal", "change": {"text": f"{pct}%"}}]},
    }
    theme = rng.choice(["浅色", "深色"])
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"外观说明改成当前：{theme}", f"主题显示当前：{theme}"]),
        "output": {"updates": [{"target": "themeVal", "change": {"text": f"当前：{theme}"}}]},
    }
    ver = f"v0.{rng.randint(1, 9)}"
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"关于版本改成 {ver}", f"版本号设为 {ver}"]),
        "output": {"updates": [{"target": "aboutVer", "change": {"text": f"版本 {ver}"}}]},
    }

    # --- messages ---
    messages = page_messages()
    ctx = context_of(messages, rng, 10)
    preview = rng.choice(["明天见", "收到，谢谢", "三点开会", "OK"])
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"第一条预览改成{preview}", f"msg0Preview 改成{preview}"]),
        "output": {"updates": [{"target": "msg0Preview", "change": {"text": preview}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["详情标题改成项目组，正文改成三点开会", "打开会话：标题项目组，内容三点开会"]),
        "output": {
            "updates": [
                {"target": "msgThreadTitle", "change": {"text": "项目组"}},
                {"target": "msgThreadBody", "change": {"text": "三点开会"}},
            ]
        },
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["状态改成已回复", "msgStatus 显示已回复"]),
        "output": {"updates": [{"target": "msgStatus", "change": {"text": "已回复"}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["清空会话区", "清空 msgThread 子节点"]),
        "output": {"updates": [{"target": "msgThread", "change": {"children": None}}]},
    }

    # --- form ---
    form = page_form()
    ctx = context_of(form, rng, 10)
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["姓名占位改成请输入姓名", "nameInput 占位符请输入姓名"]),
        "output": {"updates": [{"target": "nameInput", "change": {"placeholder": "请输入姓名"}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["禁用保存按钮", "先不能点保存"]),
        "output": {"updates": [{"target": "saveBtn", "change": {"enabled": False}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["提示改成请勾选协议，红色", "表单校验失败：请勾选协议"]),
        "output": {
            "updates": [{"target": "formHint", "change": {"text": "请勾选协议", "color": ERR}}]
        },
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["勾选同意协议", "agreeCb 设为已勾选"]),
        "output": {"updates": [{"target": "agreeCb", "change": {"data": True}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["保存按钮改成提交", "saveBtn 文案改提交"]),
        "output": {"updates": [{"target": "saveBtn", "change": {"text": "提交"}}]},
    }

    # --- chat ---
    chat = page_chat()
    ctx = context_of(chat, rng, 10)
    q = rng.choice(["今天天气怎么样", "现在几点", "帮我总结一下"])
    a = rng.choice(["晴，26°C", "下午 3 点", "好的，已整理要点"])
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"用户消息改成{q}", f"chatUserMsg 显示你：{q}"]),
        "output": {"updates": [{"target": "chatUserMsg", "change": {"text": f"你：{q}"}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"助手回复改成{a}", f"机器人说{a}"]),
        "output": {"updates": [{"target": "chatBotMsg", "change": {"text": f"助手：{a}"}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["状态改成正在思考…", "chatStatus 显示思考中"]),
        "output": {"updates": [{"target": "chatStatus", "change": {"text": "正在思考…"}}]},
    }
    new_id = f"chatBotMsg_{rng.randint(2, 99)}"
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["在会话区追加一条助手回复：已完成", "chatThread 加一条机器人消息已完成"]),
        "output": {
            "updates": [
                {
                    "target": "chatThread",
                    "change": {
                        "children": [
                            {
                                "id": new_id,
                                "type": "Label",
                                "text": "助手：已完成",
                                "style": {"color": ACCENT, "fontSize": 14},
                            }
                        ]
                    },
                }
            ]
        },
    }

    # --- launcher ---
    launcher = page_launcher()
    ctx = context_of(launcher, rng, 8)
    n_apps = rng.randint(3, 16)
    cols = rng.choice([3, 4])
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"应用数改成 {n_apps} 个应用", f"launcherCount 显示 {n_apps} 个应用"]),
        "output": {
            "updates": [{"target": "launcherCount", "change": {"text": f"{n_apps} 个应用"}}]
        },
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"网格改成 {cols} 列间距 8", f"launcherGrid {cols} 列"]),
        "output": {
            "updates": [
                {
                    "target": "launcherGrid",
                    "change": {"layout": {"type": "grid", "columns": cols, "gap": 8}},
                }
            ]
        },
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["清空应用网格", "清空 launcherGrid"]),
        "output": {"updates": [{"target": "launcherGrid", "change": {"children": None}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["网格里加一个天气应用按钮", "launcherGrid 追加天气"]),
        "output": {
            "updates": [
                {
                    "target": "launcherGrid",
                    "change": {
                        "children": [
                            _button("appWeather", "天气", handler="@openWeather", w=72),
                        ]
                    },
                }
            ]
        },
    }

    # --- loading ---
    loading = page_loading_overlay()
    ctx = context_of(loading, rng, 8)
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["显示加载遮罩", "打开 loadMask"]),
        "output": {"updates": [{"target": "loadMask", "change": {"visible": True}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["隐藏加载层", "关闭加载遮罩"]),
        "output": {"updates": [{"target": "loadMask", "change": {"visible": None}}]},
    }
    tip = rng.choice(["请稍候…", "正在同步…", "加载中..."])
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"加载文案改成{tip}", f"overlayLoader 文本 {tip}"]),
        "output": {"updates": [{"target": "overlayLoader", "change": {"text": tip}}]},
    }

    # --- dialog ---
    dialog = page_dialog()
    ctx = context_of(dialog, rng, 10)
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["显示确认对话框", "弹出 confirmDialog"]),
        "output": {"updates": [{"target": "confirmDialog", "change": {"visible": True}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["关闭对话框", "隐藏确认框"]),
        "output": {"updates": [{"target": "confirmDialog", "change": {"visible": None}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["确定按钮改成删除，背景红", "确认按钮改成危险删除样式"]),
        "output": {
            "updates": [{"target": "confirmOkBtn", "change": {"text": "删除", "bgColor": ERR}}]
        },
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["隐藏取消按钮", "不要显示取消"]),
        "output": {"updates": [{"target": "confirmCancelBtn", "change": {"visible": None}}]},
    }

    # --- product ---
    product = page_product()
    ctx = context_of(product, rng, 8)
    price = rng.choice([99, 199, 299, 599])
    stock = rng.randint(0, 50)
    prog = rng.randint(0, 100)
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"价格改成 ¥{price}", f"productPrice 设为 ¥{price}"]),
        "output": {"updates": [{"target": "productPrice", "change": {"text": f"¥{price}"}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"库存改成库存 {stock}", f"productStock 显示库存 {stock}"]),
        "output": {"updates": [{"target": "productStock", "change": {"text": f"库存 {stock}"}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice([f"进度条设为 {prog}%", f"productProgress {prog}"]),
        "output": {"updates": [{"target": "productProgress", "change": {"value": prog}}]},
    }
    if stock == 0:
        yield {
            "mode": "update",
            "context": ctx,
            "message": "售罄：禁用立即购买，库存文案改成售罄",
            "output": {
                "updates": [
                    {"target": "productStock", "change": {"text": "售罄", "color": ERR}},
                    {"target": "buyBtn", "change": {"enabled": False}},
                ]
            },
        }

    # --- empty ---
    empty = page_empty_state()
    ctx = context_of(empty)
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["空状态标题改成网络异常", "emptyTitle 改成网络异常"]),
        "output": {"updates": [{"target": "emptyTitle", "change": {"text": "网络异常"}}]},
    }
    yield {
        "mode": "update",
        "context": ctx,
        "message": rng.choice(["提示改成请检查网络后重试", "emptyHint 文案更新"]),
        "output": {
            "updates": [{"target": "emptyHint", "change": {"text": "请检查网络后重试"}}]
        },
    }
