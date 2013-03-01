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
#ifndef PDFBackend_H
#define PDFBackend_H

// FIXME: Thin the header inclusion down.
#include <QtCore>
#include <QImage>
#include <QApplication>
#include <QFlags>
#include <QColor>


// Backend Rendering
// =================

class Page;
class Document;


// FIXME: Annotations and Actions should probably be moved to separate files

// ABC for annotations
// Modelled after sec. 8.4.1 of the PDF 1.7 specifications
class PDFAnnotation
{
public:
  enum AnnotationFlag {
    Annotation_Default = 0x0,
    Annotation_Invisible = 0x1,
    Annotation_Hidden = 0x2,
    Annotation_Print = 0x4,
    Annotation_NoZoom = 0x8,
    Annotation_NoRotate = 0x10,
    Annotation_NoView = 0x20,
    Annotation_ReadOnly = 0x40,
    Annotation_Locked = 0x80,
    Annotation_ToggleNoView = 0x100,
    Annotation_LockedContents = 0x200
  };
  Q_DECLARE_FLAGS(AnnotationFlags, AnnotationFlag)

  enum AnnotationType {
    AnnotationTypeText, AnnotationTypeLink, AnnotationTypeFreeText,
    AnnotationTypeLine, AnnotationTypeSquare, AnnotationTypeCircle,
    AnnotationTypePolygon, AnnotationTypePolyLine, AnnotationTypeHighlight,
    AnnotationTypeUnderline, AnnotationTypeSquiggly, AnnotationTypeStrikeOut,
    AnnotationTypeStamp, AnnotationTypeCaret, AnnotationTypeInk,
    AnnotationTypePopup, AnnotationTypeFileAttachment, AnnotationTypeSound,
    AnnotationTypeMovie, AnnotationTypeWidget, AnnotationTypeScreen,
    AnnotationTypePrinterMark, AnnotationTypeTrapNet, AnnotationTypeWatermark,
    AnnotationType3D
  };
  
  PDFAnnotation() : _page(NULL) { }
  virtual ~PDFAnnotation() { }

  virtual AnnotationType type() const = 0;
  
  // Declare all the getter/setter methods virtual so derived classes can
  // override them
  virtual QRectF rect() const { return _rect; }
  virtual QString contents() const { return _contents; }
  virtual Page * page() const { return _page; }
  virtual QString name() const { return _name; }
  virtual QString lastModified() const { return _lastModified; }
  virtual QFlags<AnnotationFlags> flags() const { return _flags; }
  virtual QFlags<AnnotationFlags>& flags() { return _flags; }  
  virtual QColor color() const { return _color; }

  virtual void setRect(const QRectF rect) { _rect = rect; }
  virtual void setContents(const QString contents) { _contents = contents; }
  virtual void setPage(Page * page) { _page = page; }
  virtual void setName(const QString name) { _name = name; }
  virtual void setLastModified(const QString lastModified) { _lastModified = lastModified; }
  virtual void setColor(const QColor color) { _color = color; }

protected:
  QRectF _rect; // required
  QString _contents; // optional
  Page * _page; // optional; since PDF 1.3
  QString _name; // optional; since PDF 1.4
  QString _lastModified; // optional; since PDF 1.1
  // TODO: _flags, _appearance, _appearanceState, _border, _structParent, _optContent
  QFlags<AnnotationFlags> _flags;
  // QList<???> _appearance;
  // ??? _appearanceState;
  // ??? _border;
  QColor _color;
  // ??? _structParent;
  // ??? _optContent;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(PDFAnnotation::AnnotationFlags)

class PDFDestination
{
public:
  PDFDestination(const int page = -1) : _page(page) { }

  bool isValid() const { return _page >= 0; }

  int page() const { return _page; }
  void setPage(const int page) { _page = page; }

private:
  int _page;
  // FIXME: viewport, zoom, fitting, etc.
};

// TODO: Possibly merge ActionTypeGoTo, ActionTypeGoToR, ActionTypeGoToE
class PDFAction
{
public:
  enum ActionType {
    ActionTypeGoTo, ActionTypeGoToR, ActionTypeGoToE, ActionTypeLaunch,
    ActionTypeThread, ActionTypeURI, ActionTypeSound, ActionTypeMovie,
    ActionTypeHide, ActionTypeNamed, ActionTypeSubmitForm, ActionTypeResetForm,
    ActionTypeImportData, ActionTypeJavaScript, ActionTypeSetOCGState,
    ActionTypeRendition, ActionTypeTrans, ActionTypeGoTo3DView
  };

  virtual ActionType type() const = 0;
};

class PDFURIAction : public PDFAction
{
public:
  ActionType type() const { return ActionTypeURI; }
  PDFURIAction(const QUrl url) : _url(url), _isMap(false) { }
  
  // FIXME: handle _isMap (see PDF 1.7 specs)
  QUrl url() const { return _url; }

private:
  QUrl _url;
  bool _isMap;
};

class PDFGotoAction : public PDFAction
{
public:
  ActionType type() const { return ActionTypeGoTo; }
  PDFGotoAction(const PDFDestination destination = PDFDestination()) : _destination(destination) { }

  PDFDestination destination() const { return _destination; }
  void setDestination(const PDFDestination destination) { _destination = destination; }

private:
  PDFDestination _destination;
};

class PDFLaunchAction : public PDFAction
{
public:
  ActionType type() const { return ActionTypeLaunch; }
  PDFLaunchAction(const QString command) : _command(command) { }
  
  QString command() const { return _command; }
  void setCommand(const QString command) { _command = command; }

  // FIXME: handle newWindow, implement OS-specific extensions
private:
  QString _command;
  bool _newWindow;
};

class PDFLinkAnnotation : public PDFAnnotation
{
public:
  enum HighlightingMode { HighlightingNone, HighlightingInvert, HighlightingOutline, HighlightingPush };

  PDFLinkAnnotation() : PDFAnnotation(), _actionOnActivation(NULL) { }
  virtual ~PDFLinkAnnotation();
  
  AnnotationType type() const { return AnnotationTypeLink; };

  HighlightingMode highlightingMode() const { return _highlightingMode; }
  QPolygonF quadPoints() const;
  PDFAction * actionOnActivation() const { return _actionOnActivation; }

  void setHighlightingMode(const HighlightingMode mode) { _highlightingMode = mode; }
  void setQuadPoints(const QPolygonF quadPoints) { _quadPoints = quadPoints; }
  // Note: PDFLinkAnnotation takes ownership of PDFAction pointers
  void setActionOnActivation(PDFAction * const action);

private:
  // Note: the PA member of the link annotation dict is deliberately ommitted
  // because we don't support WebCapture at the moment
  // Note: The PDF specs include a "destination" field for LinkAnnotations;
  // In this implementation this case should be handled by a PDFGoToAction
  HighlightingMode _highlightingMode;
  QPolygonF _quadPoints;
  PDFAction * _actionOnActivation;
};

class PDFPageTile
{

public:
  // TODO:
  // We may want an application-wide cache instead of a document-specific cache
  // to keep memory usage down. This may require an additional piece of
  // information---the document that the page belongs to.
  PDFPageTile(double xres, double yres, QRect render_box, int page_num):
    xres(xres), yres(yres),
    render_box(render_box),
    page_num(page_num)
  {}

  double xres, yres;
  QRect render_box;
  int page_num;

  bool operator==(const PDFPageTile &other) const
  {
    return (xres == other.xres && yres == other.yres && render_box == other.render_box && page_num == other.page_num);
  }

};
// Need a hash function in order to allow `PDFPageTile` to be used as a key
// object for a `QCache`.
uint qHash(const PDFPageTile &tile);

// TODO: May need a full subclass to do things like return the nearest image if
// a specific resolution is not available.
class PDFPageCache : public QCache<PDFPageTile, QImage>
{
public:
  QReadWriteLock lock;
};

// FIXME: the program segfaults if the page is destroyed while a page processing
// request is executed. Note that using QSharedPointer doesn't help here as
// processing requests are usually initiated from the Page object in question
// which can only create a new QSharedPointer object which would interfere with
// QSharedPointer held by other objects (but inaccessible to the Page in
// question - see documentation of QSharedPointer)
class PageProcessingRequest : public QObject
{
  Q_OBJECT
  friend class PDFPageProcessingThread;

  // Protect c'tor and execute() so we can't access them except in derived
  // classes and friends
protected:
  PageProcessingRequest(Page *page, QObject *listener) : page(page), listener(listener) { }
  // Should perform whatever processing it is designed to do
  // Returns true if finished successfully, false otherwise
  virtual bool execute() = 0;

public:
  enum Type { PageRendering, LoadLinks };

  virtual ~PageProcessingRequest() { }
  virtual Type type() const = 0;

  Page *page;
  QObject *listener;
  
  virtual bool operator==(const PageProcessingRequest & r) const;
};


class PageProcessingRenderPageRequest : public PageProcessingRequest
{
  Q_OBJECT
  friend class PDFPageProcessingThread;

  // Protect c'tor and execute() so we can't access them except in derived
  // classes and friends
protected:
  PageProcessingRenderPageRequest(Page *page, QObject *listener, double xres, double yres, QRect render_box = QRect(), bool cache = false) :
    PageProcessingRequest(page, listener),
    xres(xres), yres(yres),
    render_box(render_box),
    cache(cache)
  {}

  bool execute();

public:
  Type type() const { return PageRendering; }
  
  virtual bool operator==(const PageProcessingRequest & r) const;

  double xres, yres;
  QRect render_box;
  bool cache;

};


class PDFPageRenderedEvent : public QEvent
{

public:
  PDFPageRenderedEvent(double xres, double yres, QRect render_rect, QImage rendered_page):
    QEvent(PageRenderedEvent),
    xres(xres), yres(yres),
    render_rect(render_rect),
    rendered_page(rendered_page)
  {}

  static const QEvent::Type PageRenderedEvent;

  const double xres, yres;
  const QRect render_rect;
  const QImage rendered_page;

};


class PageProcessingLoadLinksRequest : public PageProcessingRequest
{
  Q_OBJECT
  friend class PDFPageProcessingThread;

  // Protect c'tor and execute() so we can't access them except in derived
  // classes and friends
protected:
  PageProcessingLoadLinksRequest(Page *page, QObject *listener) : PageProcessingRequest(page, listener) { }
  bool execute();

public:
  Type type() const { return LoadLinks; }

};


class PDFLinksLoadedEvent : public QEvent
{

public:
  PDFLinksLoadedEvent(const QList< QSharedPointer<PDFLinkAnnotation> > links):
    QEvent(LinksLoadedEvent),
    links(links)
  {}

  static const QEvent::Type LinksLoadedEvent;

  const QList< QSharedPointer<PDFLinkAnnotation> > links;

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

  void requestRenderPage(Page *page, QObject *listener, double xres, double yres, QRect render_box = QRect(), bool cache = false);
  void requestLoadLinks(Page *page, QObject *listener);

  // add a processing request to the work stack
  // Note: request must have been created on the heap and must be in the scope
  // of this thread; use requestRenderPage() and requestLoadLinks() for that
  void addPageProcessingRequest(PageProcessingRequest * request);

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


// PDF ABCs
// ========
// This header file defines a set of Abstract Base Classes (ABCs) for PDF
// documents. Having a set of abstract classes allows tools like GUI viewers to
// be written that are agnostic to the library that provides the actual PDF
// implementation: Poppler, MuPDF, etc.

class Document
{
  friend class Page;

protected:
  int _numPages;
  PDFPageProcessingThread _processingThread;
  PDFPageCache _pageCache;

public:
  Document(QString fileName);
  virtual ~Document();

  int numPages();
  PDFPageProcessingThread& processingThread();
  PDFPageCache& pageCache();

  virtual Page *page(int at)=0;

};

class Page
{

protected:
  Document *_parent;
  const int _n;

public:
  Page(Document *parent, int at);
  virtual ~Page();

  int pageNum();
  virtual QSizeF pageSizeF()=0;

  virtual QImage renderToImage(double xres, double yres, QRect render_box = QRect(), bool cache = false)=0;
  virtual void asyncRenderToImage(QObject *listener, double xres, double yres, QRect render_box = QRect(), bool cache = false);

  // FIXME: take care that the links are destroyed - maybe use QSharedPointer?
  virtual QList< QSharedPointer<PDFLinkAnnotation> > loadLinks() = 0;
  virtual void asyncLoadLinks(QObject *listener);

  QImage *getCachedImage(double xres, double yres, QRect render_box = QRect());

};


// Backend Implementations
// =======================
// These provide library-specific concrete impelemntations of the abstract base
// classes defined here.
#ifdef USE_POPPLER
#include <backends/PopplerBackend.h> // Invokes GPL v2+ License
#endif
#ifdef USE_MUPDF
#include <backends/MuPDFBackend.h>   // Invokes GPL v3 License
#endif

#endif // End header guard
// vim: set sw=2 ts=2 et

