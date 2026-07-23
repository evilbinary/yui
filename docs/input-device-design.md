# 输入设备抽象（对齐 Pointer，收敛 backend 轮询）

> 版本：v0.2  
> 日期：2026-07-23  
> 状态：**已落地**  
> 关联：`docs/mini-game-engine-design.md`、`PointerEvent` / `KeyEvent`、`src/event.h`

## 目标

1. **已从 `backend.h` 移除** `backend_key_down` / `backend_mouse_button_down`
2. 键鼠触屏走现有事件管线喂状态
3. `Game.input` 只读 **Input State**，不碰 SDL
4. event 支持注册全局 **pointer / key** 监听（Layer 树之外）

---

## 模型：事件进、状态出

```text
SDL / 触屏 / 宿主
    │
    ▼
backend：只翻译成 KeyEvent / PointerEvent
    │
    ▼
event.c：handle_key_event / handle_pointer_event
    │  1) 通知全局 listeners（不可消费事件）
    │  2) 再分发 Layer 树
    ▼
src/input/state.c     ← 注册为 pointer/key listener
    │
    ├─► UI（照旧走 Layer 事件）
    └─► Game.input / 其它系统（读 down/pressed/axis/pointer）
```

| 层 | 职责 |
|----|------|
| backend | 产生事件，**不**提供 `*_down` 轮询 |
| event | Layer 分发 + **全局 listener 注册** |
| input state | 维护 down / prev / pointer / buttons |
| Game | 只读 state |

---

## Event 全局监听 API

```c
typedef void (*PointerEventListener)(const PointerEvent* event);
typedef void (*KeyEventListener)(const KeyEvent* event);

int register_pointer_event_listener(PointerEventListener listener);
int unregister_pointer_event_listener(PointerEventListener listener);
int register_key_event_listener(KeyEventListener listener);
int unregister_key_event_listener(KeyEventListener listener);
```

- 在 `handle_*` **入口**、UI 分发前调用；pointer 仅在根 Layer 通知一次（避免递归重复）
- 监听器**不可消费**事件；不影响 Layer `handle_*` 语义

---

## `src/input/state`

```c
void input_state_init(void);          /* 注册 listeners */
void input_state_reset(void);
void input_state_begin_frame(void);   /* 边沿：pressed/released；prev ← down */

int  input_state_key_down(const char* name);
int  input_state_key_pressed(const char* name);
int  input_state_key_released(const char* name);
float input_state_axis(const char* name);

void input_state_pointer_pos(int* x, int* y);
int  input_state_pointer_button_down(int button);
int  input_state_pointer_button_pressed(int button);
```

`game/input.c` 为薄封装，不再依赖 backend 轮询。

键名由 `key_code` 映射：`Left/Right/Up/Down/Space/Return/Enter/Escape/A..Z/...`

---

## 帧时序

```text
PollEvent → handle_* → listeners → input_state_on_*（内部）
backend update callbacks
game_update → input_state_begin_frame() → 读 pressed/down
```

- **pressed** = 本帧 begin 时 `down && !prev`
- down 只由 Key/Pointer 事件置位/清除

---

## 触屏射击（V0）

仍只认鼠标左键 + 键 `F`；触屏射击以后用 HUD 按钮 Layer。
