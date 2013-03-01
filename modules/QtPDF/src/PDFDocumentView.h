/**
 * Copyright (C) 2011  Charlie Sharpsteen, Stefan Löffler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#include <memory>
#include <QtGui/QtGui>
#include <poppler/qt4/poppler-qt4.h>

// Forward declare classes defined in this header.
class PDFDocumentScene;
class PDFPageGraphicsItem;
class PDFLinkGraphicsItem;
class PDFDocumentMagnifierView;

class PDFDocumentView : public QGraphicsView {
  Q_OBJECT
  typedef QGraphicsView Super;

  // **TODO:** _Replace with an overloaded `scene` method._
  PDFDocumentScene *_pdf_scene;
  PDFDocumentMagnifierView * _magnifier;

  qreal _zoomLevel;
  int _currentPage, _lastPage;

public:
  enum PageMode { PageMode_SinglePage, PageMode_OneColumnContinuous, PageMode_TwoColumnContinuous };
  enum MouseMode { MouseMode_MagnifyingGlass, MouseMode_Move };

  PDFDocumentView(QWidget *parent = 0);
  void setScene(PDFDocumentScene *a_scene);
  int currentPage();
  int lastPage();
  PageMode pageMode() const { return _pageMode; }
  qreal zoomLevel() const { return _zoomLevel; }

public slots:
  void goPrev();
  void goNext();
  void goFirst();
  void goLast();
  void goToPage(int pageNum);
  void setPageMode(PageMode pageMode);
  void setSinglePageMode() { setPageMode(PageMode_SinglePage); }
  void setOneColContPageMode() { setPageMode(PageMode_OneColumnContinuous); }
  void setTwoColContPageMode() { setPageMode(PageMode_TwoColumnContinuous); }
  void setMouseMode(const MouseMode newMode);

  void zoomIn();
  void zoomOut();

signals:
  void changedPage(int pageNum);
  void changedZoom(qreal zoomLevel);

protected:
  // Keep track of the current page by overloading the widget paint event.
  void paintEvent(QPaintEvent *event);
  void keyPressEvent(QKeyEvent *event);
  void mousePressEvent(QMouseEvent * event);
  void mouseMoveEvent(QMouseEvent * event);
  void mouseReleaseEvent(QMouseEvent * event);
  void moveTopLeftTo(const QPointF scenePos);

protected slots:
  void maybeUpdateSceneRect();

private:
  PageMode _pageMode;
  MouseMode _mouseMode;
  // Parent class has no copy constructor.
  Q_DISABLE_COPY(PDFDocumentView)
};

class PDFDocumentMagnifierView : public QGraphicsView {
  Q_OBJECT
  typedef QGraphicsView Super;

  PDFDocumentView * _parent_view;
  qreal _zoomLevel, _zoomFactor;

public:
  PDFDocumentMagnifierView(PDFDocumentView *parent = 0);
  // the zoom factor multiplies the parent view's _zoomLevel
  void setZoomFactor(const qreal zoomFactor);
  void setPosition(const QPoint pos);
  // ensures all settings are in sync with the parent view
  // make sure you call it before calling show()!
  // Note: we cannot override show() because prepareToShow() usually needs to be
  // called before setPosition as well (as it adjusts the region accessible in
  // setPosition())
  void prepareToShow();
};

class PageProcessingRequest : public QObject
{
  Q_OBJECT
  friend class PDFPageProcessingThread;

  // Protect c'tor and execute() so we can't access them except in derived
  // classes and friends
protected:
  PageProcessingRequest(PDFPageGraphicsItem * page) : page(page) { }
  // Should perform whatever processing it is designed to do
  // Returns true if finished successfully, false otherwise
  virtual bool execute() = 0;

public:
  enum Type { PageRendering, LoadLinks };

  virtual ~PageProcessingRequest() { }
  virtual Type type() = 0;
  
  PDFPageGraphicsItem * page;
};

class PageProcessingRenderPageRequest : public PageProcessingRequest
{
  Q_OBJECT
  friend class PDFPageProcessingThread;

  // Protect c'tor and execute() so we can't access them except in derived
  // classes and friends
protected:
  PageProcessingRenderPageRequest(PDFPageGraphicsItem * page, qreal scaleFactor);
  virtual bool execute();

public:
  virtual Type type() { return PageRendering; }

  qreal scaleFactor;

signals:
  void pageImageReady(qreal, QImage);
};

class PageProcessingLoadLinksRequest : public PageProcessingRequest
{
  Q_OBJECT
  friend class PDFPageProcessingThread;

  // Protect c'tor and execute() so we can't access them except in derived
  // classes and friends
protected:
  PageProcessingLoadLinksRequest(PDFPageGraphicsItem * page) : PageProcessingRequest(page) { }
  virtual bool execute();

public:
  virtual Type type() { return LoadLinks; }

signals:
  void linksReady(QList<PDFLinkGraphicsItem *>);
};


// Class to perform (possibly) lengthy operations on pages in the background
// Modelled after the "Blocking Fortune Client Example" in the Qt docs
// (http://doc.qt.nokia.com/stable/network-blockingfortuneclient.html)
class PDFPageProcessingThread : public QThread
{
  Q_OBJECT
public:
  PDFPageProcessingThread();
  virtual ~PDFPageProcessingThread();

  PageProcessingRenderPageRequest* requestRenderPage(PDFPageGraphicsItem * page, qreal scaleFactor);
  PageProcessingLoadLinksRequest* requestLoadLinks(PDFPageGraphicsItem * page);

protected:
  virtual void run();

private:
  QStack<PageProcessingRequest*> _workStack;
  QMutex _mutex;
  QWaitCondition _waitCondition;
  bool _quit;
#ifdef DEBUG
  QTime _renderTimer;
#endif
};

// Cannot use QGraphicsGridLayout and similar classes for pages because it only
// works for QGraphicsLayoutItem (i.e., QGraphicsWidget)
class PDFPageLayout : public QObject {
  Q_OBJECT
  struct LayoutItem {
    PDFPageGraphicsItem * page;
    int row;
    int col;
  };

  QList<LayoutItem> _layoutItems;
  int _numCols;
  int _firstCol;
  qreal _xSpacing; // spacing in pixel @ zoom=1
  qreal _ySpacing;
  bool _isContinuous;

public:
  PDFPageLayout();
  virtual ~PDFPageLayout() { }
  int columnCount() const { return _numCols; }
  int firstColumn() const { return _firstCol; }
  int xSpacing() const { return _xSpacing; }
  int ySpacing() const { return _ySpacing; }
  bool isContinuous() const { return _isContinuous; }
  void setContinuous(const bool continuous = true);

  void setColumnCount(const int numCols);
  void setColumnCount(const int numCols, const int firstCol);
  void setFirstColumn(const int firstCol);
  void setXSpacing(const qreal xSpacing);
  void setYSpacing(const qreal ySpacing);
  int rowCount() const;

  void addPage(PDFPageGraphicsItem * page);
  void removePage(PDFPageGraphicsItem * page);
  void insertPage(PDFPageGraphicsItem * page, PDFPageGraphicsItem * before = NULL);

public slots:
  void relayout();

signals:
  void layoutChanged(const QRectF sceneRect);

private:
  void rearrange();
  void continuousModeRelayout();
  void singlePageModeRelayout();
};


class PDFDocumentScene : public QGraphicsScene {
  Q_OBJECT
  typedef QGraphicsScene Super;

  const std::auto_ptr<Poppler::Document> _doc;

  // This may change to a `QSet` in the future
  QList<QGraphicsItem*> _pages;
  int _lastPage;
  PDFPageProcessingThread _processingThread;
  PDFPageLayout _pageLayout;

public:
  PDFDocumentScene(Poppler::Document *a_doc, QObject *parent = 0);
  QList<QGraphicsItem*> pages();
  QList<QGraphicsItem*> pages(const QPolygonF &polygon);
  QGraphicsItem* pageAt(const int idx);
  int pageNumAt(const QPolygonF &polygon);
  PDFPageProcessingThread& processingThread() { return _processingThread; }
  PDFPageLayout& pageLayout() { return _pageLayout; }

  void showOnePage(const int pageIdx) const;
  void showAllPages() const;

  int lastPage();
  // Poppler is *NOT* thread safe :(
  QMutex *docMutex;

signals:
  void pageChangeRequested(int pageNum);
  void pageLayoutChanged();

protected:
  bool event(QEvent* event);

protected slots:
  void pageLayoutChanged(const QRectF& sceneRect);

private:
  // Parent has no copy constructor, so this class shouldn't either. Also, we
  // hold some information in an `auto_ptr` which does interesting things on
  // copy that C++ newbies may not expect.
  Q_DISABLE_COPY(PDFDocumentScene)
};


// Also inherits QObject in order to access SIGNALS/SLOTS for `QFutureWatcher`.
// In the future, this should probably inherit `QGraphicsObject` directly, but
// since that was introduced only in Qt 4.6 it would break compilation on older
// systems for now
class PDFPageGraphicsItem : public QObject, public QGraphicsItem
{
  Q_OBJECT
  typedef QGraphicsItem Super;

  Poppler::Page *_page;
  QPixmap _renderedPage;
  QPixmap _temporaryPage;
  QPixmap _magnifiedPage;
  QPixmap _temporaryMagnifiedPage;
  double _dpiX;
  double _dpiY;
  QSizeF _pageSize;

  bool _linksLoaded;
  bool _pageIsRendering;

  QTransform _pageScale;
  qreal _zoomLevel, _magnifiedZoomLevel;

  friend class PageProcessingRenderPageRequest;
  friend class PageProcessingLoadLinksRequest;
  friend class PDFPageLayout;

public:

  PDFPageGraphicsItem(Poppler::Page *a_page, QGraphicsItem *parent = 0);

  // This seems fragile as it assumes no other code declaring a custom graphics
  // item will choose the same ID for it's object types. Unfortunately, there
  // appears to be no equivalent of `registerEventType` for `QGraphicsItem`
  // subclasses.
  enum { Type = UserType + 1 };
  int type() const;

  void paint( QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget);

  virtual QRectF boundingRect() const;

private:
  // Parent has no copy constructor.
  Q_DISABLE_COPY(PDFPageGraphicsItem)

private slots:
  void addLinks(QList<PDFLinkGraphicsItem *> links);
  void updateRenderedPage(qreal scaleFactor, QImage pageImage);
  void updateMagnifiedPage(qreal scaleFactor, QImage pageImage);
};


class PDFLinkGraphicsItem : public QGraphicsRectItem {
  typedef QGraphicsRectItem Super;

  Poppler::Link *_link;
  bool _activated;

public:
  PDFLinkGraphicsItem(Poppler::Link *a_link, QGraphicsItem *parent = 0);
  // See concerns in `PDFPageGraphicsItem` for why this feels fragile.
  enum { Type = UserType + 2 };
  int type() const;

protected:
  void hoverEnterEvent(QGraphicsSceneHoverEvent *event);
  void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);

  void mousePressEvent(QGraphicsSceneMouseEvent *event);
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

private:
  // Parent class has no copy constructor.
  Q_DISABLE_COPY(PDFLinkGraphicsItem)
};
// We need to declare a QList<PDFLinkGraphicsItem *> meta-type so we can
// pass it through inter-thread (i.e., queued) connections
Q_DECLARE_METATYPE(QList<PDFLinkGraphicsItem *>)


class PDFLinkEvent : public QEvent {
  typedef QEvent Super;

public:
  PDFLinkEvent(int a_page);
  static QEvent::Type LinkEvent;
  const int pageNum;
};

// vim: set sw=2 ts=2 et
