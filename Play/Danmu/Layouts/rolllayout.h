#ifndef ROLLLAYOUT_H
#define ROLLLAYOUT_H
#include "Play/Danmu/danmurender.h"
class RollLayout : public DanmuLayout
{
public:
    RollLayout(DanmuRender *render);

    virtual void addDanmu(QSharedPointer<DanmuComment> danmu,DanmuDrawInfo *drawInfo) override;
    virtual void moveLayout(float step) override;
    virtual void drawLayout(QPainter &painter) override;
    virtual QSharedPointer<DanmuComment> danmuAt(QPointF point) override;
    inline virtual int danmuCount(){return rolldanmu.count()+lastcol.count();}
    virtual void cleanup() override;
    virtual ~RollLayout();
    void setSpeed(float speed);
    virtual void removeBlocked();

private:
    QLinkedList<DanmuObject *> rolldanmu,lastcol;
    float base_speed;

    inline void moveLayoutList(QLinkedList<DanmuObject *> &list,float step);
    QSharedPointer<DanmuComment> danmuAtList(QPointF point,QLinkedList<DanmuObject *> &list);
    inline bool isCollided(const DanmuObject *d1, const DanmuObject *d2, float *collidedSpace);
};

#endif // ROLLLAYOUT_H
