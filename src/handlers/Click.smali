.class public Lcom/rokori/client/Click;
.super Ljava/lang/Object;
.implements Landroid/view/View$OnClickListener;

.field private id:I

.method public constructor <init>(I)V
    .registers 2
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    iput p1, p0, Lcom/rokori/client/Click;->id:I
    return-void
.end method

.method public onClick(Landroid/view/View;)V
    .registers 3
    iget v0, p0, Lcom/rokori/client/Click;->id:I
    invoke-static {v0}, Lcom/roblox/client/ActivityNativeMain;->act(I)V
    return-void
.end method
