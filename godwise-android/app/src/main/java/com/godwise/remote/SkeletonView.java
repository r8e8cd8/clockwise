package com.godwise.remote;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

/** Draws the 21 MediaPipe hand landmarks over the camera preview. */
public class SkeletonView extends View {
    private static final int[][] BONES = {
            {0, 1}, {1, 2}, {2, 3}, {3, 4},
            {0, 5}, {5, 6}, {6, 7}, {7, 8},
            {5, 9}, {9, 10}, {10, 11}, {11, 12},
            {9, 13}, {13, 14}, {14, 15}, {15, 16},
            {13, 17}, {0, 17}, {17, 18}, {18, 19}, {19, 20},
    };

    private final Paint bone = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint joint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint tip = new Paint(Paint.ANTI_ALIAS_FLAG);
    private float[] nx;
    private float[] ny;
    private int imageW = 1;
    private int imageH = 1;

    public SkeletonView(Context context) {
        this(context, null);
    }

    public SkeletonView(Context context, AttributeSet attrs) {
        super(context, attrs);
        setWillNotDraw(false);
        bone.setStyle(Paint.Style.STROKE);
        bone.setStrokeWidth(dp(3));
        bone.setStrokeCap(Paint.Cap.ROUND);
        bone.setColor(Color.parseColor("#7CFFB2"));
        joint.setStyle(Paint.Style.FILL);
        joint.setColor(Color.WHITE);
        tip.setStyle(Paint.Style.FILL);
        tip.setColor(Color.parseColor("#FF5FA2"));
    }

    void show(float[] xs, float[] ys, int w, int h) {
        nx = xs;
        ny = ys;
        if (w > 0) imageW = w;
        if (h > 0) imageH = h;
        postInvalidate();
    }

    void clearHand() {
        nx = null;
        ny = null;
        postInvalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (nx == null || ny == null || nx.length < 21) return;
        float viewAspect = getWidth() / (float) Math.max(1, getHeight());
        float imageAspect = imageW / (float) imageH;
        float dw;
        float dh;
        float ox;
        float oy;
        if (viewAspect > imageAspect) {
            dh = getHeight();
            dw = dh * imageAspect;
            ox = (getWidth() - dw) / 2f;
            oy = 0f;
        } else {
            dw = getWidth();
            dh = dw / imageAspect;
            ox = 0f;
            oy = (getHeight() - dh) / 2f;
        }
        float[] x = new float[21];
        float[] y = new float[21];
        for (int i = 0; i < 21; i++) {
            x[i] = ox + nx[i] * dw;
            y[i] = oy + ny[i] * dh;
        }
        for (int[] edge : BONES) {
            canvas.drawLine(x[edge[0]], y[edge[0]], x[edge[1]], y[edge[1]], bone);
        }
        for (int i = 0; i < 21; i++) {
            boolean fingertip = i == 4 || i == 8 || i == 12 || i == 16 || i == 20;
            canvas.drawCircle(x[i], y[i], dp(fingertip ? 5 : 3), fingertip ? tip : joint);
        }
    }

    private float dp(float value) {
        return value * getResources().getDisplayMetrics().density;
    }
}
