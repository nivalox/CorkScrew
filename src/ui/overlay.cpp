#include <jni.h>
#include <stdarg.h>

#define DEF_PW 900   /* default panel size (replaced on every open) */
#define DEF_PH 600
#define LAND_W 50    /* landscape: panel is 50% of screen width, 70% of height */
#define LAND_H 70
#define PORT_W 92    /* portrait: 92% of width, 45% of height */
#define PORT_H 45
#define SIDE_W 300   /* sidebar width (px) */
#define BS 110       /* button size (px) */

#define ACCENT 0x2196F3u   /* glow color (same blue as the button) */
#define GLOW_K 7           /* number of glow layers */
#define GLOW_STEP 2        /* px per layer, so the glow is 14px wide */
#define GLOW_ALPHA 0x1E    /* strength of each layer (0-255) */

#define LP  "android/view/WindowManager$LayoutParams"
#define LL  "android/widget/LinearLayout"
#define TV  "android/widget/TextView"
#define VW  "android/view/View"
#define VG  "android/view/ViewGroup"
#define GD  "android/graphics/drawable/GradientDrawable"
#define VPA "android/view/ViewPropertyAnimator"
#define CTXC "android/content/Context"
#define CTX "(Landroid/content/Context;)V"
#define DRAGSIG "(Landroid/view/WindowManager;Landroid/view/WindowManager$LayoutParams;Landroid/view/View;)V"

static jobject g_wm, g_ctx, g_btn, g_panel, g_blp, g_plp, g_content, g_page, g_edit;
static jobject g_tabs[3];
static int g_started = 0, g_open = 0, g_attached = 0, g_closing = 0;
static int g_pivx = 0, g_pivy = 0, g_pw = DEF_PW, g_ph = DEF_PH, g_tab = 1;

static const unsigned char HOME_PNG[] = {
#include "home_png.inc"
};

static int vcall(JNIEnv *env, jobject o, const char *cls, const char *name, const char *sig, ...) {
    jclass c = env->FindClass(cls);
    jmethodID m = c ? env->GetMethodID(c, name, sig) : nullptr;
    if (!m) { env->ExceptionClear(); return 0; }
    va_list a; va_start(a, sig);
    env->CallVoidMethodV(o, m, a);
    va_end(a);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return 0; }
    return 1;
}

static jobject ocall(JNIEnv *env, jobject o, const char *cls, const char *name, const char *sig, ...) {
    jclass c = env->FindClass(cls);
    jmethodID m = c ? env->GetMethodID(c, name, sig) : nullptr;
    if (!m) { env->ExceptionClear(); return nullptr; }
    va_list a; va_start(a, sig);
    jobject r = env->CallObjectMethodV(o, m, a);
    va_end(a);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return r;
}

static jobject mk(JNIEnv *env, const char *cls, const char *sig, ...) {
    jclass c = env->FindClass(cls);
    jmethodID m = c ? env->GetMethodID(c, "<init>", sig) : nullptr;
    if (!m) { env->ExceptionClear(); return nullptr; }
    va_list a; va_start(a, sig);
    jobject o = env->NewObjectV(c, m, a);
    va_end(a);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return o;
}

static void setI(JNIEnv *env, jobject o, const char *cls, const char *f, jint v) {
    jclass c = env->FindClass(cls);
    jfieldID id = c ? env->GetFieldID(c, f, "I") : nullptr;
    if (id) env->SetIntField(o, id, v); else env->ExceptionClear();
}

static jint getI(JNIEnv *env, jobject o, const char *cls, const char *f) {
    jclass c = env->FindClass(cls);
    jfieldID id = c ? env->GetFieldID(c, f, "I") : nullptr;
    if (!id) { env->ExceptionClear(); return 0; }
    return env->GetIntField(o, id);
}

static int clampi(int v, int lo, int hi) {
    if (hi < lo) hi = lo;
    return v < lo ? lo : (v > hi ? hi : v);
}

static void vis(JNIEnv *env, jobject v, int on) {
    vcall(env, v, VW, "setVisibility", "(I)V", on ? 0 : 8);
}

/* selected tab gets a darker background */
static void selectTab(JNIEnv *env, int id) {
    for (int i = 0; i < 3; i++)
        if (g_tabs[i])
            vcall(env, g_tabs[i], VW, "setBackgroundColor", "(I)V",
                (jint)(i + 1 == id ? 0xFF111111u : 0xFF3A3A3Au));
}

static int addv(JNIEnv *env, jobject v, jobject lp) {
    return vcall(env, g_wm, "android/view/ViewManager", "addView",
        "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V", v, lp);
}
static void remv(JNIEnv *env, jobject v) {
    vcall(env, g_wm, "android/view/ViewManager", "removeView", "(Landroid/view/View;)V", v);
}

/* removes whichever view is currently on screen (panel if g_open, else button) */
static void detach(JNIEnv *env) {
    if (!g_attached) return;
    remv(env, g_open ? g_panel : g_btn);
    g_attached = 0;
}

/* The Text Editor tab needs a focusable window (for the keyboard).
   Other tabs stay NOT_FOCUSABLE so the game keeps input.
   32 = NOT_TOUCH_MODAL, 40 = NOT_TOUCH_MODAL | NOT_FOCUSABLE */
static void applyFocus(JNIEnv *env) {
    setI(env, g_plp, LP, "flags", g_tab == 2 ? 32 : 40);
    if (g_attached && g_open)
        vcall(env, g_wm, "android/view/ViewManager", "updateViewLayout",
            "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V", g_panel, g_plp);
}

/* reset panel to its normal, fully visible state */
static void panelFinal(JNIEnv *env) {
    jobject a = ocall(env, g_panel, VW, "animate", "()L" VPA ";");
    if (a) vcall(env, a, VPA, "cancel", "()V");
    vcall(env, g_panel, VW, "setScaleX", "(F)V", 1.0);
    vcall(env, g_panel, VW, "setScaleY", "(F)V", 1.0);
    vcall(env, g_panel, VW, "setAlpha", "(F)V", 1.0);
}

static void attach(JNIEnv *env) {
    if (g_open) { panelFinal(env); g_attached = addv(env, g_panel, g_plp); }
    else g_attached = addv(env, g_btn, g_blp);
}

static void screen(JNIEnv *env, int *w, int *h) {
    *w = 1080; *h = 1920;
    jobject res = ocall(env, g_ctx, CTXC, "getResources", "()Landroid/content/res/Resources;");
    jobject dm = res ? ocall(env, res, "android/content/res/Resources",
        "getDisplayMetrics", "()Landroid/util/DisplayMetrics;") : nullptr;
    if (!dm) return;
    int ww = getI(env, dm, "android/util/DisplayMetrics", "widthPixels");
    int hh = getI(env, dm, "android/util/DisplayMetrics", "heightPixels");
    if (ww > 0 && hh > 0) { *w = ww; *h = hh; }
}

/* animate scale + alpha of a view; endAct (optional Runnable) runs when finished */
static void animTo(JNIEnv *env, jobject v, double sx, double sy, double al, jlong dur, jobject endAct) {
    jobject a = ocall(env, v, VW, "animate", "()L" VPA ";");
    if (a) a = ocall(env, a, VPA, "scaleX", "(F)L" VPA ";", sx);
    if (a) a = ocall(env, a, VPA, "scaleY", "(F)L" VPA ";", sy);
    if (a) a = ocall(env, a, VPA, "alpha", "(F)L" VPA ";", al);
    if (a) a = ocall(env, a, VPA, "setDuration", "(J)L" VPA ";", dur);
    jobject ip = mk(env, "android/view/animation/DecelerateInterpolator", "()V");
    if (a && ip) a = ocall(env, a, VPA, "setInterpolator", "(Landroid/animation/TimeInterpolator;)L" VPA ";", ip);
    if (a && endAct) ocall(env, a, VPA, "withEndAction", "(Ljava/lang/Runnable;)L" VPA ";", endAct);
}

/* swap the panel for the button, placing the button where the panel collapsed to */
static void finishClose(JNIEnv *env) {
    int sw, sh;
    screen(env, &sw, &sh);
    int bx = clampi(getI(env, g_plp, LP, "x") + g_pivx, 0, sw - BS);
    int by = clampi(getI(env, g_plp, LP, "y") + g_pivy, 0, sh - BS);
    setI(env, g_blp, LP, "x", bx);
    setI(env, g_blp, LP, "y", by);
    g_closing = 0;
    detach(env);
    g_open = 0;
}

/* add child to a LinearLayout with width/height/weight */
static void addw(JNIEnv *env, jobject parent, jobject child, int w, int h, double wt) {
    jobject lp = mk(env, "android/widget/LinearLayout$LayoutParams", "(IIF)V", w, h, wt);
    if (lp) vcall(env, parent, VG, "addView",
        "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V", child, lp);
}

static jobject label(JNIEnv *env, jobject ctx, const char *cls, const char *text, double size) {
    jobject t = mk(env, cls, CTX, ctx);
    if (!t) return nullptr;
    vcall(env, t, TV, "setText", "(Ljava/lang/CharSequence;)V", env->NewStringUTF(text));
    vcall(env, t, TV, "setTextColor", "(I)V", (jint)-1);
    vcall(env, t, TV, "setTextSize", "(F)V", size);
    return t;
}

static jobject clicker(JNIEnv *env, int id) {
    return mk(env, "com/rokori/client/Click", "(I)V", id);
}

/* decode the embedded PNG into a drawable of size x size px */
static jobject icon(JNIEnv *env, jobject ctx, int size) {
    jsize n = (jsize)sizeof(HOME_PNG);
    jbyteArray arr = env->NewByteArray(n);
    if (!arr) return nullptr;
    env->SetByteArrayRegion(arr, 0, n, (const jbyte *)HOME_PNG);

    jclass bf = env->FindClass("android/graphics/BitmapFactory");
    jmethodID dm = bf ? env->GetStaticMethodID(bf, "decodeByteArray",
        "([BII)Landroid/graphics/Bitmap;") : nullptr;
    if (!dm) { env->ExceptionClear(); return nullptr; }
    jobject bmp = env->CallStaticObjectMethod(bf, dm, arr, 0, (jint)n);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    if (!bmp) return nullptr;

    jobject res = ocall(env, ctx, CTXC, "getResources", "()Landroid/content/res/Resources;");
    jobject d = res ? mk(env, "android/graphics/drawable/BitmapDrawable",
        "(Landroid/content/res/Resources;Landroid/graphics/Bitmap;)V", res, bmp) : nullptr;
    if (d) vcall(env, d, "android/graphics/drawable/Drawable", "setBounds", "(IIII)V", 0, 0, size, size);
    return d;
}

/* soft glow: stacked translucent rounded rects, each inset a bit more than the last */
static jobject glowBg(JNIEnv *env) {
    jclass dc = env->FindClass("android/graphics/drawable/Drawable");
    if (!dc) { env->ExceptionClear(); return nullptr; }
    jobjectArray arr = env->NewObjectArray(GLOW_K, dc, nullptr);
    if (!arr) { env->ExceptionClear(); return nullptr; }
    for (int k = 0; k < GLOW_K; k++) {
        jobject g = mk(env, GD, "()V");
        if (!g) return nullptr;
        vcall(env, g, GD, "setColor", "(I)V", (jint)((GLOW_ALPHA << 24) | ACCENT));
        vcall(env, g, GD, "setCornerRadius", "(F)V", (double)(28 + (GLOW_K - k) * GLOW_STEP));
        env->SetObjectArrayElement(arr, k, g);
    }
    jobject ld = mk(env, "android/graphics/drawable/LayerDrawable",
        "([Landroid/graphics/drawable/Drawable;)V", arr);
    if (!ld) return nullptr;
    for (int k = 0; k < GLOW_K; k++)
        vcall(env, ld, "android/graphics/drawable/LayerDrawable", "setLayerInset", "(IIIII)V",
            k, k * GLOW_STEP, k * GLOW_STEP, k * GLOW_STEP, k * GLOW_STEP);
    return ld;
}

static int build(JNIEnv *env, jobject ctx0) {
    jobject ctx = ocall(env, ctx0, CTXC, "getApplicationContext", "()Landroid/content/Context;");
    if (!ctx) ctx = ctx0;
    jobject wm = ocall(env, ctx, CTXC, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;", env->NewStringUTF("window"));
    if (!wm) return 0;

    /* ---- floating round button ("C") ---- */
    jobject btn = label(env, ctx, TV, "C", 18.0);
    if (!btn) return 0;
    vcall(env, btn, TV, "setGravity", "(I)V", 17);
    vcall(env, btn, TV, "setTypeface", "(Landroid/graphics/Typeface;I)V", (jobject)nullptr, 1);
    jobject gd = mk(env, GD, "()V");
    if (gd) {
        vcall(env, gd, GD, "setShape", "(I)V", 1);
        vcall(env, gd, GD, "setColor", "(I)V", (jint)(0xFF000000u | ACCENT));
        vcall(env, btn, VW, "setBackground", "(Landroid/graphics/drawable/Drawable;)V", gd);
    }
    jobject blp = mk(env, LP, "(IIIII)V", BS, BS, 2038, 8, -3);
    if (!blp) return 0;
    setI(env, blp, LP, "gravity", 51);
    setI(env, blp, LP, "x", 100);
    setI(env, blp, LP, "y", 300);
    jobject drag = mk(env, "com/rokori/client/Drag", DRAGSIG, wm, blp, btn);
    if (drag) vcall(env, btn, VW, "setOnTouchListener", "(Landroid/view/View$OnTouchListener;)V", drag);

    /* ---- panel: root = glow frame, inner = the dark panel ---- */
    jobject root = mk(env, LL, CTX, ctx);
    if (!root) return 0;
    vcall(env, root, LL, "setOrientation", "(I)V", 1);
    jobject glow = glowBg(env);
    if (glow) vcall(env, root, VW, "setBackground", "(Landroid/graphics/drawable/Drawable;)V", glow);
    vcall(env, root, VW, "setPadding", "(IIII)V",
        GLOW_K * GLOW_STEP, GLOW_K * GLOW_STEP, GLOW_K * GLOW_STEP, GLOW_K * GLOW_STEP);

    jobject inner = mk(env, LL, CTX, ctx);
    if (!inner) return 0;
    vcall(env, inner, LL, "setOrientation", "(I)V", 1);
    jobject pg = mk(env, GD, "()V");
    if (pg) {
        vcall(env, pg, GD, "setColor", "(I)V", (jint)0xFF202020u);
        vcall(env, pg, GD, "setCornerRadius", "(F)V", (double)28.0);
        vcall(env, inner, VW, "setBackground", "(Landroid/graphics/drawable/Drawable;)V", pg);
    }
    vcall(env, inner, VW, "setClipToOutline", "(Z)V", 1);   /* keep children inside the rounded corners */
    addw(env, root, inner, -1, 0, 1.0);

    /* header: title + X */
    jobject head = mk(env, LL, CTX, ctx);
    if (!head) return 0;
    vcall(env, head, LL, "setOrientation", "(I)V", 0);
    vcall(env, head, VW, "setPadding", "(IIII)V", 36, 10, 10, 10);
    jobject title = label(env, ctx, TV, "CorkScrew", 18.0);
    if (title) {
        vcall(env, title, TV, "setGravity", "(I)V", 16);
        vcall(env, title, TV, "setTypeface", "(Landroid/graphics/Typeface;I)V", (jobject)nullptr, 1);
        addw(env, head, title, 0, -2, 1.0);
    }
    jobject x = label(env, ctx, TV, "X", 20.0);
    if (x) {
        vcall(env, x, VW, "setPadding", "(IIII)V", 36, 10, 36, 10);
        jobject cl = clicker(env, 0);
        if (cl) vcall(env, x, VW, "setOnClickListener", "(Landroid/view/View$OnClickListener;)V", cl);
        addw(env, head, x, -2, -2, 0.0);
    }
    addw(env, inner, head, -1, -2, 0.0);

    /* body: side tabs + content */
    jobject body = mk(env, LL, CTX, ctx);
    if (!body) return 0;
    vcall(env, body, LL, "setOrientation", "(I)V", 0);

    jobject side = mk(env, LL, CTX, ctx);
    if (!side) return 0;
    vcall(env, side, LL, "setOrientation", "(I)V", 1);
    vcall(env, side, VW, "setBackgroundColor", "(I)V", (jint)0xFF2B2B2Bu);
    const char *tabs[3] = { "Home", "Text Editor", "Tab 3" };
    for (int i = 0; i < 3; i++) {
        jobject b = label(env, ctx, "android/widget/Button", tabs[i], 14.0);
        if (!b) continue;
        g_tabs[i] = env->NewGlobalRef(b);

        if (i == 0) {   /* Home tab icon */
            jobject ic = icon(env, ctx, 56);
            if (ic) {
                vcall(env, b, TV, "setCompoundDrawables",
                    "(Landroid/graphics/drawable/Drawable;Landroid/graphics/drawable/Drawable;Landroid/graphics/drawable/Drawable;Landroid/graphics/drawable/Drawable;)V",
                    ic, (jobject)nullptr, (jobject)nullptr, (jobject)nullptr);
                vcall(env, b, TV, "setCompoundDrawablePadding", "(I)V", 16);
            }
        }

        jobject cl = clicker(env, i + 1);
        if (cl) vcall(env, b, VW, "setOnClickListener", "(Landroid/view/View$OnClickListener;)V", cl);
        addw(env, side, b, -1, -2, 0.0);
    }
    selectTab(env, 1);   /* Home starts selected */
    addw(env, body, side, SIDE_W, -1, 0.0);

    /* text page: Home / Tab 3 */
    jobject content = label(env, ctx, TV, "Welcome to CorkScrew", 20.0);
    if (!content) return 0;
    vcall(env, content, TV, "setGravity", "(I)V", 17);
    vcall(env, content, VW, "setBackgroundColor", "(I)V", (jint)0xFF1A1A1Au);
    addw(env, body, content, 0, -1, 1.0);

    /* Text Editor page: big editor + Clear button (hidden until its tab is picked) */
    jobject page = mk(env, LL, CTX, ctx);
    if (!page) return 0;
    vcall(env, page, LL, "setOrientation", "(I)V", 1);
    vcall(env, page, VW, "setBackgroundColor", "(I)V", (jint)0xFF1A1A1Au);

    jobject edit = label(env, ctx, "android/widget/EditText", "", 16.0);
    if (!edit) return 0;
    vcall(env, edit, TV, "setHint", "(Ljava/lang/CharSequence;)V", env->NewStringUTF("Type here..."));
    vcall(env, edit, TV, "setHintTextColor", "(I)V", (jint)0xFF777777u);
    vcall(env, edit, TV, "setGravity", "(I)V", 51);                /* top-left */
    vcall(env, edit, TV, "setInputType", "(I)V", 0x20001);         /* text + multi-line */
    vcall(env, edit, TV, "setImeOptions", "(I)V", 0x12000000);     /* no fullscreen keyboard UI */
    vcall(env, edit, VW, "setBackgroundColor", "(I)V", (jint)0xFF111111u);
    vcall(env, edit, VW, "setPadding", "(IIII)V", 24, 24, 24, 24);
    addw(env, page, edit, -1, 0, 1.0);

    jobject bar = mk(env, LL, CTX, ctx);
    if (bar) {
        vcall(env, bar, LL, "setOrientation", "(I)V", 0);
        vcall(env, bar, LL, "setGravity", "(I)V", 5);              /* right-aligned */
        vcall(env, bar, VW, "setPadding", "(IIII)V", 12, 12, 12, 12);
        jobject clr = label(env, ctx, "android/widget/Button", "Clear", 14.0);
        if (clr) {
            vcall(env, clr, VW, "setBackgroundColor", "(I)V", (jint)0xFF3A3A3Au);
            jobject cl = clicker(env, 4);
            if (cl) vcall(env, clr, VW, "setOnClickListener", "(Landroid/view/View$OnClickListener;)V", cl);
            addw(env, bar, clr, -2, -2, 0.0);
        }
        addw(env, page, bar, -1, -2, 0.0);
    }
    vis(env, page, 0);
    addw(env, body, page, 0, -1, 1.0);

    addw(env, inner, body, -1, 0, 1.0);

    /* starts NOT_FOCUSABLE | NOT_TOUCH_MODAL (40); size is set on every open */
    jobject plp = mk(env, LP, "(IIIII)V", DEF_PW, DEF_PH, 2038, 40, -3);
    if (!plp) return 0;
    setI(env, plp, LP, "gravity", 51);
    setI(env, plp, LP, "x", 60);
    setI(env, plp, LP, "y", 150);
    setI(env, plp, LP, "softInputMode", 32);   /* pan the window when the keyboard opens */

    /* drag the whole panel by the top bar */
    jobject pdrag = mk(env, "com/rokori/client/Drag", DRAGSIG, wm, plp, root);
    if (pdrag) vcall(env, head, VW, "setOnTouchListener", "(Landroid/view/View$OnTouchListener;)V", pdrag);

    g_ctx = env->NewGlobalRef(ctx);
    g_wm = env->NewGlobalRef(wm);
    g_btn = env->NewGlobalRef(btn);
    g_blp = env->NewGlobalRef(blp);
    g_panel = env->NewGlobalRef(root);
    g_plp = env->NewGlobalRef(plp);
    g_content = env->NewGlobalRef(content);
    g_page = env->NewGlobalRef(page);
    g_edit = env->NewGlobalRef(edit);
    return 1;
}

extern "C" {

JNIEXPORT void JNICALL
Java_com_roblox_client_ActivityNativeMain_show(JNIEnv *env, jclass cls, jobject ctx) {
    g_started++;
    if (!g_btn && !build(env, ctx)) { g_started--; return; }
    if (!g_attached) attach(env);
}

JNIEXPORT void JNICALL
Java_com_roblox_client_ActivityNativeMain_hide(JNIEnv *env, jclass cls, jobject ctx) {
    if (--g_started > 0 || !g_wm) return;
    g_started = 0;
    if (g_closing) { finishClose(env); return; }
    detach(env);
}

/* tap on the floating button: panel grows out of the button */
JNIEXPORT void JNICALL
Java_com_roblox_client_ActivityNativeMain_toggle(JNIEnv *env, jclass cls) {
    if (!g_panel || !g_attached || g_open || g_closing) return;
    int sw, sh;
    screen(env, &sw, &sh);
    if (sw > sh) { g_pw = sw * LAND_W / 100; g_ph = sh * LAND_H / 100; }
    else         { g_pw = sw * PORT_W / 100; g_ph = sh * PORT_H / 100; }
    setI(env, g_plp, LP, "width", g_pw);
    setI(env, g_plp, LP, "height", g_ph);

    int bx = getI(env, g_blp, LP, "x");
    int by = getI(env, g_blp, LP, "y");
    int px = clampi(bx, 0, sw - g_pw);
    int py = clampi(by, 0, sh - g_ph);
    g_pivx = bx - px;
    g_pivy = by - py;
    setI(env, g_plp, LP, "x", px);
    setI(env, g_plp, LP, "y", py);

    detach(env);
    g_open = 1;
    applyFocus(env);
    vcall(env, g_panel, VW, "setPivotX", "(F)V", (double)g_pivx);
    vcall(env, g_panel, VW, "setPivotY", "(F)V", (double)g_pivy);
    vcall(env, g_panel, VW, "setScaleX", "(F)V", (double)BS / g_pw);
    vcall(env, g_panel, VW, "setScaleY", "(F)V", (double)BS / g_ph);
    vcall(env, g_panel, VW, "setAlpha", "(F)V", 0.0);
    g_attached = addv(env, g_panel, g_plp);
    if (g_attached) animTo(env, g_panel, 1.0, 1.0, 1.0, (jlong)220, nullptr);
}

/* called by the Run class when the close animation finishes */
JNIEXPORT void JNICALL
Java_com_roblox_client_ActivityNativeMain_closed(JNIEnv *env, jclass cls) {
    if (!g_closing) return;
    finishClose(env);
    if (g_started > 0) attach(env);
}

/* 0 = X (collapse panel), 1 = Home, 2 = Text Editor, 3 = Tab 3, 4 = Clear */
JNIEXPORT void JNICALL
Java_com_roblox_client_ActivityNativeMain_act(JNIEnv *env, jclass cls, jint id) {
    if (!g_panel) return;
    if (id == 0) {
        if (!g_open || g_closing || !g_attached) return;
        jobject run = mk(env, "com/rokori/client/Run", "()V");
        if (!run) { finishClose(env); attach(env); return; }   /* no Run class: close instantly */
        g_closing = 1;
        animTo(env, g_panel, (double)BS / g_pw, (double)BS / g_ph, 0.0, (jlong)180, run);
        return;
    }
    if (id == 4) {
        vcall(env, g_edit, TV, "setText", "(Ljava/lang/CharSequence;)V", env->NewStringUTF(""));
        return;
    }
    g_tab = id;
    selectTab(env, id);
    if (id == 2) {
        vis(env, g_content, 0);
        vis(env, g_page, 1);
    } else {
        vcall(env, g_content, TV, "setText", "(Ljava/lang/CharSequence;)V",
            env->NewStringUTF(id == 1 ? "Welcome to CorkScrew" : "Coming Soon"));
        vis(env, g_page, 0);
        vis(env, g_content, 1);
    }
    applyFocus(env);
}

}  /* extern "C" */