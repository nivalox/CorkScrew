#include <jni.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lua.h"
#include "lualib.h"
#include "luacode.h"

#define DEF_PW 900   /* default panel size (replaced on every open) */
#define DEF_PH 600
#define LAND_W 64    /* landscape: panel is 64% of screen width, 84% of height */
#define LAND_H 84
#define PORT_W 92    /* portrait: 92% of width, 45% of height */
#define PORT_H 45
#define SIDE_W 300   /* sidebar width (px) */
#define TAB_H 130    /* height of each tab button (px) */
#define TAB_ANIM 250 /* tab switch animation (ms) */
#define BS 110       /* floating button size (px) */
#define BTN_PAD 20   /* padding around the icon inside the floating button (px) */
#define PRESS_SCALE 0.92f   /* how small a button gets while pressed */
#define TAB_ICON 56         /* tab icon size (px) */
#define TITLE_ICON 48       /* icon size next to the CorkScrew title (px) */

#define ACCENT 0x2196F3u   /* glow color (same blue as the button) */
#define GLOW_K 7           /* number of glow layers */
#define GLOW_STEP 2        /* px per layer, so the glow is 14px wide */
#define GLOW_ALPHA 0x1E    /* strength of each layer (0-255) */

#define RUN_SECS 3                       /* a script may use this many seconds of CPU time */
#define MEM_LIMIT (64u * 1024u * 1024u)  /* Luau memory cap (bytes) */
#define OUT_CAP 65536                    /* max output captured per run */
#define OUT_LIMIT (OUT_CAP - 64)
#define CON_MAX 30000                    /* console is trimmed above this many chars... */
#define CON_KEEP 20000                   /* ...down to this many */
#define GUT_MAX 9999                     /* highest line number shown in the gutter */

#define LP  "android/view/WindowManager$LayoutParams"
#define LL  "android/widget/LinearLayout"
#define FL  "android/widget/FrameLayout"
#define TV  "android/widget/TextView"
#define VW  "android/view/View"
#define VG  "android/view/ViewGroup"
#define GD  "android/graphics/drawable/GradientDrawable"
#define VPA "android/view/ViewPropertyAnimator"
#define CTXC "android/content/Context"
#define CTX "(Landroid/content/Context;)V"
#define DRAGSIG "(Landroid/view/WindowManager;Landroid/view/WindowManager$LayoutParams;Landroid/view/View;)V"

static jobject g_wm, g_ctx, g_btn, g_panel, g_blp, g_plp, g_area, g_hl, g_edit, g_con, g_gut;
static jobject g_pages[3];   /* 0 = Home, 1 = Editor, 2 = Console */
static int g_started = 0, g_open = 0, g_attached = 0, g_closing = 0;
static int g_pivx = 0, g_pivy = 0, g_pw = DEF_PW, g_ph = DEF_PH, g_tab = 1;
static int g_lines = 1;

static const unsigned char HOME_PNG[] = {
#include "home_png.inc"
};

static const unsigned char NOTEPAD_PNG[] = {
#include "notepad_png.inc"
};

static const unsigned char CORKSCREW_PNG[] = {
#include "corkscrew_png.inc"
};

static const unsigned char CONSOLE_PNG[] = {
#include "console_png.inc"
};

/* ---------------- JNI helpers (each frees its temporary class ref) ---------------- */

static int vcall(JNIEnv *env, jobject o, const char *cls, const char *name, const char *sig, ...) {
    jclass c = env->FindClass(cls);
    jmethodID m = c ? env->GetMethodID(c, name, sig) : nullptr;
    if (c) env->DeleteLocalRef(c);
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
    if (c) env->DeleteLocalRef(c);
    if (!m) { env->ExceptionClear(); return nullptr; }
    va_list a; va_start(a, sig);
    jobject r = env->CallObjectMethodV(o, m, a);
    va_end(a);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return r;
}

static jint icall(JNIEnv *env, jobject o, const char *cls, const char *name, const char *sig) {
    jclass c = env->FindClass(cls);
    jmethodID m = c ? env->GetMethodID(c, name, sig) : nullptr;
    if (c) env->DeleteLocalRef(c);
    if (!m) { env->ExceptionClear(); return 0; }
    jint r = env->CallIntMethod(o, m);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return 0; }
    return r;
}

static jobject mk(JNIEnv *env, const char *cls, const char *sig, ...) {
    jclass c = env->FindClass(cls);
    jmethodID m = c ? env->GetMethodID(c, "<init>", sig) : nullptr;
    if (!m) { if (c) env->DeleteLocalRef(c); env->ExceptionClear(); return nullptr; }
    va_list a; va_start(a, sig);
    jobject o = env->NewObjectV(c, m, a);
    va_end(a);
    env->DeleteLocalRef(c);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return nullptr; }
    return o;
}

static void setI(JNIEnv *env, jobject o, const char *cls, const char *f, jint v) {
    jclass c = env->FindClass(cls);
    jfieldID id = c ? env->GetFieldID(c, f, "I") : nullptr;
    if (c) env->DeleteLocalRef(c);
    if (id) env->SetIntField(o, id, v); else env->ExceptionClear();
}

static jint getI(JNIEnv *env, jobject o, const char *cls, const char *f) {
    jclass c = env->FindClass(cls);
    jfieldID id = c ? env->GetFieldID(c, f, "I") : nullptr;
    if (c) env->DeleteLocalRef(c);
    if (!id) { env->ExceptionClear(); return 0; }
    return env->GetIntField(o, id);
}

static int clampi(int v, int lo, int hi) {
    if (hi < lo) hi = lo;
    return v < lo ? lo : (v > hi ? hi : v);
}

/* hidden pages are INVISIBLE (4), not GONE, so they stay laid out */
static void vis(JNIEnv *env, jobject v, int on) {
    vcall(env, v, VW, "setVisibility", "(I)V", on ? 0 : 4);
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

/* The Editor tab needs a focusable window (keyboard + clipboard access).
   Other tabs stay NOT_FOCUSABLE so the game keeps input.
   32 = NOT_TOUCH_MODAL, 40 = NOT_TOUCH_MODAL | NOT_FOCUSABLE */
static void applyFocus(JNIEnv *env) {
    setI(env, g_plp, LP, "flags", g_tab == 2 ? 32 : 40);
    if (g_attached && g_open)
        vcall(env, g_wm, "android/view/ViewManager", "updateViewLayout",
            "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V", g_panel, g_plp);
}

/* snap every page + the selector bar to the current tab (no animation) */
static void resetPages(JNIEnv *env) {
    for (int i = 0; i < 3; i++) {
        jobject p = g_pages[i];
        if (!p) continue;
        jobject a = ocall(env, p, VW, "animate", "()L" VPA ";");
        if (a) vcall(env, a, VPA, "cancel", "()V");
        vcall(env, p, VW, "setTranslationY", "(F)V", 0.0);
        vcall(env, p, VW, "setAlpha", "(F)V", 1.0);
        vis(env, p, i + 1 == g_tab);
    }
    if (g_hl) {
        jobject a = ocall(env, g_hl, VW, "animate", "()L" VPA ";");
        if (a) vcall(env, a, VPA, "cancel", "()V");
        vcall(env, g_hl, VW, "setTranslationY", "(F)V", (double)((g_tab - 1) * TAB_H));
    }
}

/* reset panel to its normal, fully visible state */
static void panelFinal(JNIEnv *env) {
    jobject a = ocall(env, g_panel, VW, "animate", "()L" VPA ";");
    if (a) vcall(env, a, VPA, "cancel", "()V");
    vcall(env, g_panel, VW, "setScaleX", "(F)V", 1.0);
    vcall(env, g_panel, VW, "setScaleY", "(F)V", 1.0);
    vcall(env, g_panel, VW, "setAlpha", "(F)V", 1.0);
    resetPages(env);
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

/* animate vertical offset + alpha of a view */
static void animSlide(JNIEnv *env, jobject v, double ty, double al, jlong dur) {
    jobject a = ocall(env, v, VW, "animate", "()L" VPA ";");
    if (a) a = ocall(env, a, VPA, "translationY", "(F)L" VPA ";", ty);
    if (a) a = ocall(env, a, VPA, "alpha", "(F)L" VPA ";", al);
    if (a) a = ocall(env, a, VPA, "setDuration", "(J)L" VPA ";", dur);
    jobject ip = mk(env, "android/view/animation/DecelerateInterpolator", "()V");
    if (a && ip) ocall(env, a, VPA, "setInterpolator", "(Landroid/animation/TimeInterpolator;)L" VPA ";", ip);
}

/* selector bar slides to the new tab, content scrolls from the old page to the new one */
static void switchTab(JNIEnv *env, int from, int to) {
    int dir = to > from ? 1 : -1;           /* +1 = going down the list */
    int H = icall(env, g_area, VW, "getHeight", "()I");
    if (H <= 0) H = g_ph;

    for (int i = 0; i < 3; i++)             /* any page that isn't involved: hide it */
        if (i + 1 != from && i + 1 != to) vis(env, g_pages[i], 0);

    jobject in = g_pages[to - 1];
    jobject out = g_pages[from - 1];
    vcall(env, in, VW, "setTranslationY", "(F)V", (double)(dir * H));
    vcall(env, in, VW, "setAlpha", "(F)V", 0.0);
    vis(env, in, 1);

    animSlide(env, in, 0.0, 1.0, (jlong)TAB_ANIM);
    animSlide(env, out, (double)(-dir * H), 0.0, (jlong)TAB_ANIM);
    animSlide(env, g_hl, (double)((to - 1) * TAB_H), 1.0, (jlong)TAB_ANIM);
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

/* add child to a FrameLayout / ScrollView with width/height */
static void addf(JNIEnv *env, jobject parent, jobject child, int w, int h) {
    jobject lp = mk(env, "android/view/ViewGroup$LayoutParams", "(II)V", w, h);
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

/* rounded-rectangle background */
static void rounded(JNIEnv *env, jobject v, jint argb, double radius) {
    jobject g = mk(env, GD, "()V");
    if (!g) return;
    vcall(env, g, GD, "setColor", "(I)V", argb);
    vcall(env, g, GD, "setCornerRadius", "(F)V", radius);
    vcall(env, v, VW, "setBackground", "(Landroid/graphics/drawable/Drawable;)V", g);
}

/* monospace font */
static void mono(JNIEnv *env, jobject tv) {
    jclass tfc = env->FindClass("android/graphics/Typeface");
    jfieldID mf = tfc ? env->GetStaticFieldID(tfc, "MONOSPACE", "Landroid/graphics/Typeface;") : nullptr;
    if (mf) {
        jobject m = env->GetStaticObjectField(tfc, mf);
        if (m) vcall(env, tv, TV, "setTypeface", "(Landroid/graphics/Typeface;)V", m);
    }
    env->ExceptionClear();
    if (tfc) env->DeleteLocalRef(tfc);
}

/* press-down effect: the view shrinks while pressed and springs back on release.
   Built from a StateListAnimator (pressed state -> small, default state -> normal). */
static void pressFx(JNIEnv *env, jobject view) {
    jclass pc = env->FindClass("android/animation/PropertyValuesHolder");
    jclass oc = env->FindClass("android/animation/ObjectAnimator");
    if (!pc || !oc) { env->ExceptionClear(); return; }
    jmethodID ofF = env->GetStaticMethodID(pc, "ofFloat",
        "(Ljava/lang/String;[F)Landroid/animation/PropertyValuesHolder;");
    jmethodID ofP = env->GetStaticMethodID(oc, "ofPropertyValuesHolder",
        "(Ljava/lang/Object;[Landroid/animation/PropertyValuesHolder;)Landroid/animation/ObjectAnimator;");
    if (!ofF || !ofP) { env->ExceptionClear(); return; }
    jobject sla = mk(env, "android/animation/StateListAnimator", "()V");
    if (!sla) return;

    const char *props[2] = { "scaleX", "scaleY" };
    for (int s = 0; s < 2; s++) {           /* s = 0: pressed, s = 1: default */
        jfloat to = (s == 0) ? PRESS_SCALE : 1.0f;
        jfloatArray fa = env->NewFloatArray(1);
        if (!fa) return;
        env->SetFloatArrayRegion(fa, 0, 1, &to);

        jobjectArray hs = env->NewObjectArray(2, pc, nullptr);
        if (!hs) return;
        for (int p = 0; p < 2; p++) {
            jobject h = env->CallStaticObjectMethod(pc, ofF, env->NewStringUTF(props[p]), fa);
            if (env->ExceptionCheck() || !h) { env->ExceptionClear(); return; }
            env->SetObjectArrayElement(hs, p, h);
        }
        jobject anim = env->CallStaticObjectMethod(oc, ofP, view, hs);
        if (env->ExceptionCheck() || !anim) { env->ExceptionClear(); return; }
        ocall(env, anim, "android/animation/ObjectAnimator", "setDuration",
            "(J)Landroid/animation/ObjectAnimator;", (jlong)(s == 0 ? 90 : 140));

        jintArray st = env->NewIntArray(s == 0 ? 1 : 0);
        if (!st) return;
        if (s == 0) {
            jint pressed = 0x010100a7;      /* android.R.attr.state_pressed */
            env->SetIntArrayRegion(st, 0, 1, &pressed);
        }
        vcall(env, sla, "android/animation/StateListAnimator", "addState",
            "([ILandroid/animation/Animator;)V", st, anim);
    }
    vcall(env, view, VW, "setStateListAnimator", "(Landroid/animation/StateListAnimator;)V", sla);
}

/* a rounded button with press effect; id is sent to act() when clicked */
static jobject button(JNIEnv *env, jobject ctx, const char *text, jint color, int id) {
    jobject b = label(env, ctx, "android/widget/Button", text, 14.0);
    if (!b) return nullptr;
    vcall(env, b, TV, "setTransformationMethod", "(Landroid/text/method/TransformationMethod;)V", (jobject)nullptr);
    vcall(env, b, TV, "setMinWidth", "(I)V", 0);
    vcall(env, b, TV, "setMinHeight", "(I)V", 0);
    vcall(env, b, TV, "setMaxLines", "(I)V", 1);                    /* never wrap onto a second line */
    vcall(env, b, TV, "setAutoSizeTextTypeUniformWithConfiguration", "(IIII)V",
          9, 14, 1, 2);                                             /* shrink text to fit: 9sp to 14sp, 2 = sp */
    rounded(env, b, color, 18.0);
    vcall(env, b, VW, "setPadding", "(IIII)V", 24, 24, 24, 24);
    jobject cl = clicker(env, id);
    if (cl) vcall(env, b, VW, "setOnClickListener", "(Landroid/view/View$OnClickListener;)V", cl);
    pressFx(env, b);
    return b;
}

/* decode an embedded PNG (data, len) into a drawable of size x size px */
static jobject icon(JNIEnv *env, jobject ctx, const unsigned char *data, int len, int size) {
    jsize n = (jsize)len;
    jbyteArray arr = env->NewByteArray(n);
    if (!arr) return nullptr;
    env->SetByteArrayRegion(arr, 0, n, (const jbyte *)data);

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

/* put an embedded icon to the left of a TextView's text */
static void leftIcon(JNIEnv *env, jobject tv, jobject ctx, const unsigned char *png, int len, int size) {
    jobject ic = icon(env, ctx, png, len, size);
    if (!ic) return;
    vcall(env, tv, TV, "setCompoundDrawables",
        "(Landroid/graphics/drawable/Drawable;Landroid/graphics/drawable/Drawable;Landroid/graphics/drawable/Drawable;Landroid/graphics/drawable/Drawable;)V",
        ic, (jobject)nullptr, (jobject)nullptr, (jobject)nullptr);
    vcall(env, tv, TV, "setCompoundDrawablePadding", "(I)V", 16);
}

/* short pop-up message */
static void toast(JNIEnv *env, const char *msg) {
    jclass tc = env->FindClass("android/widget/Toast");
    jmethodID mt = tc ? env->GetStaticMethodID(tc, "makeText",
        "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;") : nullptr;
    if (!mt) { env->ExceptionClear(); return; }
    jobject t = env->CallStaticObjectMethod(tc, mt, g_ctx, env->NewStringUTF(msg), 0);
    if (env->ExceptionCheck() || !t) { env->ExceptionClear(); return; }
    vcall(env, t, "android/widget/Toast", "show", "()V");
    env->DeleteLocalRef(tc);
}

/* clipboard text as a CharSequence, or null (empty clipboard, or Android won't let us read it) */
static jobject clipText(JNIEnv *env) {
    jobject cm = ocall(env, g_ctx, CTXC, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;", env->NewStringUTF("clipboard"));
    if (!cm) return nullptr;
    jobject cd = ocall(env, cm, "android/content/ClipboardManager", "getPrimaryClip",
        "()Landroid/content/ClipData;");
    if (!cd) return nullptr;
    if (icall(env, cd, "android/content/ClipData", "getItemCount", "()I") < 1) return nullptr;
    jobject item = ocall(env, cd, "android/content/ClipData", "getItemAt",
        "(I)Landroid/content/ClipData$Item;", 0);
    if (!item) return nullptr;
    jobject cs = ocall(env, item, "android/content/ClipData$Item", "coerceToText",
        "(Landroid/content/Context;)Ljava/lang/CharSequence;", g_ctx);
    if (!cs) return nullptr;
    if (icall(env, cs, "java/lang/CharSequence", "length", "()I") == 0) return nullptr;
    return cs;
}

/* ---------------- Luau VM + console ---------------- */

static lua_State *g_L = nullptr;   /* the Luau state */
static lua_State *g_T = nullptr;   /* one long-lived script thread, so your globals survive between runs */
static size_t g_mem = 0;
static clock_t g_t0;
static unsigned g_tick = 0;
static int g_timedout = 0;
static char g_out[OUT_CAP];
static size_t g_outn = 0;
static int g_trunc = 0;

/* allocator with a memory cap, so a runaway script can't eat all the RAM */
static void *l_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud;
    size_t old = ptr ? osize : 0;
    if (nsize == 0) {
        if (ptr) { g_mem -= old; free(ptr); }
        return nullptr;
    }
    if (g_mem - old + nsize > MEM_LIMIT) return nullptr;
    void *p = realloc(ptr, nsize);
    if (p) g_mem = g_mem - old + nsize;
    return p;
}

static void outAdd(const char *s, size_t n) {
    size_t room = OUT_LIMIT - g_outn;
    if (n > room) { n = room; g_trunc = 1; }
    memcpy(g_out + g_outn, s, n);
    g_outn += n;
}
static void outStr(const char *s) { outAdd(s, strlen(s)); }

/* print(...): tab-separated, newline at the end, goes to the console buffer */
static int l_print(lua_State *L) {
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++) {
        size_t len = 0;
        const char *s = luaL_tolstring(L, i, &len);
        if (i > 1) outStr("\t");
        if (s) outAdd(s, len);
        lua_pop(L, 1);
    }
    outStr("\n");
    return 0;
}

/* called by Luau at safepoints: stops scripts that run too long (the UI thread would freeze otherwise) */
static void l_interrupt(lua_State *L, int gc) {
    if (gc >= 0) return;                               /* a GC event, not a safepoint */
    if (!g_timedout) {
        if ((++g_tick & 0xFFF) != 0) return;           /* only look at the clock every 4096 safepoints */
        if ((clock() - g_t0) <= (clock_t)(RUN_SECS * CLOCKS_PER_SEC)) return;
        g_timedout = 1;
    }
    lua_rawcheckstack(L, 1);                           /* room for the error message */
    luaL_error(L, "script timed out (%d s)", RUN_SECS);
}

static int initVM() {
    g_L = lua_newstate(l_alloc, nullptr);
    if (!g_L) return 0;
    luaL_openlibs(g_L);
    lua_pushcfunction(g_L, l_print, "print");          /* our print -> console */
    lua_setglobal(g_L, "print");
    lua_callbacks(g_L)->interrupt = l_interrupt;
    luaL_sandbox(g_L);                                 /* built-in libraries become read-only */
    g_T = lua_newthread(g_L);
    lua_ref(g_L, -1);                                  /* keep the thread alive forever */
    lua_pop(g_L, 1);
    luaL_sandboxthread(g_T);                           /* its own globals table */
    return 1;
}

/* compile + run a chunk of Luau source; output / errors go into the console buffer */
static void runLua(const char *code, size_t len, const char *chunk) {
    if (!g_L && !initVM()) {
        g_L = nullptr;
        outStr("error: could not start the Luau VM\n");
        return;
    }
    lua_State *T = g_T;
    g_t0 = clock();
    g_tick = 0;
    g_timedout = 0;

    size_t bsz = 0;
    char *bc = luau_compile(code, len, nullptr, &bsz);
    if (!bc) { outStr("error: out of memory while compiling\n"); return; }
    int rc = luau_load(T, chunk, bc, bsz, 0);
    free(bc);
    if (rc == 0) rc = lua_pcall(T, 0, 0, 0);
    if (rc != 0) {
        size_t n = 0;
        const char *m = lua_tolstring(T, -1, &n);
        outStr("error: ");
        if (m) outAdd(m, n); else outStr("(error object is not a string)");
        outStr("\n");
    }
    lua_settop(T, 0);
    g_timedout = 0;
}

/* UTF-8 (any bytes) -> Java String. Invalid bytes become U+FFFD instead of crashing JNI. */
static jstring jstr(JNIEnv *env, const char *s, size_t n) {
    jchar *buf = (jchar *)malloc((n + 1) * sizeof(jchar));
    if (!buf) return nullptr;
    size_t o = 0, i = 0;
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        unsigned cp;
        int len;
        if (c < 0x80)               { cp = c;        len = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { buf[o++] = 0xFFFD; i++; continue; }
        if (i + len > n) { buf[o++] = 0xFFFD; break; }
        int ok = 1;
        for (int k = 1; k < len; k++) {
            unsigned char cc = (unsigned char)s[i + k];
            if ((cc & 0xC0) != 0x80) { ok = 0; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { buf[o++] = 0xFFFD; i++; continue; }
        i += len;
        if (cp >= 0x10000) {
            if (cp > 0x10FFFF) { buf[o++] = 0xFFFD; continue; }
            cp -= 0x10000;
            buf[o++] = (jchar)(0xD800 + (cp >> 10));
            buf[o++] = (jchar)(0xDC00 + (cp & 0x3FF));
        } else {
            buf[o++] = (jchar)cp;
        }
    }
    jstring r = env->NewString(buf, (jsize)o);
    free(buf);
    return r;
}

/* scroll a TextView to its last line */
static void scrollEnd(JNIEnv *env, jobject tv) {
    jobject lay = ocall(env, tv, TV, "getLayout", "()Landroid/text/Layout;");
    if (!lay) return;
    int lines = icall(env, tv, TV, "getLineCount", "()I");
    jclass lc = env->FindClass("android/text/Layout");
    jmethodID lt = lc ? env->GetMethodID(lc, "getLineTop", "(I)I") : nullptr;
    if (lc) env->DeleteLocalRef(lc);
    if (!lt) { env->ExceptionClear(); return; }
    int top = env->CallIntMethod(lay, lt, lines);
    if (env->ExceptionCheck()) { env->ExceptionClear(); return; }
    int pad = icall(env, tv, TV, "getTotalPaddingTop", "()I")
            + icall(env, tv, TV, "getTotalPaddingBottom", "()I");
    int h = icall(env, tv, VW, "getHeight", "()I");
    int amt = top + pad - h;
    vcall(env, tv, VW, "scrollTo", "(II)V", 0, amt > 0 ? amt : 0);
}

/* push everything the script printed into the console TextView (one append per run) */
static void conFlush(JNIEnv *env) {
    if (!g_con || (g_outn == 0 && !g_trunc)) return;
    if (g_trunc) {
        const char *t = "\n[output truncated]\n";
        size_t tl = strlen(t);
        memcpy(g_out + g_outn, t, tl);
        g_outn += tl;
    }
    env->PushLocalFrame(32);
    jstring js = jstr(env, g_out, g_outn);
    if (js) vcall(env, g_con, TV, "append", "(Ljava/lang/CharSequence;)V", js);
    int len = icall(env, g_con, TV, "length", "()I");
    if (len > CON_MAX) {                                  /* keep the console from growing forever */
        jobject ed = ocall(env, g_con, TV, "getEditableText", "()Landroid/text/Editable;");
        if (ed) ocall(env, ed, "android/text/Editable", "delete", "(II)Landroid/text/Editable;",
                      0, len - CON_KEEP);
    }
    scrollEnd(env, g_con);
    env->PopLocalFrame(nullptr);
    g_outn = 0;
    g_trunc = 0;
}

/* run Java string `js` as a script, show the result in the Console tab */
static void runString(JNIEnv *env, jstring js, const char *chunk) {
    if (js) {
        const char *code = env->GetStringUTFChars(js, nullptr);
        jsize clen = env->GetStringUTFLength(js);
        if (code) {
            if (clen == 0) outStr("(nothing to run)\n");
            else runLua(code, (size_t)clen, chunk);
            env->ReleaseStringUTFChars(js, code);
        }
    }
    conFlush(env);
    if (g_tab != 3) {
        int old = g_tab;
        g_tab = 3;
        switchTab(env, old, 3);
        applyFocus(env);
    }
}

/* keep the line-number gutter in step with the editor (called when the text changes) */
static char g_gbuf[GUT_MAX * 6 + 16];

static void updateGutter(JNIEnv *env) {
    if (!g_edit || !g_gut) return;
    jobject cs = ocall(env, g_edit, TV, "getText", "()Ljava/lang/CharSequence;");
    if (!cs) return;
    jstring js = (jstring)ocall(env, cs, "java/lang/Object", "toString", "()Ljava/lang/String;");
    if (!js) return;
    jsize n = env->GetStringLength(js);
    const jchar *c = env->GetStringChars(js, nullptr);
    int lines = 1;
    if (c) {
        for (jsize i = 0; i < n; i++) if (c[i] == '\n') lines++;
        env->ReleaseStringChars(js, c);
    }
    if (lines == g_lines) return;
    g_lines = lines;

    int shown = lines > GUT_MAX ? GUT_MAX : lines;
    size_t o = 0;
    for (int i = 1; i <= shown; i++)
        o += (size_t)snprintf(g_gbuf + o, sizeof(g_gbuf) - o, i < shown ? "%d\n" : "%d", i);
    jstring gs = jstr(env, g_gbuf, o);
    if (gs) vcall(env, g_gut, TV, "setText", "(Ljava/lang/CharSequence;)V", gs);
}

/* Paste button: insert the clipboard at the cursor (replacing any selection) */
static void pasteClip(JNIEnv *env) {
    jobject cs = clipText(env);
    if (!cs) { toast(env, "Clipboard is empty or unavailable"); return; }
    jobject ed = ocall(env, g_edit, TV, "getEditableText", "()Landroid/text/Editable;");
    if (!ed) return;
    int len = icall(env, ed, "java/lang/CharSequence", "length", "()I");
    int s = icall(env, g_edit, TV, "getSelectionStart", "()I");
    int e = icall(env, g_edit, TV, "getSelectionEnd", "()I");
    if (s < 0) s = len;
    if (e < 0) e = len;
    int a = s < e ? s : e;
    int b = s < e ? e : s;
    ocall(env, ed, "android/text/Editable", "replace", "(IILjava/lang/CharSequence;)Landroid/text/Editable;",
          a, b, cs);
    int n = icall(env, cs, "java/lang/CharSequence", "length", "()I");
    vcall(env, g_edit, "android/widget/EditText", "setSelection", "(I)V", a + n);
}

/* Run Clipboard button: run the clipboard text directly, without touching the editor */
static void runClip(JNIEnv *env) {
    jobject cs = clipText(env);
    if (!cs) { toast(env, "Clipboard is empty or unavailable"); return; }
    jstring js = (jstring)ocall(env, cs, "java/lang/Object", "toString", "()Ljava/lang/String;");
    runString(env, js, "=clipboard");
}

/* ---------------- UI ---------------- */

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

    /* ---- floating round button: corkscrew icon (falls back to a plain "C") ---- */
    jobject btn = nullptr;
    jobject bic = icon(env, ctx, CORKSCREW_PNG, (int)sizeof(CORKSCREW_PNG), BS);
    if (bic) {
        btn = mk(env, "android/widget/ImageView", CTX, ctx);
        if (btn) {
            vcall(env, btn, "android/widget/ImageView", "setImageDrawable",
                "(Landroid/graphics/drawable/Drawable;)V", bic);
            vcall(env, btn, VW, "setPadding", "(IIII)V", BTN_PAD, BTN_PAD, BTN_PAD, BTN_PAD);
        }
    }
    if (!btn) {
        btn = label(env, ctx, TV, "C", 18.0);
        if (!btn) return 0;
        vcall(env, btn, TV, "setGravity", "(I)V", 17);
        vcall(env, btn, TV, "setTypeface", "(Landroid/graphics/Typeface;I)V", (jobject)nullptr, 1);
    }
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
    rounded(env, inner, (jint)0xFF202020u, 28.0);
    vcall(env, inner, VW, "setClipToOutline", "(Z)V", 1);   /* keep children inside the rounded corners */
    addw(env, root, inner, -1, 0, 1.0);

    /* header: corkscrew icon + title, and the X */
    jobject head = mk(env, LL, CTX, ctx);
    if (!head) return 0;
    vcall(env, head, LL, "setOrientation", "(I)V", 0);
    vcall(env, head, VW, "setPadding", "(IIII)V", 36, 14, 18, 6);
    jobject title = label(env, ctx, TV, "CorkScrew", 18.0);
    if (title) {
        vcall(env, title, TV, "setGravity", "(I)V", 16);
        vcall(env, title, TV, "setTypeface", "(Landroid/graphics/Typeface;I)V", (jobject)nullptr, 1);
        leftIcon(env, title, ctx, CORKSCREW_PNG, (int)sizeof(CORKSCREW_PNG), TITLE_ICON);
        addw(env, head, title, 0, -2, 1.0);
    }
    jobject x = label(env, ctx, TV, "X", 18.0);
    if (x) {
        vcall(env, x, TV, "setGravity", "(I)V", 17);
        vcall(env, x, VW, "setPadding", "(IIII)V", 40, 14, 40, 14);
        rounded(env, x, (jint)0xFF343434u, 22.0);
        jobject cl = clicker(env, 0);
        if (cl) vcall(env, x, VW, "setOnClickListener", "(Landroid/view/View$OnClickListener;)V", cl);
        pressFx(env, x);
        addw(env, head, x, -2, -2, 0.0);
    }
    addw(env, inner, head, -1, -2, 0.0);

    /* body: sidebar + content area */
    jobject body = mk(env, LL, CTX, ctx);
    if (!body) return 0;
    vcall(env, body, LL, "setOrientation", "(I)V", 0);

    /* sidebar = FrameLayout: sliding rounded selector bar behind the tab buttons */
    jobject sideF = mk(env, FL, CTX, ctx);
    if (!sideF) return 0;
    vcall(env, sideF, VW, "setBackgroundColor", "(I)V", (jint)0xFF2B2B2Bu);
    vcall(env, sideF, VW, "setPadding", "(IIII)V", 12, 12, 12, 12);

    jobject hl = mk(env, VW, CTX, ctx);
    if (!hl) return 0;
    rounded(env, hl, (jint)0xFF111111u, 22.0);
    addf(env, sideF, hl, -1, TAB_H);

    jobject side = mk(env, LL, CTX, ctx);
    if (!side) return 0;
    vcall(env, side, LL, "setOrientation", "(I)V", 1);
    const char *tabs[3] = { "Home", "Editor", "Console" };
    for (int i = 0; i < 3; i++) {
        jobject b = label(env, ctx, "android/widget/Button", tabs[i], 14.0);
        if (!b) continue;
        vcall(env, b, TV, "setTransformationMethod", "(Landroid/text/method/TransformationMethod;)V", (jobject)nullptr);
        vcall(env, b, VW, "setBackgroundColor", "(I)V", (jint)0);   /* transparent: the selector bar shows through */

        /* tab icons: Home, Editor, Console */
        if (i == 0) leftIcon(env, b, ctx, HOME_PNG, (int)sizeof(HOME_PNG), TAB_ICON);
        if (i == 1) leftIcon(env, b, ctx, NOTEPAD_PNG, (int)sizeof(NOTEPAD_PNG), TAB_ICON);
        if (i == 2) leftIcon(env, b, ctx, CONSOLE_PNG, (int)sizeof(CONSOLE_PNG), TAB_ICON);

        jobject cl = clicker(env, i + 1);
        if (cl) vcall(env, b, VW, "setOnClickListener", "(Landroid/view/View$OnClickListener;)V", cl);
        addw(env, side, b, -1, TAB_H, 0.0);
    }
    addf(env, sideF, side, -1, -1);
    addw(env, body, sideF, SIDE_W, -1, 0.0);

    /* content area = FrameLayout holding the three pages (it also clips the sliding) */
    jobject area = mk(env, FL, CTX, ctx);
    if (!area) return 0;
    vcall(env, area, VW, "setBackgroundColor", "(I)V", (jint)0xFF1A1A1Au);

    jobject pHome = label(env, ctx, TV, "Welcome to CorkScrew", 20.0);
    if (!pHome) return 0;
    vcall(env, pHome, TV, "setGravity", "(I)V", 17);
    vcall(env, pHome, VW, "setBackgroundColor", "(I)V", (jint)0xFF1A1A1Au);

    /* Editor page: scrollable [line numbers | code], then Paste / Clear / Run Clipboard / Run */
    jobject pEdit = mk(env, LL, CTX, ctx);
    if (!pEdit) return 0;
    vcall(env, pEdit, LL, "setOrientation", "(I)V", 1);
    vcall(env, pEdit, VW, "setBackgroundColor", "(I)V", (jint)0xFF1A1A1Au);
    vcall(env, pEdit, VW, "setPadding", "(IIII)V", 14, 14, 14, 14);

    jobject sv = mk(env, "android/widget/ScrollView", CTX, ctx);
    if (!sv) return 0;
    rounded(env, sv, (jint)0xFF111111u, 20.0);
    vcall(env, sv, VW, "setClipToOutline", "(Z)V", 1);
    vcall(env, sv, "android/widget/ScrollView", "setFillViewport", "(Z)V", 1);

    jobject row = mk(env, LL, CTX, ctx);
    if (!row) return 0;
    vcall(env, row, LL, "setOrientation", "(I)V", 0);

    /* gutter: 1 2 3 ... , same font/size as the editor so the lines match up */
    jobject gut = label(env, ctx, TV, "1", 15.0);
    if (!gut) return 0;
    mono(env, gut);
    vcall(env, gut, TV, "setTextColor", "(I)V", (jint)0xFF6A6A6Au);
    vcall(env, gut, TV, "setGravity", "(I)V", 53);                  /* right + top */
    vcall(env, gut, TV, "setIncludeFontPadding", "(Z)V", 0);
    vcall(env, gut, TV, "setMinWidth", "(I)V", 84);
    vcall(env, gut, VW, "setBackgroundColor", "(I)V", (jint)0xFF161616u);
    vcall(env, gut, VW, "setPadding", "(IIII)V", 12, 24, 16, 24);
    addw(env, row, gut, -2, -1, 0.0);

    jobject edit = label(env, ctx, "android/widget/EditText", "", 15.0);
    if (!edit) return 0;
    mono(env, edit);
    /* ghost text (hint): shows only while the editor is empty.
       "\xE2\x9D\xA4\xEF\xB8\x8F" is the red heart emoji */
    vcall(env, edit, TV, "setHint", "(Ljava/lang/CharSequence;)V",
        env->NewStringUTF("-- CorkScrew made by Nivalos with \xE2\x9D\xA4\xEF\xB8\x8F"));
    vcall(env, edit, TV, "setHintTextColor", "(I)V", (jint)0xFF777777u);
    vcall(env, edit, TV, "setGravity", "(I)V", 51);                 /* top-left */
    vcall(env, edit, TV, "setInputType", "(I)V", 0x20001);          /* text + multi-line */
    vcall(env, edit, TV, "setImeOptions", "(I)V", 0x12000000);      /* no fullscreen keyboard UI */
    vcall(env, edit, TV, "setHorizontallyScrolling", "(Z)V", 1);    /* no wrapping, so line numbers stay 1:1 */
    vcall(env, edit, TV, "setIncludeFontPadding", "(Z)V", 0);
    vcall(env, edit, VW, "setBackgroundColor", "(I)V", (jint)0);
    vcall(env, edit, VW, "setPadding", "(IIII)V", 16, 24, 24, 24);
    jobject watch = mk(env, "com/rokori/client/Watch", "()V");      /* tells us when the text changes */
    if (watch) vcall(env, edit, TV, "addTextChangedListener", "(Landroid/text/TextWatcher;)V", watch);
    addw(env, row, edit, 0, -1, 1.0);

    addf(env, sv, row, -1, -2);
    addw(env, pEdit, sv, -1, 0, 1.0);

    jobject bar = mk(env, LL, CTX, ctx);
    if (bar) {
        vcall(env, bar, LL, "setOrientation", "(I)V", 0);
        vcall(env, bar, VW, "setPadding", "(IIII)V", 0, 14, 0, 0);
        const char *bn[4] = { "Paste", "Clear", "Run Clipboard", "Run" };
        jint bcol[4] = { (jint)0xFF3A3A3Au, (jint)0xFF3A3A3Au, (jint)0xFF3949ABu, (jint)(0xFF000000u | ACCENT) };
        int bid[4] = { 8, 4, 9, 5 };       /* 8 = paste, 4 = clear, 9 = run clipboard, 5 = run */
        for (int i = 0; i < 4; i++) {
            if (i) {
                jobject gap = mk(env, VW, CTX, ctx);
                if (gap) addw(env, bar, gap, 12, 1, 0.0);
            }
            jobject b = button(env, ctx, bn[i], bcol[i], bid[i]);
            if (b) addw(env, bar, b, 0, -2, 1.0);
        }
        addw(env, pEdit, bar, -1, -2, 0.0);
    }

    /* Console page: Luau output + Clear button */
    jobject pCon = mk(env, LL, CTX, ctx);
    if (!pCon) return 0;
    vcall(env, pCon, LL, "setOrientation", "(I)V", 1);
    vcall(env, pCon, VW, "setBackgroundColor", "(I)V", (jint)0xFF1A1A1Au);
    vcall(env, pCon, VW, "setPadding", "(IIII)V", 14, 14, 14, 14);

    /* starting text (the newline makes the first print() start on its own line) */
    jobject con = label(env, ctx, TV, "Welcome to CorkScrew\n", 13.0);
    if (!con) return 0;
    mono(env, con);
    vcall(env, con, TV, "setTextColor", "(I)V", (jint)0xFFCFCFCFu);
    vcall(env, con, TV, "setGravity", "(I)V", 51);
    rounded(env, con, (jint)0xFF111111u, 20.0);
    vcall(env, con, VW, "setPadding", "(IIII)V", 24, 24, 24, 24);
    jclass smc = env->FindClass("android/text/method/ScrollingMovementMethod");   /* finger scrolling */
    jmethodID gi = smc ? env->GetStaticMethodID(smc, "getInstance", "()Landroid/text/method/MovementMethod;") : nullptr;
    if (gi) {
        jobject mm = env->CallStaticObjectMethod(smc, gi);
        if (mm && !env->ExceptionCheck())
            vcall(env, con, TV, "setMovementMethod", "(Landroid/text/method/MovementMethod;)V", mm);
    }
    env->ExceptionClear();
    addw(env, pCon, con, -1, 0, 1.0);

    jobject cbar = mk(env, LL, CTX, ctx);
    if (cbar) {
        vcall(env, cbar, LL, "setOrientation", "(I)V", 0);
        vcall(env, cbar, LL, "setGravity", "(I)V", 5);
        vcall(env, cbar, VW, "setPadding", "(IIII)V", 0, 14, 0, 0);
        jobject cclr = button(env, ctx, "Clear", (jint)0xFF3A3A3Au, 6);   /* 6 = clear console */
        if (cclr) addw(env, cbar, cclr, -2, -2, 0.0);
        addw(env, pCon, cbar, -1, -2, 0.0);
    }

    addf(env, area, pHome, -1, -1);
    addf(env, area, pEdit, -1, -1);
    addf(env, area, pCon, -1, -1);
    vis(env, pEdit, 0);   /* only Home shows at first */
    vis(env, pCon, 0);
    addw(env, body, area, 0, -1, 1.0);

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
    g_area = env->NewGlobalRef(area);
    g_hl = env->NewGlobalRef(hl);
    g_edit = env->NewGlobalRef(edit);
    g_gut = env->NewGlobalRef(gut);
    g_con = env->NewGlobalRef(con);
    g_pages[0] = env->NewGlobalRef(pHome);
    g_pages[1] = env->NewGlobalRef(pEdit);
    g_pages[2] = env->NewGlobalRef(pCon);
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
    resetPages(env);
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

/* 0 = X, 1 = Home, 2 = Editor, 3 = Console, 4 = Clear editor, 5 = Run, 6 = Clear console,
   7 = editor text changed, 8 = Paste, 9 = Run Clipboard */
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
    if (id == 6) {
        vcall(env, g_con, TV, "setText", "(Ljava/lang/CharSequence;)V", env->NewStringUTF(""));
        return;
    }
    if (id == 7) { updateGutter(env); return; }
    if (id == 8) { pasteClip(env); return; }
    if (id == 9) { runClip(env); return; }
    if (id == 5) {   /* Run: execute the editor text */
        jobject cs = ocall(env, g_edit, TV, "getText", "()Ljava/lang/CharSequence;");
        jstring js = cs ? (jstring)ocall(env, cs, "java/lang/Object", "toString", "()Ljava/lang/String;") : nullptr;
        runString(env, js, "=editor");
        return;
    }
    if (id < 1 || id > 3 || id == g_tab) return;
    int old = g_tab;
    g_tab = id;
    switchTab(env, old, id);
    applyFocus(env);
}

}  /* extern "C" */