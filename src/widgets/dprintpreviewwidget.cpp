#include "dprintpreviewwidget.h"
#include "private/dprintpreviewwidget_p.h"
#include <QVBoxLayout>
#include <private/qprinter_p.h>
#include <QPicture>

DWIDGET_BEGIN_NAMESPACE

DPrintPreviewWidgetPrivate::DPrintPreviewWidgetPrivate(DPrintPreviewWidget *qq)
    : DObjectPrivate(qq)
{
}

void DPrintPreviewWidgetPrivate::init()
{
    Q_Q(DPrintPreviewWidget);

    graphicsView = new GraphicsView;
    graphicsView->setInteractive(false);
    graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    graphicsView->setViewportUpdateMode(QGraphicsView::SmartViewportUpdate);

    scene = new QGraphicsScene(graphicsView);
    scene->setBackgroundBrush(Qt::gray);
    graphicsView->setScene(scene);

    QVBoxLayout *layout = new QVBoxLayout(q);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(graphicsView);

    colorMode = previewPrinter->colorMode();
}

void DPrintPreviewWidgetPrivate::populateScene()
{
    for (auto *page : qAsConst(pages))
        scene->removeItem(page);
    qDeleteAll(pages);
    pages.clear();

    QSize paperSize = previewPrinter->pageLayout().fullRectPixels(previewPrinter->resolution()).size();
    QRect pageRect = previewPrinter->pageLayout().paintRectPixels(previewPrinter->resolution());

    int page = 1;
    for (int i = 0; i < targetPictures.size(); i++) {
        PageItem *item = new PageItem(page++, &targetPictures[i], paperSize, pageRect);
        item->setVisible(false);
        scene->addItem(item);
        pages.append(item);
    }
    if (!pages.isEmpty()) {
        currentPageNumber = 1;
        pages.at(0)->setVisible(true);
    }
    setPageRangeAll();
}

void DPrintPreviewWidgetPrivate::generatePreview()
{
    Q_Q(DPrintPreviewWidget);
    previewPrinter->setPreviewMode(true);
    Q_EMIT q->paintRequested(previewPrinter);
    previewPrinter->setPreviewMode(false);
    pictures = previewPrinter->getPrinterPages();
    generateTargetPictures();
    populateScene();
    scene->setSceneRect(scene->itemsBoundingRect());
    fitView();
}

void DPrintPreviewWidgetPrivate::generateTargetPictures()
{
    targetPictures.clear();

    for (auto *picture : qAsConst(pictures)) {
        QPicture target;
        QPainter painter;
        painter.begin(&target);
        painter.drawPicture(0, 0, *picture);
        //todo scale,black and white,watermarking,……
        painter.end();

        // 图像灰度处理
        grayscalePaint(*picture, target);

        targetPictures.append(target);
    }
}

void DPrintPreviewWidgetPrivate::fitView()
{
    QRectF target = scene->sceneRect();
    graphicsView->fitInView(target, Qt::KeepAspectRatio);


}

void DPrintPreviewWidgetPrivate::setPageRangeAll()
{
    int size = pages.size();
    for (int i = 1; i <= size; i++) {
        pageRange.append(i);
    }
}

void DPrintPreviewWidgetPrivate::setCurrentPage(int page)
{
    if (page < 1 || page > pages.count())
        return;

    int lastPage = currentPageNumber;
    currentPageNumber = page;

    if (lastPage != currentPageNumber && lastPage > 0 && lastPage <= pages.count()) {
        pages.at(lastPage - 1)->setVisible(false);
        pages.at(currentPageNumber - 1)->setVisible(true);
    }
}

void DPrintPreviewWidgetPrivate::grayscalePaint(const QPicture &picture, QPicture &target)
{
    if (colorMode == DPrinter::Color)
        return;

    QImage image(previewPrinter->pageLayout().fullRectPixels(previewPrinter->resolution()).size(), QImage::Format_Grayscale8);
    QPainter imageP;

    image.fill(Qt::transparent);
    imageP.begin(&image);
    imageP.drawPicture(0, 0, picture);
    imageP.end();

    image = imageGrayscale(&image);

    QPicture temp;
    QPainter tempP;

    tempP.begin(&temp);
    tempP.drawImage(previewPrinter->pageLayout().fullRectPixels(previewPrinter->resolution()), image);
    tempP.end();

    target = temp;
}

QImage DPrintPreviewWidgetPrivate::imageGrayscale(const QImage *origin)
{
    QImage newImage = QImage(origin->width(), origin->height(), QImage::Format_ARGB32);
    QColor newColor;

    for (int x = 0; x < newImage.width(); ++x) {
        for (int y = 0; y < newImage.height(); ++y) {
            newColor = QColor(origin->pixel(x,y));

            if (newColor == QColor(0, 0, 0, 255))
                newColor = QColor(255, 255, 255, 0);

            newImage.setPixel(x, y, newColor.rgba());
        }
    }

    return newImage;
}

DPrintPreviewWidget::DPrintPreviewWidget(DPrinter *printer, QWidget *parent)
    : QWidget(parent)
    , DObject(*new DPrintPreviewWidgetPrivate(this))
{
    Q_D(DPrintPreviewWidget);
    d->previewPrinter = printer;
    d->init();
}

void DPrintPreviewWidget::setVisible(bool visible)
{
    QWidget::setVisible(visible);
    if (visible) {
        updatePreview();
    }

}

void DPrintPreviewWidget::setPageRange(const QVector<int> &rangePages)
{
    Q_D(DPrintPreviewWidget);
    d->pageRange = rangePages;
}

int DPrintPreviewWidget::pagesCount()
{
    Q_D(DPrintPreviewWidget);
    return d->pages.size();
}

bool DPrintPreviewWidget::turnPageAble()
{
    Q_D(DPrintPreviewWidget);
    return d->pages.size() > 1;
}

void DPrintPreviewWidget::setColorMode(const QPrinter::ColorMode &colorMode)
{
    Q_D(DPrintPreviewWidget);

    d->colorMode = colorMode;
    d->generatePreview();
}

void DPrintPreviewWidget::updatePreview()
{
    Q_D(DPrintPreviewWidget);
    d->generatePreview();
    d->graphicsView->updateGeometry();
}

void DPrintPreviewWidget::turnFront()
{
    Q_D(DPrintPreviewWidget);
    if (d->currentPageNumber < 2)
        return;
    setCurrentPage(d->currentPageNumber - 1);
}

void DPrintPreviewWidget::turnBack()
{
    Q_D(DPrintPreviewWidget);
    if (d->currentPageNumber >= d->pages.size())
        return;
    setCurrentPage(d->currentPageNumber + 1);
}

void DPrintPreviewWidget::turnBegin()
{
    Q_D(DPrintPreviewWidget);
    if (d->pageRange.isEmpty())
        return;
    setCurrentPage(1);
}

void DPrintPreviewWidget::turnEnd()
{
    Q_D(DPrintPreviewWidget);
    if (d->pageRange.isEmpty())
        return;
    setCurrentPage(d->pages.size());
}

void DPrintPreviewWidget::setCurrentPage(int page)
{
    Q_D(DPrintPreviewWidget);
    if (page == d->currentPageNumber)
        return;
    d->setCurrentPage(page);
    Q_EMIT currentPageChanged(page);
}

DPrinter::DPrinter(QPrinter::PrinterMode mode) : QPrinter(mode)
{
}

void DPrinter::setPreviewMode(bool isPreview)
{
    d_ptr->setPreviewMode(isPreview);
}

QList<const QPicture *> DPrinter::getPrinterPages()
{
    return d_ptr->previewPages();
}

void PageItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);
    QRectF paperRect(0, 0, paperSize.width(), paperSize.height());

    painter->setClipRect(paperRect & option->exposedRect);
    painter->fillRect(paperRect, Qt::white);

    if (!pagePicture)
        return;

    painter->drawPicture(pageRect.topLeft(), *pagePicture);

    painter->setPen(QPen(Qt::black, 0));
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(paperRect);
}
DWIDGET_END_NAMESPACE
