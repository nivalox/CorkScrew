.class public Lcom/rokori/client/Drag;
.super Ljava/lang/Object;
.implements Landroid/view/View$OnTouchListener;

.field private wm:Landroid/view/WindowManager;
.field private lp:Landroid/view/WindowManager$LayoutParams;
.field private target:Landroid/view/View;
.field private sx:I
.field private sy:I
.field private tx:F
.field private ty:F
.field private moved:Z

.method public constructor <init>(Landroid/view/WindowManager;Landroid/view/WindowManager$LayoutParams;Landroid/view/View;)V
    .registers 4
    invoke-direct {p0}, Ljava/lang/Object;-><init>()V
    iput-object p1, p0, Lcom/rokori/client/Drag;->wm:Landroid/view/WindowManager;
    iput-object p2, p0, Lcom/rokori/client/Drag;->lp:Landroid/view/WindowManager$LayoutParams;
    iput-object p3, p0, Lcom/rokori/client/Drag;->target:Landroid/view/View;
    return-void
.end method

.method public onTouch(Landroid/view/View;Landroid/view/MotionEvent;)Z
    .registers 10

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
    const/4 v2, 0x0
    iput-boolean v2, p0, Lcom/rokori/client/Drag;->moved:Z
    goto :done

    :not_down
    const/4 v2, 0x2
    if-ne v0, v2, :not_move

    invoke-virtual {p2}, Landroid/view/MotionEvent;->getRawX()F
    move-result v2
    iget v3, p0, Lcom/rokori/client/Drag;->tx:F
    sub-float/2addr v2, v3
    float-to-int v2, v2
    invoke-virtual {p2}, Landroid/view/MotionEvent;->getRawY()F
    move-result v3
    iget v4, p0, Lcom/rokori/client/Drag;->ty:F
    sub-float/2addr v3, v4
    float-to-int v3, v3

    invoke-static {v2}, Ljava/lang/Math;->abs(I)I
    move-result v4
    const/16 v5, 0x14
    if-gt v4, v5, :set_moved
    invoke-static {v3}, Ljava/lang/Math;->abs(I)I
    move-result v4
    if-le v4, v5, :check_moved

    :set_moved
    const/4 v4, 0x1
    iput-boolean v4, p0, Lcom/rokori/client/Drag;->moved:Z

    :check_moved
    iget-boolean v4, p0, Lcom/rokori/client/Drag;->moved:Z
    if-eqz v4, :done

    iget v4, p0, Lcom/rokori/client/Drag;->sx:I
    add-int/2addr v2, v4
    iput v2, v1, Landroid/view/WindowManager$LayoutParams;->x:I
    iget v4, p0, Lcom/rokori/client/Drag;->sy:I
    add-int/2addr v3, v4
    iput v3, v1, Landroid/view/WindowManager$LayoutParams;->y:I
    iget-object v2, p0, Lcom/rokori/client/Drag;->wm:Landroid/view/WindowManager;
    iget-object v4, p0, Lcom/rokori/client/Drag;->target:Landroid/view/View;
    invoke-interface {v2, v4, v1}, Landroid/view/ViewManager;->updateViewLayout(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V
    goto :done

    :not_move
    const/4 v2, 0x1
    if-ne v0, v2, :done
    iget-boolean v2, p0, Lcom/rokori/client/Drag;->moved:Z
    if-nez v2, :done
    invoke-static {}, Lcom/roblox/client/ActivityNativeMain;->toggle()V

    :done
    const/4 v0, 0x1
    return v0
.end method
