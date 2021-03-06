#include "danmurender.h"
#include "Layouts/rolllayout.h"
#include "Layouts/toplayout.h"
#include "Layouts/bottomlayout.h"
#include <QPair>
#include <QRandomGenerator>
#include "globalobjects.h"
#include "Play/Danmu/danmupool.h"
#include "Play/Playlist/playlist.h"
DanmuRender::DanmuRender()
{
    layout_table[0]=new RollLayout(this);
    layout_table[1]=new TopLayout(this);
    layout_table[2]=new BottomLayout(this);
    hideLayout[0]=hideLayout[1]=hideLayout[2]=false;
    bottomSubtitleProtect=true;
    topSubtitleProtect=false;
    dense=false;
    maxCount=-1;
    danmuOpacity=1;
    danmuStyle.strokeWidth=3.5;
    fontSizeTable[0]=20;
    fontSizeTable[1]=fontSizeTable[0]/1.5;
    fontSizeTable[2]=fontSizeTable[0]*1.5;
    danmuStyle.fontSizeTable=fontSizeTable;
    danmuStyle.fontFamily="Microsoft YaHei";
    danmuStyle.randomSize=false;
	danmuStyle.bold = false;
    QObject::connect(GlobalObjects::mpvplayer,&MPVPlayer::resized,this,&DanmuRender::refreshDMRect);

    cacheWorker=new CacheWorker(&danmuCache,&danmuStyle);
    cacheWorker->moveToThread(&cacheThread);
    QObject::connect(&cacheThread, &QThread::finished, cacheWorker, &QObject::deleteLater);
    QObject::connect(this,&DanmuRender::cacheDanmu,cacheWorker,&CacheWorker::beginCache);
    QObject::connect(cacheWorker,&CacheWorker::cacheDone,this,&DanmuRender::addDanmu);
    QObject::connect(this,&DanmuRender::danmuStyleChanged,cacheWorker,&CacheWorker::changeDanmuStyle);
    cacheThread.setObjectName(QStringLiteral("cacheThread"));
    cacheThread.start(QThread::NormalPriority);
}

DanmuRender::~DanmuRender()
{
    delete layout_table[0];
    delete layout_table[1];
    delete layout_table[2];
    cacheThread.quit();
    cacheThread.wait();
    for(auto iter=danmuCache.cbegin();iter!=danmuCache.cend();++iter)
        delete iter.value();
    DanmuObject::DeleteObjPool();
}

void DanmuRender::drawDanmu(QPainter &painter)
{
    painter.setOpacity(danmuOpacity);
    if(!hideLayout[DanmuComment::Rolling])layout_table[DanmuComment::Rolling]->drawLayout(painter);
    if(!hideLayout[DanmuComment::Top])layout_table[DanmuComment::Top]->drawLayout(painter);
    if(!hideLayout[DanmuComment::Bottom])layout_table[DanmuComment::Bottom]->drawLayout(painter);
}

void DanmuRender::moveDanmu(float interval)
{
    layout_table[DanmuComment::Rolling]->moveLayout(interval);
    layout_table[DanmuComment::Top]->moveLayout(interval);
    layout_table[DanmuComment::Bottom]->moveLayout(interval);
}

void DanmuRender::cleanup(DanmuComment::DanmuType cleanType)
{
    layout_table[cleanType]->cleanup();
}

void DanmuRender::cleanup()
{
    layout_table[DanmuComment::Rolling]->cleanup();
    layout_table[DanmuComment::Top]->cleanup();
    layout_table[DanmuComment::Bottom]->cleanup();
}

QSharedPointer<DanmuComment> DanmuRender::danmuAt(QPointF point)
{
    auto dm(layout_table[DanmuComment::Top]->danmuAt(point));
    if(!dm.isNull())return dm;
    dm=layout_table[DanmuComment::Rolling]->danmuAt(point);
    if(!dm.isNull())return dm;
    return layout_table[DanmuComment::Bottom]->danmuAt(point);
}

void DanmuRender::removeBlocked()
{
    layout_table[DanmuComment::Rolling]->removeBlocked();
    layout_table[DanmuComment::Top]->removeBlocked();
    layout_table[DanmuComment::Bottom]->removeBlocked();
}

void DanmuRender::refreshDMRect()
{
    const QSize surfaceSize(GlobalObjects::mpvplayer->size());
    this->surfaceRect.setRect(0,0,surfaceSize.width(),surfaceSize.height());
    if(bottomSubtitleProtect)
    {
        this->surfaceRect.setBottom(surfaceSize.height()*0.85);
    }
    if(topSubtitleProtect)
    {
        this->surfaceRect.setTop(surfaceSize.height()*0.10);
    }
}

void DanmuRender::setBottomSubtitleProtect(bool bottomOn)
{
    bottomSubtitleProtect=bottomOn;
    refreshDMRect();
}

void DanmuRender::setTopSubtitleProtect(bool topOn)
{
    topSubtitleProtect=topOn;
    refreshDMRect();

}

void DanmuRender::setFontSize(int pt)
{
    fontSizeTable[0]=pt;
    fontSizeTable[1]=fontSizeTable[0]/1.5;
    fontSizeTable[2]=fontSizeTable[0]*1.5;
}

void DanmuRender::setBold(bool bold)
{
    danmuStyle.bold=bold;
    emit danmuStyleChanged();
}

void DanmuRender::setOpacity(float opacity)
{
    danmuOpacity=qBound(0.f,opacity,1.f);
}

void DanmuRender::setFontFamily(QString &family)
{
    danmuStyle.fontFamily=family;
    emit danmuStyleChanged();
}

void DanmuRender::setSpeed(float speed)
{
    static_cast<RollLayout *>(layout_table[0])->setSpeed(speed);
}

void DanmuRender::setStrokeWidth(float width)
{
    danmuStyle.strokeWidth=width;
}

void DanmuRender::setRandomSize(bool randomSize)
{
    danmuStyle.randomSize=randomSize;
}

void DanmuRender::setMaxDanmuCount(int count)
{
    maxCount=count;
}

void DanmuRender::addDanmu(PrepareList *newDanmu)
{
    if(GlobalObjects::playlist->getCurrentItem()!=nullptr)
    {
        for(auto &danmuInfo:*newDanmu)
        {
            layout_table[danmuInfo.first->type]->addDanmu(danmuInfo.first,danmuInfo.second);
            if(maxCount!=-1)
            {
                if(layout_table[DanmuComment::Rolling]->danmuCount()+
                   layout_table[DanmuComment::Top]->danmuCount()+
                   layout_table[DanmuComment::Bottom]->danmuCount()>maxCount)
                    break;
            }
        }
    }
    GlobalObjects::danmuPool->recyclePrepareList(newDanmu);
}

CacheWorker::CacheWorker(QHash<QString, DanmuDrawInfo *> *cache, const DanmuStyle *style):
    danmuCache(cache),danmuStyle(style)
{
    danmuFont.setFamily(danmuStyle->fontFamily);
    danmuStrokePen.setWidthF(danmuStyle->strokeWidth);
    danmuStrokePen.setJoinStyle(Qt::RoundJoin);
    danmuStrokePen.setCapStyle(Qt::RoundCap);
}

DanmuDrawInfo *CacheWorker::createDanmuCache(const DanmuComment *comment)
{
    if(danmuStyle->randomSize)
        danmuFont.setPointSize(QRandomGenerator::global()->
                               bounded(danmuStyle->fontSizeTable[DanmuComment::FontSizeLevel::Small],
                               danmuStyle->fontSizeTable[DanmuComment::FontSizeLevel::Large]));
    else
        danmuFont.setPointSize(danmuStyle->fontSizeTable[comment->fontSizeLevel]);
    QFontMetrics metrics(danmuFont);
    danmuStrokePen.setWidthF(danmuStyle->strokeWidth);
    int strokeWidth=danmuStyle->strokeWidth;
    int left=qAbs(metrics.leftBearing(comment->text.front()));

    QSize size=metrics.size(0, comment->text)+QSize(strokeWidth*2+left,strokeWidth);
    QImage *img=new QImage(size, QImage::Format_ARGB32_Premultiplied);

    DanmuDrawInfo *drawInfo=new DanmuDrawInfo;
    drawInfo->useCount=0;
    drawInfo->height=size.height();
    drawInfo->width=size.width();
    drawInfo->img=img;

    QPainterPath path;
    QStringList multilines(comment->text.split('\n'));
    int py = qAbs((size.height() - metrics.height()*multilines.size()) / 2 + metrics.ascent());
    int i=0;
    for(const QString &line:multilines)
    {
        path.addText(left+strokeWidth,py+i*metrics.height(),danmuFont,line);
        ++i;
    }
    img->fill(Qt::transparent);
    QPainter painter(img);
    painter.setRenderHint(QPainter::Antialiasing);
    int r=comment->color>>16,g=(comment->color>>8)&0xff,b=comment->color&0xff;
    if(strokeWidth>0)
    {
        danmuStrokePen.setColor(comment->color==0x000000?Qt::white:Qt::black);
        painter.strokePath(path,danmuStrokePen);
        painter.drawPath(path);
    }
    painter.fillPath(path,QBrush(QColor(r,g,b)));
    painter.end();

    return drawInfo;
}

void CacheWorker::cleanCache()
{
#ifdef QT_DEBUG
    qDebug()<<"clean start, items:"<<danmuCache->size();
    QElapsedTimer timer;
    timer.start();
#endif
    for(auto iter=danmuCache->begin();iter!=danmuCache->end();)
    {
        if(iter.value()->useCount==0)
        {
            delete iter.value();
            iter=danmuCache->erase(iter);
        }
        else
        {
            ++iter;
        }
    }
#ifdef QT_DEBUG
    qDebug()<<"clean done:"<<timer.elapsed()<<"ms, left item:"<<danmuCache->size();
#endif
}

void CacheWorker::beginCache(PrepareList *danmus)
{
#ifdef QT_DEBUG
    qDebug()<<"cache start---";
    QElapsedTimer timer;
    timer.start();
    qint64 etime=0;
#endif
    for(QPair<QSharedPointer<DanmuComment>,DanmuDrawInfo*> &dm:*danmus)
    {
         QString hash_str(QString("%1%2%3").arg(dm.first->text).arg(dm.first->color).arg(danmuStyle->fontSizeTable[dm.first->fontSizeLevel]));
         DanmuDrawInfo *drawInfo(nullptr);
         if(danmuCache->contains(hash_str))
         {
             drawInfo=danmuCache->value(hash_str);
             drawInfo->useCountLock.lock();
             drawInfo->useCount++;
             drawInfo->useCountLock.unlock();
         }
         else
         {
             drawInfo=createDanmuCache(dm.first.data());
             drawInfo->useCount++;
             danmuCache->insert(hash_str,drawInfo);
         }
         dm.second=drawInfo;
#ifdef QT_DEBUG
         qDebug()<<"Gen cache:"<<dm.first->text<<" time: "<<dm.first->date;
#endif

    }
	if (danmuCache->size()>max_cache)
		cleanCache();
#ifdef QT_DEBUG
    etime=timer.elapsed();
    qDebug()<<"cache end, time: "<<etime<<"ms";
#endif
    emit cacheDone(danmus);
}

void CacheWorker::changeDanmuStyle()
{
    danmuFont.setFamily(danmuStyle->fontFamily);
    danmuFont.setBold(danmuStyle->bold);
}
