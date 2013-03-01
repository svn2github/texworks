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

#include <PDFBackend.h>


PDFLinkAnnotation::~PDFLinkAnnotation()
{
  if (_actionOnActivation)
    delete _actionOnActivation;
}

QPolygonF PDFLinkAnnotation::quadPoints() const
{
  if (_quadPoints.isEmpty())
    return QPolygonF(rect());
  // The PDF specs (1.7) state that: "QuadPoints should be ignored if any
  // coordinate in the array lies outside the region specified by Rect."
  foreach (QPointF p, _quadPoints) {
    if (!rect().contains(p))
      return QPolygonF(rect());
  }
  return _quadPoints;
}

void PDFLinkAnnotation::setActionOnActivation(PDFAction * const action)
{
  if (_actionOnActivation)
    delete _actionOnActivation;
  _actionOnActivation = action;
}

// Backend Rendering
// =================
// The `PDFPageProcessingThread` is a thread that processes background jobs.
// Each job is represented by a subclass of `PageProcessingRequest` and
// contains an `execute` method that performs the actual work.
PDFPageProcessingThread::PDFPageProcessingThread() :
_quit(false)
{
}

PDFPageProcessingThread::~PDFPageProcessingThread()
{
  _mutex.lock();
  _quit = true;
  _waitCondition.wakeAll();
  _mutex.unlock();
  wait();
}

void PDFPageProcessingThread::requestRenderPage(Page *page, QObject *listener, double xres, double yres, QRect render_box, bool cache)
{
  addPageProcessingRequest(new PageProcessingRenderPageRequest(page, listener, xres, yres, render_box, cache));
}

void PDFPageProcessingThread::requestLoadLinks(Page *page, QObject *listener)
{
  addPageProcessingRequest(new PageProcessingLoadLinksRequest(page, listener));
}

void PDFPageProcessingThread::addPageProcessingRequest(PageProcessingRequest * request)
{
  int i;

  if (!request)
    return;

  QMutexLocker locker(&(this->_mutex));
  // remove any instances of the given request type before adding the new one to
  // avoid processing it several times
  // **TODO:** Could it be that we require several concurrent versions of the
  //           same page?
  for (i = _workStack.size() - 1; i >= 0; --i) {
    if (*(_workStack[i]) == *request) {
      // Using deleteLater() doesn't work because we have no event queue in this
      // thread. However, since the object is still on the stack, it is still
      // sleeping and directly deleting it should therefore be safe.
      delete _workStack[i];
      _workStack.remove(i);
    }
  }

  _workStack.push(request);
  locker.unlock();
#ifdef DEBUG
  QString jobDesc;
  switch (request->type()) {
    case PageProcessingRequest::LoadLinks:
      {
        qDebug() << "new 'loading links request' for page" << request->page->pageNum();
      }
      break;
    case PageProcessingRequest::PageRendering:
      {
        PageProcessingRenderPageRequest * r = static_cast<PageProcessingRenderPageRequest*>(request);
        qDebug() << "new 'rendering page request' for page" << request->page->pageNum() << "and tile" << r->render_box;
      }
      break;
  }
#endif

  if (!isRunning())
    start();
  else
    _waitCondition.wakeOne();
}

void PDFPageProcessingThread::run()
{
  PageProcessingRequest * workItem;

  _mutex.lock();
  while (!_quit) {
    // mutex must be locked at start of loop
    if (_workStack.size() > 0) {
      workItem = _workStack.pop();
      _mutex.unlock();

#ifdef DEBUG
      qDebug() << "processing work item; remaining items:" << _workStack.size();
      _renderTimer.start();
#endif
      workItem->execute();
#ifdef DEBUG
      QString jobDesc;
      switch (workItem->type()) {
        case PageProcessingRequest::LoadLinks:
          jobDesc = QString::fromUtf8("loading links");
          break;
        case PageProcessingRequest::PageRendering:
          jobDesc = QString::fromUtf8("rendering page");
          break;
      }
      qDebug() << "finished " << jobDesc << "for page" << workItem->page->pageNum() << ". Time elapsed: " << _renderTimer.elapsed() << " ms.";
#endif

      // Delete the work item as it has fulfilled its purpose
      // Note that we can't delete it here or we might risk that some emitted
      // signals are invalidated; to ensure they reach their destination, we
      // need to call deleteLater(), which requires and event queue; thus, we
      // first move it to the main processing thread
      workItem->moveToThread(QApplication::instance()->thread());
      workItem->deleteLater();

      _mutex.lock();
    }
    else {
#ifdef DEBUG
      qDebug() << "going to sleep";
#endif
      _waitCondition.wait(&_mutex);
#ifdef DEBUG
      qDebug() << "waking up";
#endif
    }
  }
}


// Asynchronous Page Operations
// ----------------------------
//
// The `execute` functions here are called by the processing theread to perform
// background jobs such as page rendering or link loading. This alows the GUI
// thread to stay unblocked and responsive. The results of background jobs are
// posted as events to a `listener` which can be any subclass of `QObject`. The
// `listener` will need a custom `event` function that is capable of picking up
// on these events.

bool PageProcessingRequest::operator==(const PageProcessingRequest & r) const
{
  // TODO: Should we care about the listener here as well?
  return (type() == r.type() && page == r.page);
}

bool PageProcessingRenderPageRequest::operator==(const PageProcessingRequest & r) const
{
  if (r.type() != PageRendering)
    return false;
  const PageProcessingRenderPageRequest * rr = static_cast<const PageProcessingRenderPageRequest*>(&r);
  // TODO: Should we care about the listener here as well?
  return (xres == rr->xres && yres == rr->yres && render_box == rr->render_box && cache == rr->cache);
}

// ### Custom Event Types
// These are the events posted by `execute` functions.
const QEvent::Type PDFPageRenderedEvent::PageRenderedEvent = static_cast<QEvent::Type>( QEvent::registerEventType() );
const QEvent::Type PDFLinksLoadedEvent::LinksLoadedEvent = static_cast<QEvent::Type>( QEvent::registerEventType() );

bool PageProcessingRenderPageRequest::execute()
{
  // FIXME:
  // Aborting renders doesn't really work right now---the backend knows nothing
  // about the PDF scenes.
  //
  // Idea: Perhaps allow page render requests to provide a pointer to a function
  // that returns a `bool` value indicating if the request is still valid? Then
  // the `PDFPageGraphicsItem` could have a function that indicates if the item
  // is anywhere near a viewport.
  QImage rendered_page = page->renderToImage(xres, yres, render_box, cache);
  QCoreApplication::postEvent(listener, new PDFPageRenderedEvent(xres, yres, render_box, rendered_page));

  return true;
}

bool PageProcessingLoadLinksRequest::execute()
{
  QCoreApplication::postEvent(listener, new PDFLinksLoadedEvent(page->loadLinks()));
  return true;
}

// ### Cache for Rendered Images
uint qHash(const PDFPageTile &tile)
{
  // FIXME: This is a horrible, horrible hash function, but it is a good quick and dirty
  // implementation. Should come up with something that executes faster.
  QByteArray hash_string;
  QDataStream(&hash_string, QIODevice::WriteOnly) << tile.xres << tile.yres << tile.render_box << tile.page_num;
  return qHash(hash_string);
}


// PDF ABCs
// ========

// Document Class
// --------------
Document::Document(QString fileName):
  _numPages(-1)
{
  // Set cache for rendered pages to be 1GB. This is enough for 256 RGBA tiles
  // (1024 x 1024 pixels x 4 bytes per pixel).
  //
  // NOTE: The application seems to exceed 1 GB---usage plateaus at around 2GB. No idea why. Perhaps freed
  // blocks are not garbage collected?? Perhaps my math is off??
  _pageCache.setMaxCost(1024 * 1024 * 1024);
}

Document::~Document()
{
}

int Document::numPages() { return _numPages; }
PDFPageProcessingThread &Document::processingThread() { return _processingThread; }
PDFPageCache &Document::pageCache() { return _pageCache; }


// Page Class
// ----------
Page::Page(Document *parent, int at):
  _parent(parent),
  _n(at)
{
}

Page::~Page()
{
}

int Page::pageNum() { return _n; }

QImage *Page::getCachedImage(double xres, double yres, QRect render_box)
{
  return _parent->pageCache().object(PDFPageTile(xres, yres, render_box, _n));
}

void Page::asyncRenderToImage(QObject *listener, double xres, double yres, QRect render_box, bool cache)
{
  _parent->processingThread().requestRenderPage(this, listener, xres, yres, render_box, cache);
}

void Page::asyncLoadLinks(QObject *listener)
{
  _parent->processingThread().requestLoadLinks(this, listener);
}


// vim: set sw=2 ts=2 et

