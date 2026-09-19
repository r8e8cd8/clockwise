package com.godwise.remote;

import android.util.Base64;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.TypedValue;
import android.view.GestureDetector;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.HorizontalScrollView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.ImageAnalysis;
import androidx.camera.core.ImageProxy;
import androidx.camera.core.Preview;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.camera.video.FileOutputOptions;
import androidx.camera.video.FallbackStrategy;
import androidx.camera.video.Quality;
import androidx.camera.video.QualitySelector;
import androidx.camera.video.Recorder;
import androidx.camera.video.Recording;
import androidx.camera.video.VideoCapture;
import androidx.camera.video.VideoRecordEvent;
import androidx.camera.view.PreviewView;
import androidx.core.content.ContextCompat;

import com.google.common.util.concurrent.ListenableFuture;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileWriter;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.charset.StandardCharsets;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity {
    private static final String DEFAULT_HOST = "http://clockwise.local";

    private final ExecutorService net = Executors.newSingleThreadExecutor();
    private final ExecutorService camExec = Executors.newSingleThreadExecutor();
    private final ExecutorService bgNet = Executors.newSingleThreadExecutor();
    private final ExecutorService uploadExec = Executors.newSingleThreadExecutor();
    private final Set<String> uploading = Collections.synchronizedSet(new HashSet<>());
    private HandGestures hands;

    private static final String[] SCENE_NAMES = {
            "传送寿司", "街机", "珊瑚拱", "浮空城", "迪斯科", "奶茶台", "糖果塔", "星球穹",
            "红包墙", "水晶井", "烟花港", "纸鹤湖", "幽灵巷", "蜂巢墙", "冰尖", "地铁壁画",
            "雨玻璃", "悲喜剧", "锦鲤桥", "拉面帘", "马戏团", "机器巷", "闪电", "画室",
            "齿轮钟", "温室", "盆栽园"
    };

    private EditText hostBox;
    private TextView status;
    private TextView gesture;
    private PreviewView preview;
    private SkeletonView bones;
    private ImageView bigPreview;
    private ImageView ledView;
    private TextView sceneLabel;
    private HorizontalScrollView strip;
    private LinearLayout thumbRow;
    private Bitmap[] sceneBitmaps;
    private ImageView[] thumbs;
    private int sceneCount;
    private int sceneIdx;
    private boolean scenesLoading;
    private static final long SEGMENT_MS = 30_000;
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private VideoCapture<Recorder> videoCapture;
    private Recording activeRecording;
    private boolean recordWanted;
    private boolean clipStarting;
    private int clipFailures;
    private File currentClip;
    private ProcessCameraProvider cameraProvider;
    private long coolUntil;
    private int mins = 5;
    private int interval = 30;
    private boolean cameraWanted;
    private final StringBuilder bubble = new StringBuilder();
    private TextView bubblePreview;

    private final ActivityResultLauncher<String> cameraPermission =
            registerForActivityResult(new ActivityResultContracts.RequestPermission(), granted -> {
                if (granted && cameraWanted) {
                    openCamera();
                } else if (!granted) {
                    gesture.setText("没有摄像头权限，请用下方按钮");
                }
            });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        LinearLayout root = findViewById(R.id.root);
        buildUi(root);
        mainHandler.post(this::refreshLed);
    }

    @Override
    protected void onDestroy() {
        recordWanted = false;
        stopClip();
        mainHandler.removeCallbacks(this::refreshLed);
        super.onDestroy();
        if (cameraProvider != null) {
            cameraProvider.unbindAll();
        }
        if (hands != null) {
            hands.close();
            hands = null;
        }
        net.shutdownNow();
        camExec.shutdownNow();
        bgNet.shutdownNow();
    }

    private void buildUi(LinearLayout root) {
        TextView title = text("遥控", 28, Color.parseColor("#1A2430"), true);
        status = text("连同一 WiFi 后即可用", 13, Color.parseColor("#5A6B7A"), false);
        root.addView(title);
        root.addView(status);

        hostBox = new EditText(this);
        hostBox.setText(DEFAULT_HOST);
        hostBox.setSingleLine(true);
        hostBox.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        hostBox.setPadding(dp(14), dp(12), dp(14), dp(12));
        hostBox.setBackground(round(Color.WHITE, dp(12)));
        LinearLayout.LayoutParams hostLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        hostLp.topMargin = dp(12);
        hostLp.bottomMargin = dp(12);
        hostBox.setLayoutParams(hostLp);
        root.addView(hostBox);

        LinearLayout ledCard = card(root, "LED 画面");
        ledView = new ImageView(this);
        ledView.setBackgroundColor(Color.parseColor("#0F1419"));
        ledView.setScaleType(ImageView.ScaleType.FIT_CENTER);
        LinearLayout.LayoutParams ledLp = new LinearLayout.LayoutParams(dp(168), dp(168));
        ledLp.gravity = Gravity.CENTER_HORIZONTAL;
        ledLp.bottomMargin = dp(6);
        ledView.setLayoutParams(ledLp);
        ledCard.addView(ledView);
        ledCard.addView(text("每 5 秒读取时钟当前画面", 12, Color.parseColor("#5A6B7A"), false));

        LinearLayout camCard = card(root, "摄像头手势");
        preview = new PreviewView(this);
        preview.setScaleType(PreviewView.ScaleType.FIT_CENTER);
        preview.setBackgroundColor(Color.parseColor("#0F1419"));
        bones = new SkeletonView(this);
        FrameLayout stage = new FrameLayout(this);
        stage.setClipChildren(true);
        LinearLayout.LayoutParams prevLp = new LinearLayout.LayoutParams(dp(168), dp(168));
        prevLp.gravity = Gravity.CENTER_HORIZONTAL;
        prevLp.bottomMargin = dp(8);
        stage.setLayoutParams(prevLp);
        FrameLayout.LayoutParams fill = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        preview.setLayoutParams(fill);
        bones.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        // Show only the center of the camera. The saved file is the full raw frame.
        preview.setScaleX(1.8f);
        preview.setScaleY(1.8f);
        bones.setScaleX(1.8f);
        bones.setScaleY(1.8f);
        stage.addView(preview);
        stage.addView(bones);
        camCard.addView(stage);
        row(camCard,
                button("开启摄像头", 0xFFD8F0E2, 0xFF2F7A4E, v -> toggleCamera()),
                button("测试连接", 0xFFDCE9F7, 0xFF2C5F9E, v -> ping()));
        gesture = text("手势关闭 · 按钮可直接用", 14, Color.parseColor("#2A7D8C"), true);
        gesture.setPadding(0, dp(8), 0, 0);
        camCard.addView(gesture);
        TextView hint = text("绿线是手的骨架，指尖是粉色\n握拳 / 食指顺时针 / 滑动 → 下一张\n食指逆时针 → 上一张\n张掌 → 眨眼    剪刀手 → 单眼眨    三指 → 小惊喜\n比心 / 我爱你 → 爱心    点赞 → 偷看\n拇指向下 → 瞌睡    食指竖起 → 呼唤", 12, Color.parseColor("#5A6B7A"), false);
        hint.setPadding(0, dp(6), 0, 0);
        camCard.addView(hint);
        buildBgCard(root);

        LinearLayout day = card(root, "白天 / 黑夜");
        row(day,
                button("白天", 0xFFF7E8B8, 0xFF9A6B12, v -> poke("dn", "day")),
                button("黑夜", 0xFFD8DFF5, 0xFF3D4F8A, v -> poke("dn", "night")));
        day.addView(button("跟随时间", 0xFFEEF3F6, 0xFF5A6B7A, v -> poke("dn", "auto")));

        LinearLayout fav = card(root, "背景收藏");
        row(fav,
                button("收藏当前", 0xFFF5DCE6, 0xFF9A3D5C, v -> poke("fav", "toggle")),
                button("下一张收藏", 0xFFDCE9F7, 0xFF2C5F9E, v -> poke("fav", "next")));
        row(fav,
                button("只播收藏", 0xFFD8F0E2, 0xFF2F7A4E, v -> poke("fav", "only")),
                button("播放全部", 0xFFEEF3F6, 0xFF5A6B7A, v -> poke("fav", "all")));

        LinearLayout move = card(root, "人物进出");
        row(move,
                button("请出去 →", 0xFFEEF3F6, 0xFF5A6B7A, v -> poke("bye", "right")),
                button("← 请出去", 0xFFEEF3F6, 0xFF5A6B7A, v -> poke("bye", "left")));
        row(move,
                button("请回来", 0xFFD8F0E2, 0xFF2F7A4E, v -> poke("back", null)),
                button("呼唤", 0xFFF5EBC8, 0xFF8A6A18, v -> poke("call", null)));

        LinearLayout dial = card(root, "表盘");
        row(dial,
                button("显示秒针", 0xFFDCE9F7, 0xFF2C5F9E, v -> poke("sec", "on")),
                button("隐藏秒针", 0xFFEEF3F6, 0xFF5A6B7A, v -> poke("sec", "off")));

        LinearLayout auto = card(root, "自动状态");
        auto.addView(chips(new int[]{5, 10, 30, 60}, true));
        auto.addView(chips(new int[]{15, 30, 60}, false));
        row(auto,
                button("睡觉", 0xFFD4EEF2, 0xFF2A7D8C, v -> lock("sleep")),
                button("害羞", 0xFFF5DCE6, 0xFF9A3D5C, v -> lock("shy")));
        row(auto,
                button("比心", 0xFFD8F0E2, 0xFF2F7A4E, v -> lock("heart")),
                button("偷看", 0xFFEBE6F5, 0xFF5C4D8A, v -> lock("peek")));
        auto.addView(button("取消自动", 0xFFEEF3F6, 0xFF5A6B7A, v -> lock("none")));

        LinearLayout now = card(root, "瞬间");
        row(now,
                button("眨眼", 0xFFEEF3F6, 0xFF1A2430, v -> poke("blink", null)),
                button("单眼眨", 0xFFEBE6F5, 0xFF5C4D8A, v -> poke("wink", null)));
        row(now,
                button("偷看", 0xFFEBE6F5, 0xFF5C4D8A, v -> poke("peek", null)),
                button("害羞", 0xFFF5DCE6, 0xFF9A3D5C, v -> poke("shy", null)));
        row(now,
                button("比心", 0xFFD8F0E2, 0xFF2F7A4E, v -> poke("heart", null)),
                button("瞌睡", 0xFFD4EEF2, 0xFF2A7D8C, v -> poke("sleep", null)));
        row(now,
                button("小惊喜", 0xFFF5EBC8, 0xFF8A6A18, v -> poke("surprise", null)),
                button("换背景", 0xFFDCE9F7, 0xFF2C5F9E, v -> poke("next", null)));

        LinearLayout say = card(root, "气泡");
        bubblePreview = text("", 20, Color.parseColor("#1A2430"), true);
        bubblePreview.setGravity(Gravity.CENTER);
        bubblePreview.setMinHeight(dp(32));
        say.addView(bubblePreview);
        String letters = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        for (int i = 0; i < letters.length(); i += 7) {
            LinearLayout line = new LinearLayout(this);
            line.setOrientation(LinearLayout.HORIZONTAL);
            int end = Math.min(i + 7, letters.length());
            for (int j = i; j < end; j++) {
                final char letter = letters.charAt(j);
                Button b = button(String.valueOf(letter), 0xFFFFFFFF, 0xFF1A2430, v -> appendLetter(letter));
                b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
                b.setMinWidth(0);
                b.setMinimumWidth(0);
                b.setPadding(0, dp(4), 0, dp(4));
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
                lp.setMargins(dp(2), dp(2), dp(2), dp(2));
                b.setLayoutParams(lp);
                line.addView(b);
            }
            say.addView(line);
        }
        row(say,
                button("退格", 0xFFEEF3F6, 0xFF5A6B7A, v -> backspaceLetter()),
                button("发送", 0xFFD8F0E2, 0xFF2F7A4E, v -> sendBubble()));
    }

    private void appendLetter(char ch) {
        if (bubble.length() >= 10) return;
        bubble.append(ch);
        bubblePreview.setText(bubble.toString());
    }

    private void backspaceLetter() {
        if (bubble.length() == 0) return;
        bubble.deleteCharAt(bubble.length() - 1);
        bubblePreview.setText(bubble.toString());
    }

    private void sendBubble() {
        if (bubble.length() == 0) return;
        poke("say", bubble.toString());
    }

    private void toggleCamera() {
        if (cameraProvider != null) {
            recordWanted = false;
            stopClip();
            cameraProvider.unbindAll();
            cameraProvider = null;
            cameraWanted = false;
            if (bones != null) bones.clearHand();
            gesture.setText("手势已关闭");
            return;
        }
        cameraWanted = true;
        if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.CAMERA)
                == android.content.pm.PackageManager.PERMISSION_GRANTED) {
            openCamera();
        } else {
            cameraPermission.launch(android.Manifest.permission.CAMERA);
        }
    }

    private void openCamera() {
        gesture.setText("正在打开摄像头…");
        ListenableFuture<ProcessCameraProvider> future = ProcessCameraProvider.getInstance(this);
        future.addListener(() -> {
            try {
                cameraProvider = future.get();
                Preview previewUse = new Preview.Builder().build();
                previewUse.setSurfaceProvider(preview.getSurfaceProvider());
                ImageAnalysis analysis = new ImageAnalysis.Builder()
                        .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                        .setOutputImageFormat(ImageAnalysis.OUTPUT_IMAGE_FORMAT_RGBA_8888)
                        .setTargetResolution(new android.util.Size(640, 480))
                        .build();
                analysis.setAnalyzer(camExec, this::analyze);
                Recorder recorder = new Recorder.Builder()
                        .setQualitySelector(QualitySelector.fromOrderedList(
                                Arrays.asList(Quality.LOWEST, Quality.SD),
                                FallbackStrategy.lowerQualityOrHigherThan(Quality.SD)))
                        .build();
                videoCapture = VideoCapture.withOutput(recorder);
                cameraProvider.unbindAll();
                try {
                    cameraProvider.bindToLifecycle(
                            this, CameraSelector.DEFAULT_FRONT_CAMERA, previewUse, analysis, videoCapture);
                    recordWanted = true;
                    uploadPending();
                    startClip();
                } catch (Exception bindAll) {
                    videoCapture = null;
                    cameraProvider.unbindAll();
                    cameraProvider.bindToLifecycle(
                            this, CameraSelector.DEFAULT_FRONT_CAMERA, previewUse, analysis);
                }
                gesture.setText("手势待机 · 看绿线骨架是否跟上");
            } catch (Exception e) {
                cameraProvider = null;
                gesture.setText("摄像头打开失败，请用按钮");
            }
        }, ContextCompat.getMainExecutor(this));
    }

    private void startClip() {
        if (!recordWanted || videoCapture == null || clipStarting || activeRecording != null) return;
        File dir = new File(getExternalFilesDir(Environment.DIRECTORY_MOVIES), "debug");
        if (!dir.exists() && !dir.mkdirs()) return;
        String stamp = new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(new Date());
        File clip = new File(dir, stamp + ".mp4");
        currentClip = clip;
        clipStarting = true;
        FileOutputOptions options = new FileOutputOptions.Builder(clip).build();
        // Camera stream only. Skeleton lines are a view overlay and are not in this file.
        try {
            activeRecording = videoCapture.getOutput()
                    .prepareRecording(this, options)
                    .start(ContextCompat.getMainExecutor(this), event -> {
                        if (!(event instanceof VideoRecordEvent.Finalize)) return;
                        activeRecording = null;
                        int err = ((VideoRecordEvent.Finalize) event).getError();
                        if (err == VideoRecordEvent.Finalize.ERROR_NONE) {
                            clipFailures = 0;
                            uploadClip(clip);
                            if (recordWanted) mainHandler.post(this::startClip);
                        } else if (recordWanted && clipFailures < 2) {
                            clipFailures++;
                            mainHandler.postDelayed(this::startClip, 1500);
                        }
                    });
        } catch (Exception e) {
            activeRecording = null;
            return;
        } finally {
            clipStarting = false;
        }
        mainHandler.removeCallbacks(this::rotateClip);
        mainHandler.postDelayed(this::rotateClip, SEGMENT_MS);
    }

    private void rotateClip() {
        Recording recording = activeRecording;
        if (recording == null || clipStarting) return;
        recording.stop();
    }

    private void stopClip() {
        mainHandler.removeCallbacks(this::startClip);
        mainHandler.removeCallbacks(this::rotateClip);
        Recording recording = activeRecording;
        activeRecording = null;
        clipStarting = false;
        if (recording != null) recording.stop();
    }

    private void uploadPending() {
        File dir = new File(getExternalFilesDir(Environment.DIRECTORY_MOVIES), "debug");
        File[] files = dir.listFiles();
        if (files == null) return;
        for (File file : files) {
            if (file.getName().endsWith(".mp4") && !file.equals(currentClip)) uploadClip(file);
        }
    }

    private void uploadClip(File file) {
        if (file == null || BuildConfig.GH_TOKEN.isEmpty() || !file.exists() || file.length() == 0) return;
        if (!uploading.add(file.getName())) return;
        uploadExec.execute(() -> {
            try {
                putGithub("clips/" + file.getName(), readAll(file));
                file.delete();
            } catch (Exception ignored) {
            } finally {
                uploading.remove(file.getName());
            }
        });
    }

    private static byte[] readAll(File file) throws java.io.IOException {
        try (FileInputStream in = new FileInputStream(file);
             ByteArrayOutputStream out = new ByteArrayOutputStream()) {
            byte[] buf = new byte[8192];
            int n;
            while ((n = in.read(buf)) >= 0) out.write(buf, 0, n);
            return out.toByteArray();
        }
    }

    private static void putGithub(String path, byte[] bytes) throws java.io.IOException {
        String body = "{\"message\":\"clip\",\"content\":\""
                + Base64.encodeToString(bytes, Base64.NO_WRAP) + "\"}";
        byte[] payload = body.getBytes(StandardCharsets.UTF_8);
        HttpURLConnection conn = (HttpURLConnection) new URL(
                "https://api.github.com/repos/" + BuildConfig.GH_REPO + "/contents/" + path).openConnection();
        conn.setConnectTimeout(20000);
        conn.setReadTimeout(180000);
        conn.setRequestMethod("PUT");
        conn.setDoOutput(true);
        conn.setFixedLengthStreamingMode(payload.length);
        conn.setRequestProperty("Authorization", "Bearer " + BuildConfig.GH_TOKEN);
        conn.setRequestProperty("Accept", "application/vnd.github+json");
        conn.setRequestProperty("Content-Type", "application/json");
        conn.setRequestProperty("User-Agent", "godwise-remote");
        try (OutputStream os = conn.getOutputStream()) {
            os.write(payload);
        }
        int code = conn.getResponseCode();
        conn.disconnect();
        if (code != 200 && code != 201) throw new java.io.IOException("github " + code);
    }

    private void noteGesture(String name) {
        if (currentClip == null) return;
        File log = new File(currentClip.getParentFile(), "gestures.log");
        String line = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.US).format(new Date())
                + "\t" + currentClip.getName() + "\t" + name + "\n";
        camExec.execute(() -> {
            try (FileWriter w = new FileWriter(log, true)) {
                w.write(line);
            } catch (Exception ignored) {
            }
        });
    }

    private void analyze(@NonNull ImageProxy image) {
        Bitmap raw = null;
        Bitmap upright = null;
        try {
            if (image.getPlanes().length == 0) return;
            int w = image.getWidth();
            int h = image.getHeight();
            if (w <= 0 || h <= 0) return;
            raw = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888);
            ByteBuffer buffer = image.getPlanes()[0].getBuffer();
            buffer.rewind();
            raw.copyPixelsFromBuffer(buffer);
            int rotation = image.getImageInfo().getRotationDegrees();
            if (hands == null) hands = new HandGestures(this);
            upright = HandGestures.uprightFront(raw, rotation);
            HandGestures.Hit hit = hands.recognize(upright);
            runOnUiThread(() -> {
                if (bones == null) return;
                if (hit.nx == null) bones.clearHand();
                else bones.show(hit.nx, hit.ny, hit.imageW, hit.imageH);
            });
            if (hit.action == null && hit.seen == null) return;
            if ("prev".equals(hit.action)) {
                switchScene(-1, hit.name);
            } else if (hit.action != null) {
                fire(hit.name, hit.action, null);
            } else if (hit.seen != null) {
                runOnUiThread(() -> gesture.setText("看到：" + hit.seen));
            }
        } catch (Exception e) {
            runOnUiThread(() -> gesture.setText("手势模型未就绪"));
        } finally {
            if (raw != null) raw.recycle();
            if (upright != null && upright != raw) upright.recycle();
            image.close();
        }
    }

    private void switchScene(int delta, String name) {
        long now = SystemClock.elapsedRealtime();
        if (now < coolUntil) return;
        coolUntil = now + 1600;
        runOnUiThread(() -> {
            gesture.setText("识别到：" + name);
            noteGesture(name);
            if (sceneCount > 0) showScene(sceneIdx + delta, true);
            else poke("next", null);
        });
    }

    private void fire(String name, String action, String extra) {
        long now = SystemClock.elapsedRealtime();
        if (now < coolUntil) return;
        coolUntil = now + 1600;
        runOnUiThread(() -> {
            gesture.setText("识别到：" + name);
            noteGesture(name);
            poke(action, extra);
        });
    }

    private void buildBgCard(LinearLayout root) {
        LinearLayout card = card(root, "背景预览");
        bigPreview = new ImageView(this);
        bigPreview.setBackgroundColor(Color.parseColor("#0F1419"));
        bigPreview.setScaleType(ImageView.ScaleType.FIT_CENTER);
        LinearLayout.LayoutParams bigLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(220));
        bigLp.bottomMargin = dp(8);
        bigPreview.setLayoutParams(bigLp);
        GestureDetector detector = new GestureDetector(this, new GestureDetector.SimpleOnGestureListener() {
            @Override
            public boolean onDown(MotionEvent e) {
                return true;
            }

            @Override
            public boolean onFling(MotionEvent e1, MotionEvent e2, float velocityX, float velocityY) {
                if (sceneCount <= 0 || Math.abs(velocityX) < 180) return false;
                showScene(sceneIdx + (velocityX < 0 ? 1 : -1), true);
                return true;
            }
        });
        bigPreview.setOnTouchListener((v, event) -> detector.onTouchEvent(event));
        card.addView(bigPreview);

        sceneLabel = text("滑动大图切换，点缩略图选用", 13, Color.parseColor("#2A7D8C"), true);
        sceneLabel.setPadding(0, 0, 0, dp(8));
        card.addView(sceneLabel);

        strip = new HorizontalScrollView(this);
        strip.setHorizontalScrollBarEnabled(false);
        thumbRow = new LinearLayout(this);
        thumbRow.setOrientation(LinearLayout.HORIZONTAL);
        strip.addView(thumbRow);
        strip.setOnScrollChangeListener((v, scrollX, scrollY, oldX, oldY) -> previewCenteredThumb(scrollX));
        card.addView(strip);
        card.addView(button("加载背景", 0xFFD4EEF2, 0xFF2A7D8C, v -> loadScenes()));
    }

    private void refreshLed() {
        final String host = hostText();
        bgNet.execute(() -> {
            Bitmap bmp = readBmp(host + "/screen");
            Bitmap show = null;
            if (bmp != null) {
                int px = Math.max(dp(168), 64);
                show = Bitmap.createScaledBitmap(bmp, px, px, false);
                if (show != bmp) bmp.recycle();
            }
            final Bitmap frame = show;
            runOnUiThread(() -> {
                if (ledView != null && frame != null) ledView.setImageBitmap(frame);
                if (!isFinishing()) mainHandler.postDelayed(this::refreshLed, 5000);
            });
        });
    }

    private void loadScenes() {
        if (scenesLoading) return;
        scenesLoading = true;
        sceneLabel.setText("正在从时钟读取背景…");
        final String host = hostText();
        bgNet.execute(() -> {
            try {
                String meta = readText(host + "/bgs");
                int comma = meta == null ? -1 : meta.indexOf(',');
                if (comma < 0) {
                    runOnUiThread(() -> sceneLabel.setText("读不到背景，先烧录新固件再试"));
                    return;
                }
                int count = Integer.parseInt(meta.substring(0, comma).trim());
                int current = Integer.parseInt(meta.substring(comma + 1).trim());
                if (count <= 0 || count > 64) count = 54;
                if (current < 0 || current >= count) current = 0;
                final int n = count;
                final int cur = current;
                runOnUiThread(() -> prepareThumbs(n, cur));
                for (int i = 0; i < n; i++) {
                    Bitmap raw = readBmp(host + "/bg?i=" + i);
                    if (raw == null) continue;
                    Bitmap scaled = Bitmap.createScaledBitmap(raw, 192, 192, false);
                    if (scaled != raw) raw.recycle();
                    final int idx = i;
                    final Bitmap show = scaled;
                    final int total = n;
                    runOnUiThread(() -> {
                        if (sceneBitmaps != null && idx < sceneBitmaps.length) sceneBitmaps[idx] = show;
                        if (thumbs != null && idx < thumbs.length && thumbs[idx] != null) {
                            thumbs[idx].setImageBitmap(show);
                        }
                        if (idx == sceneIdx && bigPreview != null) bigPreview.setImageBitmap(show);
                        if (sceneLabel != null) sceneLabel.setText("已加载 " + (idx + 1) + "/" + total);
                    });
                }
                runOnUiThread(() -> {
                    if (sceneLabel != null) sceneLabel.setText(sceneCaption(false));
                });
            } catch (Exception e) {
                runOnUiThread(() -> sceneLabel.setText("加载失败，确认同一 WiFi"));
            } finally {
                scenesLoading = false;
            }
        });
    }

    private void prepareThumbs(int count, int current) {
        sceneCount = count;
        sceneIdx = current;
        sceneBitmaps = new Bitmap[count];
        thumbs = new ImageView[count];
        thumbRow.removeAllViews();
        for (int i = 0; i < count; i++) {
            ImageView iv = new ImageView(this);
            iv.setScaleType(ImageView.ScaleType.FIT_CENTER);
            iv.setBackgroundColor(Color.parseColor("#0F1419"));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(dp(72), dp(72));
            lp.setMargins(0, 0, dp(8), dp(8));
            iv.setLayoutParams(lp);
            final int idx = i;
            iv.setOnClickListener(v -> showScene(idx, true));
            thumbs[i] = iv;
            thumbRow.addView(iv);
        }
        highlightThumb(current);
        sceneLabel.setText(sceneCaption(false));
    }

    private void previewCenteredThumb(int scrollX) {
        if (thumbs == null || strip == null || sceneCount <= 0) return;
        int center = scrollX + strip.getWidth() / 2;
        int best = sceneIdx;
        int bestDist = Integer.MAX_VALUE;
        for (int i = 0; i < thumbs.length; i++) {
            if (thumbs[i] == null) continue;
            int mid = thumbs[i].getLeft() + thumbs[i].getWidth() / 2;
            int dist = Math.abs(mid - center);
            if (dist < bestDist) {
                bestDist = dist;
                best = i;
            }
        }
        if (best == sceneIdx) return;
        sceneIdx = best;
        if (sceneBitmaps != null && best < sceneBitmaps.length && sceneBitmaps[best] != null) {
            bigPreview.setImageBitmap(sceneBitmaps[best]);
        }
        highlightThumb(best);
        sceneLabel.setText(sceneCaption(false));
    }

    private void showScene(int index, boolean apply) {
        if (sceneCount <= 0) return;
        if (index < 0) index = sceneCount - 1;
        if (index >= sceneCount) index = 0;
        sceneIdx = index;
        if (sceneBitmaps != null && sceneBitmaps[index] != null) {
            bigPreview.setImageBitmap(sceneBitmaps[index]);
        }
        highlightThumb(index);
        if (strip != null && thumbs != null && thumbs[index] != null) {
            strip.smoothScrollTo(Math.max(0, thumbs[index].getLeft() - dp(16)), 0);
        }
        sceneLabel.setText(sceneCaption(apply));
        if (apply) poke("bg", String.valueOf(index));
    }

    private void highlightThumb(int index) {
        if (thumbs == null) return;
        for (int i = 0; i < thumbs.length; i++) {
            if (thumbs[i] == null) continue;
            GradientDrawable d = round(Color.parseColor("#0F1419"), dp(8));
            if (i == index) d.setStroke(dp(3), Color.parseColor("#2A7D8C"));
            thumbs[i].setBackground(d);
        }
    }

    private String sceneCaption(boolean applied) {
        return (sceneIdx + 1) + "/" + sceneCount + "  " + sceneName(sceneIdx)
                + (applied ? " · 已选用" : " · 滑动预览，点一下选用");
    }

    private static String sceneName(int index) {
        int n = Math.floorMod(index, SCENE_NAMES.length);
        String name = SCENE_NAMES[n];
        return index >= SCENE_NAMES.length ? "夜·" + name : name;
    }

    private String readText(String url) {
        HttpURLConnection conn = null;
        try {
            conn = (HttpURLConnection) new URL(url).openConnection();
            conn.setConnectTimeout(4000);
            conn.setReadTimeout(4000);
            InputStream in = conn.getInputStream();
            byte[] buf = new byte[64];
            int n = in.read(buf);
            in.close();
            if (n <= 0) return "";
            return new String(buf, 0, n, "UTF-8").trim();
        } catch (Exception e) {
            return "";
        } finally {
            if (conn != null) conn.disconnect();
        }
    }

    private Bitmap readBmp(String url) {
        HttpURLConnection conn = null;
        try {
            conn = (HttpURLConnection) new URL(url).openConnection();
            conn.setConnectTimeout(5000);
            conn.setReadTimeout(8000);
            InputStream in = conn.getInputStream();
            Bitmap bmp = BitmapFactory.decodeStream(in);
            in.close();
            return bmp;
        } catch (Exception e) {
            return null;
        } finally {
            if (conn != null) conn.disconnect();
        }
    }

    private void lock(String mode) {
        if ("none".equals(mode)) poke("lock", "none");
        else poke("lock", mode + "," + mins + "," + interval);
    }

    private void poke(String action, String extra) {
        final String host = hostText();
        net.execute(() -> request(host + "/poke?a=" + enc(action) + (extra == null ? "" : "&t=" + enc(extra)), "已发送"));
    }

    private void ping() {
        final String host = hostText();
        net.execute(() -> request(host + "/poke", "已连上时钟"));
    }

    private void request(String url, String okText) {
        HttpURLConnection conn = null;
        try {
            conn = (HttpURLConnection) new URL(url).openConnection();
            conn.setConnectTimeout(4000);
            conn.setReadTimeout(4000);
            conn.setRequestMethod("GET");
            int code = conn.getResponseCode();
            InputStream in = code >= 400 ? conn.getErrorStream() : conn.getInputStream();
            if (in != null) in.close();
            final String msg = code < 400 ? okText : "失败 " + code;
            runOnUiThread(() -> status.setText(msg));
        } catch (Exception e) {
            runOnUiThread(() -> status.setText("连不上时钟，确认同一 WiFi"));
        } finally {
            if (conn != null) conn.disconnect();
        }
    }

    private String hostText() {
        String host = hostBox.getText().toString().trim();
        if (host.endsWith("/")) host = host.substring(0, host.length() - 1);
        if (host.isEmpty()) host = DEFAULT_HOST;
        return host;
    }

    private static String enc(String s) {
        try {
            return URLEncoder.encode(s, "UTF-8");
        } catch (Exception e) {
            return s;
        }
    }

    private LinearLayout chips(int[] values, boolean isMins) {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        List<TextView> views = new ArrayList<>();
        for (int value : values) {
            TextView chip = text(isMins ? value + "分" : "每" + value + "秒", 13, Color.parseColor("#5A6B7A"), true);
            chip.setPadding(dp(12), dp(8), dp(12), dp(8));
            boolean on = isMins ? value == mins : value == interval;
            chip.setBackground(round(on ? Color.parseColor("#D4EEF2") : Color.WHITE, dp(99)));
            if (on) chip.setTextColor(Color.parseColor("#2A7D8C"));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.setMargins(0, 0, dp(8), dp(8));
            chip.setLayoutParams(lp);
            chip.setOnClickListener(v -> {
                if (isMins) mins = value;
                else interval = value;
                for (TextView item : views) {
                    item.setBackground(round(Color.WHITE, dp(99)));
                    item.setTextColor(Color.parseColor("#5A6B7A"));
                }
                chip.setBackground(round(Color.parseColor("#D4EEF2"), dp(99)));
                chip.setTextColor(Color.parseColor("#2A7D8C"));
            });
            views.add(chip);
            row.addView(chip);
        }
        return row;
    }

    private LinearLayout card(LinearLayout parent, String title) {
        LinearLayout card = new LinearLayout(this);
        card.setOrientation(LinearLayout.VERTICAL);
        card.setPadding(dp(14), dp(14), dp(14), dp(14));
        card.setBackground(round(Color.WHITE, dp(16)));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.bottomMargin = dp(12);
        card.setLayoutParams(lp);
        TextView label = text(title, 12, Color.parseColor("#5A6B7A"), true);
        label.setPadding(0, 0, 0, dp(10));
        card.addView(label);
        parent.addView(card);
        return card;
    }

    private void row(LinearLayout parent, View left, View right) {
        LinearLayout line = new LinearLayout(this);
        line.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        left.setLayoutParams(weight());
        right.setLayoutParams(weight());
        line.addView(left);
        line.addView(right);
        line.setLayoutParams(lp);
        parent.addView(line);
    }

    private LinearLayout.LayoutParams weight() {
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        lp.setMargins(dp(4), dp(4), dp(4), dp(4));
        return lp;
    }

    private Button button(String label, int bg, int fg, View.OnClickListener click) {
        Button b = new Button(this);
        b.setText(label);
        b.setAllCaps(false);
        b.setTextColor(fg);
        b.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        b.setBackground(round(bg, dp(12)));
        b.setOnClickListener(click);
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        lp.setMargins(dp(4), dp(4), dp(4), dp(4));
        b.setLayoutParams(lp);
        return b;
    }

    private TextView text(String value, int sp, int color, boolean bold) {
        TextView tv = new TextView(this);
        tv.setText(value);
        tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, sp);
        tv.setTextColor(color);
        tv.setGravity(Gravity.START);
        if (bold) tv.setTypeface(tv.getTypeface(), android.graphics.Typeface.BOLD);
        return tv;
    }

    private GradientDrawable round(int color, int radius) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(color);
        d.setCornerRadius(radius);
        return d;
    }

    private int dp(int value) {
        return Math.round(value * getResources().getDisplayMetrics().density);
    }
}
