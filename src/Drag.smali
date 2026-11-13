.class public Lcom/rokori/client/Drag;
.super Ljava/lang/Object;
.implements Landroid/view/View$OnTouchListener;

.field private wm:Landroid/view/WindowManager;
.field private lp:Landroid/view/WindowManager$LayoutParams;
.field private sx:I
.field private sy:I
.field private tx:F
.field private ty:F

.method public constructor <init>(Landroid/view/WindowManager;Landroid/view/WindowManager$LayoutParams;)V
    .registers 3
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    iput-object p1, p0, Lcom/rokori/client/Drag;->wm:Landroid/view/WindowManager;
    iput-object p2, p0, Lcom/rokori/client/Drag;->lp:Landroid/view/WindowManager$LayoutParams;
    return-void
.end method

.method public onTouch(Landroid/view/View;Landroid/view/MotionEvent;)Z
    .registers 8

    invoke-virtual {p2}, Landroid/view/MotionEvent;->getAction()I
    move-result v0

    iget-object v1, p0, Lcom/rokori/client/Drag;->lp:Landroid/view/WindowManager$LayoutParams;

    if-nez v0, :not_down

    iget v2, v1, Landroid/view/WindowManager$LayoutParams;->x:I
    iput v2, p0, Lcom/rokori/client/Drag;->sx:I
    iget v2, v1, Landroid/view/WindowManager$LayoutParams;->y:I
    iput v2, p0, Lcom/rokori/client/Drag;->sy:I
    invoke-virtual {p2}, Landroid/view/MotionEvent;->getRawX()F
    move-result v2
    iput v2, p0, Lcom/rokori/client/Drag;->tx:F
    invoke-virtual {p2}, Landroid/view/MotionEvent;->getRawY()F
    move-result v2
    iput v2, p0, Lcom/rokori/client/Drag;->ty:F
    goto :done

    :not_down
    const/4 v2, 0x2
    if-ne v0, v2, :done

    invoke-virtual {p2}, Landroid/view/MotionEvent;->getRawX()F
    move-result v2
    iget v3, p0, Lcom/rokori/client/Drag;->tx:F
    sub-float/2addr v2, v3
    float-to-int v2, v2
    iget v3, p0, Lcom/rokori/client/Drag;->sx:I
    add-int/2addr v2, v3
    iput v2, v1, Landroid/view/WindowManager$LayoutParams;->x:I

    invoke-virtual {p2}, Landroid/view/MotionEvent;->getRawY()F
    move-result v2
    iget v3, p0, Lcom/rokori/client/Drag;->ty:F
    sub-float/2addr v2, v3
    float-to-int v2, v2
    iget v3, p0, Lcom/rokori/client/Drag;->sy:I
    add-int/2addr v2, v3
    iput v2, v1, Landroid/view/WindowManager$LayoutParams;->y:I

    iget-object v2, p0, Lcom/rokori/client/Drag;->wm:Landroid/view/WindowManager;
    invoke-interface {v2, p1, v1}, Landroid/view/ViewManager;->updateViewLayout(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    :done
    const/4 v0, 0x1
    return v0
.end method
