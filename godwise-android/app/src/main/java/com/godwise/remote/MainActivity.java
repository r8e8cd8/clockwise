package com.godwise.remote;

import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
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
import androidx.camera.view.PreviewView;
import androidx.core.content.ContextCompat;

import com.google.common.util.concurrent.ListenableFuture;

import java.io.InputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLEncoder;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public class MainActivity extends AppCompatActivity {
    private static final String DEFAULT_HOST = "http://clockwise.local";
    private static final int TW = 48;
    private static final int TH = 36;

    private final ExecutorService net = Executors.newSingleThreadExecutor();
    private final ExecutorService camExec = Executors.newSingleThreadExecutor();
    private final List<Float> xs = new ArrayList<>();
    private final List<Float> ys = new ArrayList<>();
    private final List<Float> motions = new ArrayList<>();

    private EditText hostBox;
    private TextView status;
    private TextView gesture;
    private PreviewView preview;
    private ProcessCameraProvider cameraProvider;
    private byte[] prev;
    private long coolUntil;
    private int mins = 5;
    private int interval = 30;
    private boolean cameraWanted;

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
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (cameraProvider != null) {
            cameraProvider.unbindAll();
        }
        net.shutdownNow();
        camExec.shutdownNow();
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

        LinearLayout camCard = card(root, "摄像头手势");
        preview = new PreviewView(this);
        LinearLayout.LayoutParams prevLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(220));
        prevLp.bottomMargin = dp(8);
        preview.setLayoutParams(prevLp);
        preview.setBackgroundColor(Color.parseColor("#0F1419"));
        camCard.addView(preview);
        row(camCard,
                button("开启摄像头", 0xFFD8F0E2, 0xFF2F7A4E, v -> toggleCamera()),
                button("测试连接", 0xFFDCE9F7, 0xFF2C5F9E, v -> ping()));
        gesture = text("手势关闭 · 按钮可直接用", 14, Color.parseColor("#2A7D8C"), true);
        gesture.setPadding(0, dp(8), 0, 0);
        camCard.addView(gesture);
        TextView hint = text("左右挥手 → 眨眼 · 上下点头 → 比心 · 快速晃动 → 换背景", 12, Color.parseColor("#5A6B7A"), false);
        hint.setPadding(0, dp(6), 0, 0);
        camCard.addView(hint);

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
        LinearLayout sayRow = new LinearLayout(this);
        sayRow.setOrientation(LinearLayout.HORIZONTAL);
        for (String word : new String[]{"Hi!", "Love", "Night"}) {
            Button b = button(word, 0xFFFFFFFF, 0xFF5A6B7A, v -> poke("say", word));
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.setMargins(0, 0, dp(8), 0);
            b.setLayoutParams(lp);
            sayRow.addView(b);
        }
        say.addView(sayRow);
    }

    private void toggleCamera() {
        if (cameraProvider != null) {
            cameraProvider.unbindAll();
            cameraProvider = null;
            cameraWanted = false;
            prev = null;
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
                        .build();
                analysis.setAnalyzer(camExec, this::analyze);
                cameraProvider.unbindAll();
                cameraProvider.bindToLifecycle(
                        this, CameraSelector.DEFAULT_FRONT_CAMERA, previewUse, analysis);
                gesture.setText("手势待机 · 左右挥手 / 上下点头");
            } catch (Exception e) {
                cameraProvider = null;
                gesture.setText("摄像头打开失败，请用按钮");
            }
        }, ContextCompat.getMainExecutor(this));
    }

    private void analyze(@NonNull ImageProxy image) {
        try {
            if (image.getPlanes().length == 0) return;
            ImageProxy.PlaneProxy plane = image.getPlanes()[0];
            ByteBuffer buffer = plane.getBuffer();
            int rowStride = plane.getRowStride();
            int pixelStride = plane.getPixelStride();
            int w = image.getWidth();
            int h = image.getHeight();
            if (w <= 0 || h <= 0) return;
            byte[] gray = new byte[TW * TH];
            int limit = buffer.limit();
            for (int y = 0; y < TH; y++) {
                int sy = y * h / TH;
                for (int x = 0; x < TW; x++) {
                    int sx = x * w / TW;
                    int index = sy * rowStride + sx * pixelStride;
                    int value = 0;
                    if (index >= 0 && index < limit) value = buffer.get(index) & 0xff;
                    gray[y * TW + x] = (byte) value;
                }
            }
            byte[] last = prev;
            prev = gray;
            if (last == null || last.length != gray.length) return;

            int sum = 0;
            int cx = 0;
            int cy = 0;
            for (int y = 0; y < TH; y++) {
                for (int x = 0; x < TW; x++) {
                    int i = y * TW + x;
                    int d = Math.abs((gray[i] & 0xff) - (last[i] & 0xff));
                    if (d > 18) {
                        sum += d;
                        cx += x * d;
                        cy += y * d;
                    }
                }
            }
            float motion = sum / (float) (TW * TH);
            push(xs, sum == 0 ? TW / 2f : cx / (float) sum, 14);
            push(ys, sum == 0 ? TH / 2f : cy / (float) sum, 14);
            push(motions, motion, 10);
            float avg = 0;
            for (float v : motions) avg += v;
            avg /= motions.size();
            if (avg > 55) {
                fire("快速晃动", "next");
                clearTracks();
            } else if (flips(xs) >= 3 && avg > 8) {
                fire("左右挥手", "blink");
                clearTracks();
            } else if (flips(ys) >= 2 && avg > 7) {
                fire("上下点头", "heart");
                clearTracks();
            }
        } catch (Exception ignored) {
        } finally {
            image.close();
        }
    }

    private void fire(String name, String action) {
        long now = SystemClock.elapsedRealtime();
        if (now < coolUntil) return;
        coolUntil = now + 1600;
        runOnUiThread(() -> {
            gesture.setText("识别到：" + name);
            poke(action, null);
        });
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

    private static void push(List<Float> list, float value, int max) {
        list.add(value);
        if (list.size() > max) list.remove(0);
    }

    private static int flips(List<Float> list) {
        int count = 0;
        for (int i = 2; i < list.size(); i++) {
            float a = list.get(i - 1) - list.get(i - 2);
            float b = list.get(i) - list.get(i - 1);
            if (a * b < 0 && Math.abs(a) > 1.2f && Math.abs(b) > 1.2f) count++;
        }
        return count;
    }

    private void clearTracks() {
        xs.clear();
        ys.clear();
        motions.clear();
    }
}
