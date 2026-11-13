#include <jni.h>

#define CHK() if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }

JNIEXPORT void JNICALL
Java_com_roblox_client_ActivityNativeMain_show(JNIEnv *env, jclass cls, jobject ctx) {
    jclass ctxC = (*env)->GetObjectClass(env, ctx);
    jmethodID gss = (*env)->GetMethodID(env, ctxC, "getSystemService",
        "(Ljava/lang/String;)Ljava/lang/Object;"); CHK();
    jobject wm = (*env)->CallObjectMethod(env, ctx, gss,
        (*env)->NewStringUTF(env, "window")); CHK();
    if (!wm) return;

    /* TextView with centered white "F" */
    jclass tvC = (*env)->FindClass(env, "android/widget/TextView"); CHK();
    jmethodID tvInit = (*env)->GetMethodID(env, tvC, "<init>", "(Landroid/content/Context;)V"); CHK();
    jobject tv = (*env)->NewObject(env, tvC, tvInit, ctx); CHK();
    (*env)->CallVoidMethod(env, tv, (*env)->GetMethodID(env, tvC, "setText",
        "(Ljava/lang/CharSequence;)V"), (*env)->NewStringUTF(env, "F")); CHK();
    (*env)->CallVoidMethod(env, tv, (*env)->GetMethodID(env, tvC, "setTextColor", "(I)V"), (jint)-1); CHK();
    (*env)->CallVoidMethod(env, tv, (*env)->GetMethodID(env, tvC, "setGravity", "(I)V"), 17); CHK();
    (*env)->CallVoidMethod(env, tv, (*env)->GetMethodID(env, tvC, "setTextSize", "(F)V"), (double)22.0); CHK();

    /* round background */
    jclass gdC = (*env)->FindClass(env, "android/graphics/drawable/GradientDrawable"); CHK();
    jobject gd = (*env)->NewObject(env, gdC, (*env)->GetMethodID(env, gdC, "<init>", "()V")); CHK();
    (*env)->CallVoidMethod(env, gd, (*env)->GetMethodID(env, gdC, "setShape", "(I)V"), 1); CHK(); /* OVAL */
    (*env)->CallVoidMethod(env, gd, (*env)->GetMethodID(env, gdC, "setColor", "(I)V"), (jint)0xFF2196F3); CHK();
    (*env)->CallVoidMethod(env, tv, (*env)->GetMethodID(env, tvC, "setBackground",
        "(Landroid/graphics/drawable/Drawable;)V"), gd); CHK();

    /* 150x150 px overlay, top-left gravity so x/y drag works */
    jclass lpC = (*env)->FindClass(env, "android/view/WindowManager$LayoutParams"); CHK();
    jobject lp = (*env)->NewObject(env, lpC, (*env)->GetMethodID(env, lpC, "<init>", "(IIIII)V"),
        150, 150, 2038, 8, -3); CHK();
    (*env)->SetIntField(env, lp, (*env)->GetFieldID(env, lpC, "gravity", "I"), 51); CHK();
    (*env)->SetIntField(env, lp, (*env)->GetFieldID(env, lpC, "x", "I"), 100); CHK();
    (*env)->SetIntField(env, lp, (*env)->GetFieldID(env, lpC, "y", "I"), 300); CHK();

    /* drag listener */
    jclass dC = (*env)->FindClass(env, "com/rokori/client/Drag"); CHK();
    jobject drag = (*env)->NewObject(env, dC, (*env)->GetMethodID(env, dC, "<init>",
        "(Landroid/view/WindowManager;Landroid/view/WindowManager$LayoutParams;)V"), wm, lp); CHK();
    (*env)->CallVoidMethod(env, tv, (*env)->GetMethodID(env, tvC, "setOnTouchListener",
        "(Landroid/view/View$OnTouchListener;)V"), drag); CHK();

    /* add */
    jclass vmC = (*env)->FindClass(env, "android/view/ViewManager"); CHK();
    (*env)->CallVoidMethod(env, wm, (*env)->GetMethodID(env, vmC, "addView",
        "(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V"), tv, lp); CHK();
}
