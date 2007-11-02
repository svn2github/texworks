#include "PDFDocument.h"
#include "TeXDocument.h"
#include "QTeXApp.h"
#include "QTeXUtils.h"

#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QPainter>
#include <QPaintEngine>
#include <QLabel>
#include <QScrollArea>
#include <QStyle>
#include <QDesktopWidget>
#include <QSettings>

#include <math.h>

const double kMaxScaleFactor = 8.0;
const double kMinScaleFactor = 0.125;

#pragma mark === PDFMagnifier ===

const int kMagnifierWidth = 400;
const int kMagnifierHeight = 300;
const int kMagFactor = 2;

PDFMagnifier::PDFMagnifier(QWidget *parent, double inDpi)
	: QLabel(parent)
	, page(NULL)
	, scaleFactor(kMagFactor)
	, dpi(inDpi)
	, imageDpi(0)
	, imagePage(NULL)
{
}

void PDFMagnifier::setPage(Poppler::Page *p, double scale)
{
	page = p;
	scaleFactor = scale * kMagFactor;
	if (page == NULL) {
		imagePage = NULL;
		image = QImage();
	}
	else {
		QWidget *parentWidget = qobject_cast<QWidget*>(parent());
		double newDpi = dpi * scaleFactor;
		QPoint newLoc = parentWidget->rect().topLeft() * kMagFactor;
		QSize newSize = parentWidget->rect().size() * kMagFactor;
		if (page != imagePage || newDpi != imageDpi || newLoc != imageLoc || newSize != imageSize)
			image = page->renderToImage(newDpi, newDpi,
										newLoc.x(), newLoc.y(),
										newSize.width(), newSize.height());
		imagePage = page;
		imageDpi = newDpi;
		imageLoc = newLoc;
		imageSize = newSize;
	}
	update();
}

void PDFMagnifier::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    drawFrame(&painter);
	painter.drawImage(event->rect(), image,
		event->rect().translated(x() * kMagFactor + kMagnifierWidth / 2,
								 y() * kMagFactor + kMagnifierHeight / 2));
}

#pragma mark === PDFWidget ===

PDFWidget::PDFWidget()
	: QLabel()
	, document(NULL)
	, page(NULL)
	, pageIndex(0)
	, scaleFactor(1.0)
	, dpi(72.0)
	, scaleOption(kFixedMag)
	, magnifier(NULL)
	, magnifying(false)
{
	dpi = QApplication::desktop()->logicalDpiX();
}

void PDFWidget::setDocument(Poppler::Document *doc)
{
	document = doc;
	reloadPage();
}

void PDFWidget::windowResized()
{
	switch (scaleOption) {
		case kFixedMag:
			break;
		case kFitWidth:
			fitWidth();
			break;
		case kFitWindow:
			fitWindow();
			break;
	}
}

void PDFWidget::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	drawFrame(&painter);

	double newDpi = dpi * scaleFactor;
	QRect newRect = rect();
	if (page != imagePage || newDpi != imageDpi || newRect != imageRect)
		image = page->renderToImage(dpi * scaleFactor, dpi * scaleFactor,
									rect().x(), rect().y(),
									rect().width(), rect().height());
	imagePage = page;
	imageDpi = newDpi;
	imageRect = newRect;

	painter.drawImage(event->rect(), image, event->rect());
}

void PDFWidget::mousePressEvent(QMouseEvent *event)
{
	image = page->renderToImage(dpi * scaleFactor, dpi * scaleFactor,
										rect().x(), rect().y(),
										rect().width(), rect().height());

	if (!magnifier)
		magnifier = new PDFMagnifier(this, dpi);
	magnifier->setPage(page, scaleFactor);
	magnifier->setFixedSize(kMagnifierWidth, kMagnifierHeight);
	magnifier->move(event->x() - kMagnifierWidth/2, event->y() - kMagnifierHeight/2);
	magnifier->show();
	event->accept();
}

void PDFWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if (magnifier)
		magnifier->close();
	event->accept();
}

void PDFWidget::mouseMoveEvent(QMouseEvent *event)
{
	if (magnifier)
		magnifier->move(event->x() - kMagnifierWidth/2, event->y() - kMagnifierHeight/2);
	event->accept();
}

void PDFWidget::adjustSize()
{
	if (page) {
		QSize	pageSize = (page->pageSizeF() * scaleFactor * dpi / 72.0).toSize();
		if (pageSize != size())
			resize(pageSize);
	}
}

void PDFWidget::reloadPage()
{
	if (page != NULL)
		delete page;
	if (magnifier != NULL)
		magnifier->setPage(NULL, 0);
	imagePage = NULL;
	image = QImage();
	page = document->page(pageIndex);
	adjustSize();
	update();
	updateStatusBar();
}

void PDFWidget::updateStatusBar()
{
	QWidget *widget = window();
	PDFDocument *doc = qobject_cast<PDFDocument*>(widget);
	if (doc) {
		doc->showPage(pageIndex+1);
		doc->showScale(scaleFactor);
	}
}

void PDFWidget::goFirst()
{
	if (pageIndex != 0) {
		pageIndex = 0;
		reloadPage();
		update();
	}
}

void PDFWidget::goPrev()
{
	if (pageIndex > 0) {
		--pageIndex;
		reloadPage();
		update();
	}
}

void PDFWidget::goNext()
{
	if (pageIndex < document->numPages() - 1) {
		++pageIndex;
		reloadPage();
		update();
	}
}

void PDFWidget::goLast()
{
	if (pageIndex != document->numPages() - 1) {
		pageIndex = document->numPages() - 1;
		reloadPage();
		update();
	}
}

void PDFWidget::doPageDialog()
{
}


void PDFWidget::actualSize()
{
	scaleOption = kFixedMag;
	if (scaleFactor != 1.0) {
		scaleFactor = 1.0;
		adjustSize();
		update();
		updateStatusBar();
	}
}

void PDFWidget::fitWidth()
{
	scaleOption = kFitWidth;
	QWidget *widget = window();
	PDFDocument*	doc = qobject_cast<PDFDocument*>(widget);
	if (doc) {
		QScrollArea*	scrollArea = qobject_cast<QScrollArea*>(doc->centralWidget());
		if (scrollArea) {
			double portWidth = scrollArea->viewport()->width();
			QSizeF	pageSize = page->pageSizeF() * dpi / 72.0;
			scaleFactor = portWidth / pageSize.width();
			if (scaleFactor < kMinScaleFactor)
				scaleFactor = kMinScaleFactor;
			else if (scaleFactor > kMaxScaleFactor)
				scaleFactor = kMaxScaleFactor;
			adjustSize();
			update();
			updateStatusBar();
		}
	}
}

void PDFWidget::fitWindow()
{
	scaleOption = kFitWindow;
	QWidget *widget = window();
	PDFDocument*	doc = qobject_cast<PDFDocument*>(widget);
	if (doc) {
		QScrollArea*	scrollArea = qobject_cast<QScrollArea*>(doc->centralWidget());
		if (scrollArea) {
			double portWidth = scrollArea->viewport()->width();
			double portHeight = scrollArea->viewport()->height();
			QSizeF	pageSize = page->pageSizeF() * dpi / 72.0;
			double sfh = portWidth / pageSize.width();
			double sfv = portHeight / pageSize.height();
			scaleFactor = sfh < sfv ? sfh : sfv;
			if (scaleFactor < kMinScaleFactor)
				scaleFactor = kMinScaleFactor;
			else if (scaleFactor > kMaxScaleFactor)
				scaleFactor = kMaxScaleFactor;
			adjustSize();
			update();
			updateStatusBar();
		}
	}
}

void PDFWidget::zoomIn()
{
	scaleOption = kFixedMag;
	if (scaleFactor < kMaxScaleFactor) {
		scaleFactor *= sqrt(2.0);
		if (fabs(scaleFactor - round(scaleFactor)) < 0.01)
			scaleFactor = round(scaleFactor);
		if (scaleFactor > kMaxScaleFactor)
			scaleFactor = kMaxScaleFactor;
		adjustSize();
		update();
		updateStatusBar();
	}
}

void PDFWidget::zoomOut()
{
	scaleOption = kFixedMag;
	if (scaleFactor > kMinScaleFactor) {
		scaleFactor /= sqrt(2.0);
		if (fabs(scaleFactor - round(scaleFactor)) < 0.01)
			scaleFactor = round(scaleFactor);
		if (scaleFactor < kMinScaleFactor)
			scaleFactor = kMinScaleFactor;
		adjustSize();
		update();
		updateStatusBar();
	}
}

#pragma mark === PDFDocument ===

QList<PDFDocument*> PDFDocument::docList;

PDFDocument::PDFDocument(const QString &fileName, TeXDocument *texDoc)
	: sourceDoc(texDoc)
{
	init();
	loadFile(fileName);
	stackUnder((QWidget*)texDoc);
}

PDFDocument::~PDFDocument()
{
	docList.removeAll(this);
}

void
PDFDocument::init()
{
	docList.append(this);

	setupUi(this);
	setAttribute(Qt::WA_DeleteOnClose, true);
	setAttribute(Qt::WA_MacNoClickThrough, true);
	
	pdfWidget = new PDFWidget;
	pdfWidget->setBackgroundRole(QPalette::Base);
	pdfWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	pdfWidget->setScaledContents(true);
	connect(this, SIGNAL(windowResized()), pdfWidget, SLOT(windowResized()));

	scaleLabel = new QLabel();
	statusBar()->addPermanentWidget(scaleLabel);
	scaleLabel->setFrameStyle(QFrame::StyledPanel);
	scaleLabel->setFont(statusBar()->font());

	pageLabel = new QLabel();
	statusBar()->addPermanentWidget(pageLabel);
	pageLabel->setFrameStyle(QFrame::StyledPanel);
	pageLabel->setFont(statusBar()->font());

	scrollArea = new QScrollArea;
	scrollArea->setBackgroundRole(QPalette::Dark);
	scrollArea->setWidget(pdfWidget);
	setCentralWidget(scrollArea);

	document = NULL;
	
	connect(actionAbout_QTeX, SIGNAL(triggered()), qApp, SLOT(about()));

	connect(actionNew, SIGNAL(triggered()), qApp, SLOT(newFile()));
	connect(actionOpen, SIGNAL(triggered()), qApp, SLOT(open()));

	connect(actionFirst_Page, SIGNAL(triggered()), pdfWidget, SLOT(goFirst()));
	connect(actionPrevious_Page, SIGNAL(triggered()), pdfWidget, SLOT(goPrev()));
	connect(actionNext_Page, SIGNAL(triggered()), pdfWidget, SLOT(goNext()));
	connect(actionLast_Page, SIGNAL(triggered()), pdfWidget, SLOT(goLast()));

	connect(actionActual_Size, SIGNAL(triggered()), pdfWidget, SLOT(actualSize()));
	connect(actionFit_to_Width, SIGNAL(triggered()), pdfWidget, SLOT(fitWidth()));
	connect(actionFit_to_Window, SIGNAL(triggered()), pdfWidget, SLOT(fitWindow()));
	connect(actionZoom_In, SIGNAL(triggered()), pdfWidget, SLOT(zoomIn()));
	connect(actionZoom_Out, SIGNAL(triggered()), pdfWidget, SLOT(zoomOut()));

	connect(actionTypeset, SIGNAL(triggered()), this, SLOT(retypeset()));
	
	connect(actionStack, SIGNAL(triggered()), qApp, SLOT(stackWindows()));
	connect(actionTile, SIGNAL(triggered()), qApp, SLOT(tileWindows()));
	connect(actionTile_Front_Two, SIGNAL(triggered()), qApp, SLOT(tileTwoWindows()));
	connect(actionGo_to_Source, SIGNAL(triggered()), this, SLOT(goToSource()));

	menuRecent = new QMenu(tr("Open Recent"));
	updateRecentFileActions();
	menuFile->insertMenu(actionOpen_Recent, menuRecent);
	menuFile->removeAction(actionOpen_Recent);

	connect(qApp, SIGNAL(recentFileActionsChanged()), this, SLOT(updateRecentFileActions()));
	connect(qApp, SIGNAL(windowListChanged()), this, SLOT(updateWindowMenu()));

	connect(actionPreferences, SIGNAL(triggered()), qApp, SLOT(preferences()));

	connect(this, SIGNAL(destroyed()), qApp, SLOT(updateWindowMenus()));

	QSettings settings;
	QTeXUtils::applyToolbarOptions(this, settings.value("toolBarIconSize", 2).toInt(), settings.value("toolBarShowText", false).toBool());
}
 
void PDFDocument::updateRecentFileActions()
{
	QTeXUtils::updateRecentFileActions(this, recentFileActions, menuRecent);
}

void PDFDocument::updateWindowMenu()
{
	QTeXUtils::updateWindowMenu(this, menuWindow);
}

void PDFDocument::selectWindow()
{
	show();
	raise();
	activateWindow();
}

void PDFDocument::resizeEvent(QResizeEvent *event)
{
	QMainWindow::resizeEvent(event);
	emit windowResized();
}

void PDFDocument::loadFile(const QString &fileName)
{
	setCurrentFile(fileName);
	reload();
}

void PDFDocument::reload()
{
	QApplication::setOverrideCursor(Qt::WaitCursor);

	if (document != NULL)
		delete document;

	document = Poppler::Document::load(curFile);
	document->setRenderBackend(Poppler::Document::SplashBackend);
	document->setRenderHint(Poppler::Document::Antialiasing);
	document->setRenderHint(Poppler::Document::TextAntialiasing);
	pdfWidget->setDocument(document);
	
	QApplication::restoreOverrideCursor();

	if (document == NULL)
		statusBar()->showMessage(tr("Failed to load file \"%1\"").arg(QTeXUtils::strippedName(curFile)));
}

void PDFDocument::setCurrentFile(const QString &fileName)
{
	curFile = QFileInfo(fileName).canonicalFilePath();

	setWindowTitle(tr("%1[*] - %2").arg(QTeXUtils::strippedName(curFile)).arg(tr("TeXWorks")));

	QTeXApp *app = qobject_cast<QTeXApp*>(qApp);
	if (app)
		app->updateWindowMenus();
}
 
PDFDocument *PDFDocument::findDocument(const QString &fileName)
{
	QString canonicalFilePath = QFileInfo(fileName).canonicalFilePath();

	foreach (QWidget *widget, qApp->topLevelWidgets()) {
		PDFDocument *theDoc = qobject_cast<PDFDocument*>(widget);
		if (theDoc && theDoc->curFile == canonicalFilePath)
			return theDoc;
	}
	return NULL;
}

void PDFDocument::zoomToRight(QWidget *otherWindow)
{
	QDesktopWidget *desktop = QApplication::desktop();
	QRect screenRect = desktop->availableGeometry(otherWindow == NULL ? this : otherWindow);
	screenRect.setTop(screenRect.top() + 22);
	screenRect.setLeft((screenRect.left() + screenRect.right()) / 2 + 1);
	screenRect.setBottom(screenRect.bottom() - 1);
	screenRect.setRight(screenRect.right() - 1);
	setGeometry(screenRect);
}

void PDFDocument::showPage(int page)
{
	pageLabel->setText(tr("page %1 of %2").arg(page).arg(document->numPages()));
}

void PDFDocument::showScale(double scale)
{
	scaleLabel->setText(tr("%1%").arg(round(scale * 10000.0) / 100.0));
}

void PDFDocument::retypeset()
{
	if (sourceDoc != NULL)
		sourceDoc->typeset();
}

void PDFDocument::goToSource()
{
	if (sourceDoc != NULL)
		sourceDoc->selectWindow();
}
