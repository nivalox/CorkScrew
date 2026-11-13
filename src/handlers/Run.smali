.class public Lcom/rokori/client/Run;
.super Ljava/lang/Object;
.implements Ljava/lang/Runnable;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public run()V
    .registers 1
    invoke-static {}, Lcom/roblox/client/ActivityNativeMain;->closed()V
    return-void
.end method
