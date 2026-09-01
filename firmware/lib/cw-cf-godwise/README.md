# Godwise 人物表盘（cw-cf-godwise）

根据你的手绘女孩头像定制的 64×64 表盘。

## 布局

- 人物下移，顶部留出米色时间区（不再挡住胸口）
- 时间：顶部居中圆角胶囊 `HH:MM`，桃色描边

## 动画（已写进 Clockface.cpp）

| 动画 | 效果 |
|------|------|
| 眨眼 | 约每 3–5 秒闭眼 ~140ms |
| 腮红呼吸 | 脸颊粉红强弱缓变 |
| 冒号闪烁 | 每 0.5 秒 |
| 换分弹跳 | 分钟变化时时间胶囊上跳再回落 |

## 预览图

- `preview_with_time.png` — 当前布局
- `preview_anim_storyboard.png` — idle / blink / minute+
- `preview_blink.png` — 眨眼单帧

改参考图或布局后运行：

```powershell
python "C:\Users\yfh\Desktop\时钟\gen_portrait_assets.py"
```

确认效果后再编译烧录。
