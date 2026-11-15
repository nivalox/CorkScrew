.class public Lcom/rokori/client/Watch;
.super Ljava/lang/Object;
.implements Landroid/text/TextWatcher;

.method public constructor <init>()V
    .registers 1
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    return-void
.end method

.method public afterTextChanged(Landroid/text/Editable;)V
    .registers 3
    const/4 v0, 0x7
    invoke-static {v0}, Lcom/roblox/client/ActivityNativeMain;->act(I)V
    return-void
.end method

.method public beforeTextChanged(Ljava/lang/CharSequence;III)V
    .registers 5
    return-void
.end method

.method public onTextChanged(Ljava/lang/CharSequence;III)V
    .registers 5
    return-void
.end method
