.class public Lcom/rokori/client/Tick;
.super Ljava/lang/Object;
.implements Ljava/lang/Runnable;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public run()V
    .registers 2
    const/16 v0, 0xb
    invoke-static {v0}, Lcom/roblox/client/ActivityNativeMain;->act(I)V
    return-void
.end method
