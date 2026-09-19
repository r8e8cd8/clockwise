package com.godwise.remote;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.graphics.Matrix;

import com.google.mediapipe.framework.image.BitmapImageBuilder;
import com.google.mediapipe.tasks.components.containers.NormalizedLandmark;
import com.google.mediapipe.tasks.core.BaseOptions;
import com.google.mediapipe.tasks.vision.core.RunningMode;
import com.google.mediapipe.tasks.vision.handlandmarker.HandLandmarker;
import com.google.mediapipe.tasks.vision.handlandmarker.HandLandmarkerResult;

import org.tensorflow.lite.Interpreter;

import java.io.FileInputStream;
import java.nio.MappedByteBuffer;
import java.nio.channels.FileChannel;
import java.util.ArrayDeque;
import java.util.List;

/**
 * Port of Kazuhito00/hand-gesture-recognition-using-mediapipe.
 * Static signs: Open / Close / Pointer. Finger motion: Stop / Clockwise /
 * Counter Clockwise / Move. Motion and a fist both switch the clock background.
 */
public final class HandGestures {
    static final class Hit {
        final String name;
        final String action;
        final String seen;
        final float[] nx;
        final float[] ny;
        final int imageW;
        final int imageH;

        Hit(String name, String action, String seen, float[] nx, float[] ny, int imageW, int imageH) {
            this.name = name;
            this.action = action;
            this.seen = seen;
            this.nx = nx;
            this.ny = ny;
            this.imageW = imageW;
            this.imageH = imageH;
        }
    }

    private static final int HISTORY = 16;
    private static final int CLOSE = 1;
    private static final int POINTER = 2;

    private final HandLandmarker landmarker;
    private final Interpreter signs;
    private final Interpreter motions;
    private final ArrayDeque<int[]> trail = new ArrayDeque<>();
    private final ArrayDeque<Integer> motionVotes = new ArrayDeque<>();
    private int closeStreak;
    private String latched;
    private long frameMs;

    HandGestures(Context context) throws Exception {
        landmarker = HandLandmarker.createFromOptions(
                context,
                HandLandmarker.HandLandmarkerOptions.builder()
                        .setBaseOptions(BaseOptions.builder().setModelAssetPath("hand_landmarker.task").build())
                        .setRunningMode(RunningMode.VIDEO)
                        .setNumHands(1)
                        .setMinHandDetectionConfidence(0.7f)
                        .setMinHandPresenceConfidence(0.6f)
                        .setMinTrackingConfidence(0.5f)
                        .build());
        signs = new Interpreter(mapAsset(context, "keypoint_classifier.tflite"));
        motions = new Interpreter(mapAsset(context, "point_history_classifier.tflite"));
    }

    void close() {
        landmarker.close();
        signs.close();
        motions.close();
    }

    static Bitmap uprightFront(Bitmap rgba, int rotationDegrees) {
        Matrix matrix = new Matrix();
        matrix.postRotate(rotationDegrees);
        matrix.postScale(-1f, 1f);
        return Bitmap.createBitmap(rgba, 0, 0, rgba.getWidth(), rgba.getHeight(), matrix, true);
    }

    Hit recognize(Bitmap upright) {
        frameMs += 33;
        HandLandmarkerResult result = landmarker.detectForVideo(new BitmapImageBuilder(upright).build(), frameMs);
        if (result == null || result.landmarks().isEmpty() || result.landmarks().get(0).size() < 21) {
            pushTrail(0, 0);
            closeStreak = 0;
            latched = null;
            return new Hit(null, null, null, null, null, upright.getWidth(), upright.getHeight());
        }
        List<NormalizedLandmark> hand = result.landmarks().get(0);
        int w = upright.getWidth();
        int h = upright.getHeight();
        float[] nx = new float[21];
        float[] ny = new float[21];
        int[][] pts = new int[21][2];
        for (int i = 0; i < 21; i++) {
            NormalizedLandmark lm = hand.get(i);
            nx[i] = lm.x();
            ny[i] = lm.y();
            pts[i][0] = clamp((int) (lm.x() * w), 0, w - 1);
            pts[i][1] = clamp((int) (lm.y() * h), 0, h - 1);
        }

        int sign = run(signs, new float[][]{preprocessSign(pts)}, 3);
        if (sign == POINTER) pushTrail(pts[8][0], pts[8][1]);
        else pushTrail(0, 0);

        int voted = 0;
        if (sign == POINTER && trail.size() == HISTORY) {
            int motion = run(motions, new float[][]{preprocessTrail(w, h)}, 4);
            pushVote(motion);
            voted = majority();
        }

        if (voted == 1) return fire("顺时针换下一张", "next", "顺时针", nx, ny, w, h);
        if (voted == 2) return fire("逆时针换上一张", "prev", "逆时针", nx, ny, w, h);
        if (voted == 3) return fire("滑动换背景", "next", "滑动", nx, ny, w, h);

        Hit pose = poseOf(hand, sign);
        if (pose != null) return fire(pose.name, pose.action, pose.seen, nx, ny, w, h);

        if (sign == CLOSE) {
            closeStreak++;
            if (closeStreak >= 6) return fire("握拳换背景", "next", "握拳", nx, ny, w, h);
            return seen("握拳", nx, ny, w, h);
        }
        closeStreak = 0;
        if (sign == 0) return seen("张掌", nx, ny, w, h);
        if (sign == POINTER) return seen("食指", nx, ny, w, h);
        return seen(null, nx, ny, w, h);
    }

    private Hit poseOf(List<NormalizedLandmark> lm, int sign) {
        boolean index = extended(lm, 8, 6);
        boolean middle = extended(lm, 12, 10);
        boolean ring = extended(lm, 16, 14);
        boolean pinky = extended(lm, 20, 18);
        boolean thumb = thumbOut(lm);
        float palm = dist(lm, 5, 17);
        if (palm > 0.02f && dist(lm, 4, 8) < palm * 0.55f && !middle && !ring && !pinky) {
            return new Hit("比心", "heart", "比心", null, null, 0, 0);
        }
        if (thumb && index && pinky && !middle && !ring) {
            return new Hit("我爱你", "heart", "我爱你", null, null, 0, 0);
        }
        if (index && middle && !ring && !pinky) {
            return new Hit("剪刀手", "wink", "剪刀手", null, null, 0, 0);
        }
        if (index && middle && ring && !pinky) {
            return new Hit("三指小惊喜", "surprise", "三指", null, null, 0, 0);
        }
        if (thumb && !index && !middle && !ring && !pinky) {
            if (lm.get(4).y() < lm.get(3).y()) return new Hit("点赞偷看", "peek", "点赞", null, null, 0, 0);
            return new Hit("拇指向下", "sleep", "拇指向下", null, null, 0, 0);
        }
        if (index && middle && ring && pinky) {
            return new Hit("张掌眨眼", "blink", "张掌", null, null, 0, 0);
        }
        if (sign == POINTER && index && !middle && !ring && !pinky) {
            return new Hit("食指呼唤", "call", "食指", null, null, 0, 0);
        }
        return null;
    }

    private Hit fire(String name, String action, String seen, float[] nx, float[] ny, int w, int h) {
        if (name.equals(latched)) return seen(seen, nx, ny, w, h);
        latched = name;
        return new Hit(name, action, seen, nx, ny, w, h);
    }

    private Hit seen(String seen, float[] nx, float[] ny, int w, int h) {
        return new Hit(null, null, seen, nx, ny, w, h);
    }

    private static boolean extended(List<NormalizedLandmark> lm, int tip, int pip) {
        return dist(lm, 0, tip) > dist(lm, 0, pip) * 1.18f;
    }

    private static boolean thumbOut(List<NormalizedLandmark> lm) {
        return dist(lm, 4, 17) > dist(lm, 2, 17) * 1.08f;
    }

    private static float dist(List<NormalizedLandmark> lm, int a, int b) {
        float dx = lm.get(a).x() - lm.get(b).x();
        float dy = lm.get(a).y() - lm.get(b).y();
        return (float) Math.hypot(dx, dy);
    }

    private void pushTrail(int x, int y) {
        trail.addLast(new int[]{x, y});
        while (trail.size() > HISTORY) trail.removeFirst();
    }

    private void pushVote(int id) {
        motionVotes.addLast(id);
        while (motionVotes.size() > HISTORY) motionVotes.removeFirst();
    }

    private int majority() {
        int[] count = new int[4];
        for (int id : motionVotes) {
            if (id >= 0 && id < count.length) count[id]++;
        }
        int best = 0;
        for (int i = 1; i < count.length; i++) {
            if (count[i] > count[best]) best = i;
        }
        return count[best] >= 10 ? best : 0;
    }

    private int run(Interpreter interpreter, float[][] input, int classes) {
        float[][] output = new float[1][classes];
        interpreter.run(input, output);
        int best = 0;
        for (int i = 1; i < output[0].length; i++) {
            if (output[0][i] > output[0][best]) best = i;
        }
        return output[0][best] >= 0.5f ? best : -1;
    }

    /** Wrist-relative, max-abs normalized. Same as pre_process_landmark(). */
    private static float[] preprocessSign(int[][] pts) {
        float[] flat = new float[42];
        int baseX = pts[0][0];
        int baseY = pts[0][1];
        float max = 1f;
        for (int i = 0; i < 21; i++) {
            flat[i * 2] = pts[i][0] - baseX;
            flat[i * 2 + 1] = pts[i][1] - baseY;
            max = Math.max(max, Math.abs(flat[i * 2]));
            max = Math.max(max, Math.abs(flat[i * 2 + 1]));
        }
        for (int i = 0; i < flat.length; i++) flat[i] /= max;
        return flat;
    }

    /** Same as pre_process_point_history(). */
    private float[] preprocessTrail(int imageW, int imageH) {
        float[] flat = new float[HISTORY * 2];
        int i = 0;
        int baseX = 0;
        int baseY = 0;
        boolean first = true;
        for (int[] p : trail) {
            if (first) {
                baseX = p[0];
                baseY = p[1];
                first = false;
            }
            flat[i++] = (p[0] - baseX) / (float) imageW;
            flat[i++] = (p[1] - baseY) / (float) imageH;
        }
        return flat;
    }

    private static String motionName(int id) {
        switch (id) {
            case 1:
                return "顺时针";
            case 2:
                return "逆时针";
            case 3:
                return "滑动";
            default:
                return "食指";
        }
    }

    private static int clamp(int v, int min, int max) {
        return Math.max(min, Math.min(max, v));
    }

    private static MappedByteBuffer mapAsset(Context context, String name) throws Exception {
        try (AssetFileDescriptor fd = context.getAssets().openFd(name);
             FileInputStream in = new FileInputStream(fd.getFileDescriptor())) {
            FileChannel channel = in.getChannel();
            return channel.map(FileChannel.MapMode.READ_ONLY, fd.getStartOffset(), fd.getDeclaredLength());
        }
    }
}
