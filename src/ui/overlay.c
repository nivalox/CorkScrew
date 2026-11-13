#include <jni.h>
#include <stdarg.h>

static jobject g_wm, g_btn, g_panel, g_blp, g_plp, g_content;
static int g_started = 0, g_open = 0, g_attached = 0;

static int vcall(JNIEnv *env, jobject o, const char *cls, const char *name, const char *sig, ...) {
    jclass c = (*env)->FindClass(env, cls);
    jmethodID m = c ? (*env)->GetMethodID(env, c, name, sig) : 0;
    if (!m) { (*env)->ExceptionClear(env); return 0; }
    va_list a; va_start(a, sig);
    (*env)->CallVoidMethodV(env, o, m, a);
    va_end(a);
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
    return 1;
}

static jobject mk(JNIEnv *env, const char *cls, const char *sig, ...) {
    jclass c = (*env)->FindClass(env, cls);
    jmethodID m = c ? (*env)->GetMethodID(env, c, "<init>", sig) : 0;
    if (!m) { (*env)->ExceptionClear(env); return NULL; }
    va_list a; va_start(a, sig);
    jobject o = (*env)->NewObjectV(env, c, m, a);
    va_end(a);
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return NULL; }
    return o;
}

static void setI(JNIEnv *env, jobject o, const char *cls, const char *f, jint v) {
    jclass c = (*env)->FindClass(env, cls);
    jfieldID id = c ? (*env)->GetFieldID(env, c, f, "I") : 0;
    if (id) (*env)->SetIntField(env, o, id, v); else (*env)->ExceptionClear(env);
}

#define LP  "android/view/WindowManager$LayoutParams"
#define LL  "android/widget/LinearLayout"
#define TV  "android/widget/TextView"
#define VW  "android/view/View"
#define VG  "android/view/ViewGroup"
#define GD  "android/graphics/drawable/GradientDrawable"
#define CTX "(Landroid/content/Context;)V"
#define DRAGSIG "(Landroid/view/WindowManager;Landroid/view/WindowManager$LayoutParams;Landroid/view/View;)V"

static int addv(JNIEnv *env, jobject v, jobject lp) {
    return vcall(env, g_wm, "android/view/ViewManager", "addView",
        "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V", v, lp);
}
static void remv(JNIEnv *env, jobject v) {
    vcall(env, g_wm, "android/view/ViewManager", "removeView", "(Landroid/view/View;)V", v);
}
static void attach(JNIEnv *env) {
    g_attached = g_open ? addv(env, g_panel, g_plp) : addv(env, g_btn, g_blp);
}
static void detach(JNIEnv *env) {
    if (!g_attached) return;
    remv(env, g_open ? g_panel : g_btn);
    g_attached = 0;
}

/* add child to a LinearLayout with width/height/weight */
static void addw(JNIEnv *env, jobject parent, jobject child, int w, int h, double wt) {
    jobject lp = mk(env, "android/widget/LinearLayout$LayoutParams", "(IIF)V", w, h, wt);
    if (lp) vcall(env, parent, VG, "addView",
        "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V", child, lp);
}

static jobject label(JNIEnv *env, jobject ctx, const char *cls, const char *text, double size) {
    jobject t = mk(env, cls, CTX, ctx);
    if (!t) return NULL;
    vcall(env, t, TV, "setText", "(Ljava/lang/CharSequence;)V", (*env)->NewStringUTF(env, text));
    vcall(env, t, TV, "setTextColor", "(I)V", (jint)-1);
    vcall(env, t, TV, "setTextSize", "(F)V", size);
    return t;
}

static jobject clicker(JNIEnv *env, int id) {
    return mk(env, "com/rokori/client/Click", "(I)V", id);
}

static int build(JNIEnv *env, jobject ctx0) {
    jclass cc = (*env)->GetObjectClass(env, ctx0);
    jmethodID gac = (*env)->GetMethodID(env, cc, "getApplicationContext", "()Landroid/content/Context;");
    jobject ctx = gac ? (*env)->CallObjectMethod(env, ctx0, gac) : NULL;
    if (!ctx) { (*env)->ExceptionClear(env); ctx = ctx0; }

    jmethodID gss = (*env)->GetMethodID(env, (*env)->GetObjectClass(env, ctx), "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;");
    jobject wm = (*env)->CallObjectMethod(env, ctx, gss, (*env)->NewStringUTF(env, "window"));
    if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
    if (!wm) return 0;

    /* ---- floating round button ("C") ---- */
    jobject btn = label(env, ctx, TV, "C", 18.0);
    if (!btn) return 0;
    vcall(env, btn, TV, "setGravity", "(I)V", 17);
    vcall(env, btn, TV, "setTypeface", "(Landroid/graphics/Typeface;I)V", (jobject)NULL, 1);
    jobject gd = mk(env, GD, "()V");
    if (gd) {
        vcall(env, gd, GD, "setShape", "(I)V", 1);
        vcall(env, gd, GD, "setColor", "(I)V", (jint)0xFF2196F3u);
        vcall(env, btn, VW, "setBackground", "(Landroid/graphics/drawable/Drawable;)V", gd);
    }
    jobject blp = mk(env, LP, "(IIIII)V", 110, 110, 2038, 8, -3);
    if (!blp) return 0;
    setI(env, blp, LP, "gravity", 51);
    setI(env, blp, LP, "x", 100);
    setI(env, blp, LP, "y", 300);
    jobject drag = mk(env, "com/rokori/client/Drag", DRAGSIG, wm, blp, btn);
    if (drag) vcall(env, btn, VW, "setOnTouchListener", "(Landroid/view/View$OnTouchListener;)V", drag);

    /* ---- panel root ---- */
    jobject root = mk(env, LL, CTX, ctx);
    if (!root) return 0;
    vcall(env, root, LL, "setOrientation", "(I)V", 1);
    jobject pg = mk(env, GD, "()V");
    if (pg) {
        vcall(env, pg, GD, "setColor", "(I)V", (jint)0xF0202020u);
        vcall(env, pg, GD, "setCornerRadius", "(F)V", (double)28.0);
        vcall(env, root, VW, "setBackground", "(Landroid/graphics/drawable/Drawable;)V", pg);
    }

    /* header: title + X */
    jobject head = mk(env, LL, CTX, ctx);
    if (!head) return 0;
    vcall(env, head, LL, "setOrientation", "(I)V", 0);
    vcall(env, head, VW, "setPadding", "(IIII)V", 36, 10, 10, 10);
    jobject title = label(env, ctx, TV, "CorkScrew", 18.0);
    if (title) {
        vcall(env, title, TV, "setGravity", "(I)V", 16);
        vcall(env, title, TV, "setTypeface", "(Landroid/graphics/Typeface;I)V", (jobject)NULL, 1);
        addw(env, head, title, 0, -2, 1.0);
    }
    jobject x = label(env, ctx, TV, "X", 20.0);
    if (x) {
        vcall(env, x, VW, "setPadding", "(IIII)V", 36, 10, 36, 10);
        jobject cl = clicker(env, 0);
        if (cl) vcall(env, x, VW, "setOnClickListener", "(Landroid/view/View$OnClickListener;)V", cl);
        addw(env, head, x, -2, -2, 0.0);
    }
    addw(env, root, head, -1, -2, 0.0);

    /* body: side tabs + content */
    jobject body = mk(env, LL, CTX, ctx);
    if (!body) return 0;
    vcall(env, body, LL, "setOrientation", "(I)V", 0);

    jobject side = mk(env, LL, CTX, ctx);
    if (!side) return 0;
    vcall(env, side, LL, "setOrientation", "(I)V", 1);
    vcall(env, side, VW, "setBackgroundColor", "(I)V", (jint)0xFF2B2B2Bu);
    const char *tabs[3] = { "Home", "Tab 2", "Tab 3" };
    for (int i = 0; i < 3; i++) {
        jobject b = label(env, ctx, "android/widget/Button", tabs[i], 14.0);
        if (!b) continue;
        vcall(env, b, VW, "setBackgroundColor", "(I)V", (jint)0xFF3A3A3Au);
        jobject cl = clicker(env, i + 1);
        if (cl) vcall(env, b, VW, "setOnClickListener", "(Landroid/view/View$OnClickListener;)V", cl);
        addw(env, side, b, -1, -2, 0.0);
    }
    addw(env, body, side, 260, -1, 0.0);

    jobject content = label(env, ctx, TV, "Welcome to CorkScrew", 20.0);
    if (!content) return 0;
    vcall(env, content, TV, "setGravity", "(I)V", 17);
    vcall(env, content, VW, "setBackgroundColor", "(I)V", (jint)0xFF1A1A1Au);
    addw(env, body, content, 0, -1, 1.0);
    addw(env, root, body, -1, 0, 1.0);

    /* 900x600, NOT_FOCUSABLE | NOT_TOUCH_MODAL (40) so the game keeps focus */
    jobject plp = mk(env, LP, "(IIIII)V", 900, 600, 2038, 40, -3);
    if (!plp) return 0;
    setI(env, plp, LP, "gravity", 51);
    setI(env, plp, LP, "x", 60);
    setI(env, plp, LP, "y", 150);

    /* drag the whole panel by the top bar */
    jobject pdrag = mk(env, "com/rokori/client/Drag", DRAGSIG, wm, plp, root);
    if (pdrag) vcall(env, head, VW, "setOnTouchListener", "(Landroid/view/View$OnTouchListener;)V", pdrag);

    g_wm = (*env)->NewGlobalRef(env, wm);
    g_btn = (*env)->NewGlobalRef(env, btn);
    g_blp = (*env)->NewGlobalRef(env, blp);
    g_panel = (*env)->NewGlobalRef(env, root);
    g_plp = (*env)->NewGlobalRef(env, plp);
    g_content = (*env)->NewGlobalRef(env, content);
    return 1;
}

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
    detach(env);
}

/* tap on the floating button: hide it, open the panel */
JNIEXPORT void JNICALL
Java_com_roblox_client_ActivityNativeMain_toggle(JNIEnv *env, jclass cls) {
    if (!g_panel || !g_attached || g_open) return;
    detach(env); g_open = 1; attach(env);
}

/* 0 = X (close panel, bring button back), 1 = Home, 2/3 = other tabs */
JNIEXPORT void JNICALL
Java_com_roblox_client_ActivityNativeMain_act(JNIEnv *env, jclass cls, jint id) {
    if (!g_panel) return;
    if (id == 0) { detach(env); g_open = 0; attach(env); return; }
    vcall(env, g_content, TV, "setText", "(Ljava/lang/CharSequence;)V",
        (*env)->NewStringUTF(env, id == 1 ? "Welcome to CorkScrew" : "Coming Soon"));
}
