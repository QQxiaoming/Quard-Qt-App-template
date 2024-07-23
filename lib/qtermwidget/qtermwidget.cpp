/*  Copyright (C) 2008 e_k (e_k@users.sourceforge.net)

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include <QLayout>
#include <QBoxLayout>
#include <QtDebug>
#include <QDir>
#include <QMessageBox>
#include <QRegularExpression>

#include "ColorTables.h"
#include "Session.h"
#include "Screen.h"
#include "ScreenWindow.h"
#include "Emulation.h"
#include "TerminalDisplay.h"
#include "KeyboardTranslator.h"
#include "ColorScheme.h"
#include "SearchBar.h"
#include "qtermwidget.h"

#ifdef Q_OS_MACOS
// Qt does not support fontconfig on macOS, so we need to use a "real" font name.
#define DEFAULT_FONT_FAMILY                   "Menlo"
#else
#define DEFAULT_FONT_FAMILY                   "Monospace"
#endif

#define STEP_ZOOM 3

using namespace Konsole;

class TermWidgetImpl {
public:
    TermWidgetImpl(QWidget* parent = nullptr);

    TerminalDisplay *m_terminalDisplay;
    Session *m_session;

    Session* createSession(QWidget* parent);
    TerminalDisplay* createTerminalDisplay(Session *session, QWidget* parent);
};

TermWidgetImpl::TermWidgetImpl(QWidget* parent)
{
    this->m_session = createSession(parent);
    this->m_terminalDisplay = createTerminalDisplay(this->m_session, parent);
}


Session *TermWidgetImpl::createSession(QWidget* parent)
{
    Session *session = new Session(parent);

    session->setTitle(Session::NameRole, QLatin1String("QTermWidget"));

    QStringList args = QStringList(QString());

    session->setCodec(QStringEncoder{QStringConverter::Encoding::Utf8});

    session->setFlowControlEnabled(true);
    session->setHistoryType(HistoryTypeBuffer(1000));

    session->setDarkBackground(true);

    session->setKeyBindings(QString());
    return session;
}

TerminalDisplay *TermWidgetImpl::createTerminalDisplay(Session *session, QWidget* parent)
{
//    TerminalDisplay* display = new TerminalDisplay(this);
    TerminalDisplay* display = new TerminalDisplay(parent);

    display->setBellMode(TerminalDisplay::NotifyBell);
    display->setTerminalSizeHint(true);
    display->setTripleClickMode(TerminalDisplay::SelectWholeLine);
    display->setTerminalSizeStartup(true);

    display->setRandomSeed(session->sessionId() * 31);

    return display;
}

QTermWidget::QTermWidget(QWidget *messageParentWidget, QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    setLayout(m_layout);

    m_impl = new TermWidgetImpl(this);
    m_layout->addWidget(m_impl->m_terminalDisplay);
    m_impl->m_terminalDisplay->setObjectName("terminalDisplay");
    setMessageParentWidget(messageParentWidget?messageParentWidget:this);

    connect(m_impl->m_session, SIGNAL(bellRequest(QString)), m_impl->m_terminalDisplay, SLOT(bell(QString)));
    connect(m_impl->m_terminalDisplay, SIGNAL(notifyBell(QString)), this, SIGNAL(bell(QString)));
    connect(m_impl->m_terminalDisplay, SIGNAL(handleCtrlC()), this, SIGNAL(handleCtrlC()));
    connect(m_impl->m_terminalDisplay, SIGNAL(changedContentCountSignal(int,int)),this, SLOT(sizeChange(int,int)));
    connect(m_impl->m_terminalDisplay, SIGNAL(mousePressEventForwarded(QMouseEvent*)), this, SIGNAL(mousePressEventForwarded(QMouseEvent*)));
    connect(m_impl->m_session, SIGNAL(activity()), this, SIGNAL(activity()));
    connect(m_impl->m_session, SIGNAL(silence()), this, SIGNAL(silence()));
    connect(m_impl->m_session, &Session::profileChangeCommandReceived, this, &QTermWidget::profileChanged);
    connect(m_impl->m_session, &Session::receivedData, this, &QTermWidget::receivedData);
    connect(m_impl->m_session->emulation(), SIGNAL(zmodemRecvDetected()), this, SIGNAL(zmodemRecvDetected()) );
    connect(m_impl->m_session->emulation(), SIGNAL(zmodemSendDetected()), this, SIGNAL(zmodemSendDetected()) );
             
    // That's OK, FilterChain's dtor takes care of UrlFilter.
    urlFilter = new UrlFilter();
    connect(urlFilter, &UrlFilter::activated, this, &QTermWidget::urlActivated);
    m_impl->m_terminalDisplay->filterChain()->addFilter(urlFilter);
    m_UrlFilterEnable = true;

    m_searchBar = new SearchBar(this);
    m_searchBar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    connect(m_searchBar, SIGNAL(searchCriteriaChanged()), this, SLOT(find()));
    connect(m_searchBar, SIGNAL(findNext()), this, SLOT(findNext()));
    connect(m_searchBar, SIGNAL(findPrevious()), this, SLOT(findPrevious()));
    m_layout->addWidget(m_searchBar);
    m_searchBar->hide();
    QString style_sheet = qApp->styleSheet();
    m_searchBar->setStyleSheet(style_sheet);
    
    this->setFocus( Qt::OtherFocusReason );
    this->setFocusPolicy( Qt::WheelFocus );
    m_impl->m_terminalDisplay->resize(this->size());

    this->setFocusProxy(m_impl->m_terminalDisplay);
    connect(m_impl->m_terminalDisplay, SIGNAL(copyAvailable(bool)),
            this, SLOT(selectionChanged(bool)));
    connect(m_impl->m_terminalDisplay, SIGNAL(termGetFocus()),
            this, SIGNAL(termGetFocus()));
    connect(m_impl->m_terminalDisplay, SIGNAL(termLostFocus()),
            this, SIGNAL(termLostFocus()));
    connect(m_impl->m_terminalDisplay, &TerminalDisplay::keyPressedSignal, this,
            [this] (QKeyEvent* e, bool) { Q_EMIT termKeyPressed(e); });
//    m_impl->m_terminalDisplay->setSize(80, 40);

    QFont font = QApplication::font();
    font.setFamily(QLatin1String(DEFAULT_FONT_FAMILY));
    font.setPointSize(10);
    font.setStyleHint(QFont::TypeWriter);
    setTerminalFont(font);
    m_searchBar->setFont(font);

    setScrollBarPosition(NoScrollBar);
    setKeyboardCursorShape(Emulation::KeyboardCursorShape::BlockCursor);

    m_impl->m_session->addView(m_impl->m_terminalDisplay);

    connect(m_impl->m_session, SIGNAL(resizeRequest(QSize)), this, SLOT(setSize(QSize)));
    connect(m_impl->m_session, SIGNAL(finished()), this, SLOT(sessionFinished()));
    connect(m_impl->m_session, &Session::titleChanged, this, &QTermWidget::titleChanged);
    connect(m_impl->m_session, &Session::cursorChanged, this, &QTermWidget::cursorChanged);
}

void QTermWidget::selectionChanged(bool textSelected)
{
    emit copyAvailable(textSelected);
}

void QTermWidget::find()
{
    search(true, false);
}

void QTermWidget::findNext()
{
    search(true, true);
}

void QTermWidget::findPrevious()
{
    search(false, false);
}

void QTermWidget::search(bool forwards, bool next)
{
    int startColumn, startLine;

    if (next) // search from just after current selection
    {
        m_impl->m_terminalDisplay->screenWindow()->screen()->getSelectionEnd(startColumn, startLine);
        startColumn++;
    }
    else // search from start of current selection
    {
        m_impl->m_terminalDisplay->screenWindow()->screen()->getSelectionStart(startColumn, startLine);
    }

    //qDebug() << "current selection starts at: " << startColumn << startLine;
    //qDebug() << "current cursor position: " << m_impl->m_terminalDisplay->screenWindow()->cursorPosition();

    QRegularExpression regExp;
    if (m_searchBar->useRegularExpression()) {
        regExp.setPattern(m_searchBar->searchText());
    } else {
        regExp.setPattern(QRegularExpression::escape(m_searchBar->searchText()));
    }
    regExp.setPatternOptions(m_searchBar->matchCase() ? QRegularExpression::NoPatternOption : QRegularExpression::CaseInsensitiveOption);

    HistorySearch *historySearch =
            new HistorySearch(m_impl->m_session->emulation(), regExp, forwards, startColumn, startLine, this);
    connect(historySearch, SIGNAL(matchFound(int, int, int, int)), this, SLOT(matchFound(int, int, int, int)));
    connect(historySearch, SIGNAL(noMatchFound()), this, SLOT(noMatchFound()));
    connect(historySearch, SIGNAL(noMatchFound()), m_searchBar, SLOT(noMatchFound()));
    historySearch->search();
}


void QTermWidget::matchFound(int startColumn, int startLine, int endColumn, int endLine)
{
    ScreenWindow* sw = m_impl->m_terminalDisplay->screenWindow();
    //qDebug() << "Scroll to" << startLine;
    sw->scrollTo(startLine);
    sw->setTrackOutput(false);
    sw->notifyOutputChanged();
    sw->setSelectionStart(startColumn, startLine - sw->currentLine(), false);
    sw->setSelectionEnd(endColumn, endLine - sw->currentLine());
}

void QTermWidget::noMatchFound()
{
        m_impl->m_terminalDisplay->screenWindow()->clearSelection();
}

QSize QTermWidget::sizeHint() const
{
    QSize size = m_impl->m_terminalDisplay->sizeHint();
    size.rheight() = 150;
    return size;
}

void QTermWidget::setTerminalSizeHint(bool enabled)
{
    m_impl->m_terminalDisplay->setTerminalSizeHint(enabled);
}

bool QTermWidget::terminalSizeHint()
{
    return m_impl->m_terminalDisplay->terminalSizeHint();
}

void QTermWidget::startTerminalTeletype()
{
    m_impl->m_session->runEmptyPTY();
    // redirect data from TTY to external recipient
    connect( m_impl->m_session->emulation(), &Emulation::sendData, this, [this](const char *buff, int len) {
        if (m_echo) {
            recvData(buff, len);
        }
        emit sendData(buff, len);
    });
    connect( m_impl->m_session->emulation(), &Emulation::dupDisplayOutput, this, &QTermWidget::dupDisplayOutput);
}

QTermWidget::~QTermWidget()
{
    clearHighLightTexts();
    delete m_searchBar;
    delete m_impl;
    emit destroyed();
}


void QTermWidget::setTerminalFont(const QFont &font)
{
    m_impl->m_terminalDisplay->setVTFont(font);
}

QFont QTermWidget::getTerminalFont()
{
    return m_impl->m_terminalDisplay->getVTFont();
}

void QTermWidget::setTerminalOpacity(qreal level)
{
    m_impl->m_terminalDisplay->setOpacity(level);
}

void QTermWidget::setTerminalBackgroundImage(const QString& backgroundImage)
{
    m_impl->m_terminalDisplay->setBackgroundImage(backgroundImage);
}

void QTermWidget::setTerminalBackgroundMovie(const QString& backgroundMovie)
{
    m_impl->m_terminalDisplay->setBackgroundMovie(backgroundMovie);
}

void QTermWidget::setTerminalBackgroundVideo(const QString& backgroundVideo)
{
    m_impl->m_terminalDisplay->setBackgroundVideo(backgroundVideo);
}

void QTermWidget::setTerminalBackgroundMode(int mode)
{
    m_impl->m_terminalDisplay->setBackgroundMode((Konsole::BackgroundMode)mode);
}

void QTermWidget::setTextCodec(QStringEncoder codec)
{
    if (!m_impl->m_session)
        return;
    m_impl->m_session->setCodec(std::move(codec));
}

void QTermWidget::setColorScheme(const QString& origName)
{
    const ColorScheme *cs = nullptr;

    const bool isFile = QFile::exists(origName);
    const QString& name = isFile ?
            QFileInfo(origName).baseName() :
            origName;

    // avoid legacy (int) solution
    if (!availableColorSchemes().contains(name))
    {
        if (isFile)
        {
            if (ColorSchemeManager::instance()->loadCustomColorScheme(origName))
                cs = ColorSchemeManager::instance()->findColorScheme(name);
            else
                qWarning () << Q_FUNC_INFO
                        << "cannot load color scheme from"
                        << origName;
        }

        if (!cs)
            cs = ColorSchemeManager::instance()->defaultColorScheme();
    }
    else
        cs = ColorSchemeManager::instance()->findColorScheme(name);

    if (! cs)
    {
        QMessageBox::information(messageParentWidget,
                                 tr("Color Scheme Error"),
                                 tr("Cannot load color scheme: %1").arg(name));
        return;
    }
    ColorEntry table[TABLE_COLORS];
    cs->getColorTable(table);
    m_impl->m_terminalDisplay->setColorTable(table);
    m_impl->m_session->setDarkBackground(cs->hasDarkBackground());
}

QStringList QTermWidget::getAvailableColorSchemes()
{
   return QTermWidget::availableColorSchemes();
}

QStringList QTermWidget::availableColorSchemes()
{
    QStringList ret;
    const auto allColorSchemes = ColorSchemeManager::instance()->allColorSchemes();
    for (const ColorScheme* cs : allColorSchemes)
        ret.append(cs->name());
    return ret;
}

void QTermWidget::addCustomColorSchemeDir(const QString& custom_dir)
{
    ColorSchemeManager::instance()->addCustomColorSchemeDir(custom_dir);
}

void QTermWidget::setBackgroundColor(const QColor &color)
{
    m_impl->m_terminalDisplay->setBackgroundColor(color);
}

void QTermWidget::setForegroundColor(const QColor &color)
{
    m_impl->m_terminalDisplay->setForegroundColor(color);
}

void QTermWidget::setANSIColor(const int ansiColorId, const QColor &color)
{
    m_impl->m_terminalDisplay->setColorTableColor(ansiColorId, color);
}

void QTermWidget::setPreeditColorIndex(int index)
{
    m_impl->m_terminalDisplay->setPreeditColorIndex(index);
}

void QTermWidget::setSize(const QSize &size)
{
    m_impl->m_terminalDisplay->setSize(size.width(), size.height());
}

void QTermWidget::setHistorySize(int lines)
{
    if (lines < 0)
        m_impl->m_session->setHistoryType(HistoryTypeFile());
    else if (lines == 0)
        m_impl->m_session->setHistoryType(HistoryTypeNone());
    else
        m_impl->m_session->setHistoryType(HistoryTypeBuffer(lines));
}

int QTermWidget::historySize() const
{
    const HistoryType& currentHistory = m_impl->m_session->historyType();

     if (currentHistory.isEnabled()) {
         if (currentHistory.isUnlimited()) {
             return -1;
         } else {
             return currentHistory.maximumLineCount();
         }
     } else {
         return 0;
     }
}

void QTermWidget::setScrollBarPosition(ScrollBarPosition pos)
{
    m_impl->m_terminalDisplay->setScrollBarPosition(pos);
}

void QTermWidget::scrollToEnd()
{
    m_impl->m_terminalDisplay->scrollToEnd();
}

void QTermWidget::sendText(const QString &text)
{
    m_impl->m_session->sendText(text);
}

void QTermWidget::sendKeyEvent(QKeyEvent *e)
{
    m_impl->m_session->sendKeyEvent(e);
}

void QTermWidget::resizeEvent(QResizeEvent*)
{
//qDebug("global window resizing...with %d %d", this->size().width(), this->size().height());
    m_impl->m_terminalDisplay->resize(this->size());
}


void QTermWidget::sessionFinished()
{
    emit finished();
}

void QTermWidget::bracketText(QString& text)
{
    m_impl->m_terminalDisplay->bracketText(text);
}

void QTermWidget::disableBracketedPasteMode(bool disable)
{
    m_impl->m_terminalDisplay->disableBracketedPasteMode(disable);
}

bool QTermWidget::bracketedPasteModeIsDisabled() const
{
    return m_impl->m_terminalDisplay->bracketedPasteModeIsDisabled();
}

void QTermWidget::copyClipboard()
{
    m_impl->m_terminalDisplay->copyClipboard(QClipboard::Clipboard);
}

void QTermWidget::copySelection()
{
    m_impl->m_terminalDisplay->copyClipboard(QClipboard::Selection);
}

void QTermWidget::pasteClipboard()
{
    m_impl->m_terminalDisplay->pasteClipboard();
}

void QTermWidget::pasteSelection()
{
    m_impl->m_terminalDisplay->pasteSelection();
}

void QTermWidget::selectAll()
{
    m_impl->m_terminalDisplay->selectAll();
}

int QTermWidget::setZoom(int step)
{
    QFont font = m_impl->m_terminalDisplay->getVTFont();

    font.setPointSize(font.pointSize() + step);
    setTerminalFont(font);
    return font.pointSize();
}

int QTermWidget::zoomIn()
{
    return setZoom(STEP_ZOOM);
}

int QTermWidget::zoomOut()
{
    return setZoom(-STEP_ZOOM);
}

void QTermWidget::setKeyBindings(const QString & kb)
{
    m_impl->m_session->setKeyBindings(kb);
}

void QTermWidget::clear()
{
    clearScreen();
    clearScrollback();
}

void QTermWidget::clearScrollback()
{
    m_impl->m_session->clearHistory();
}

void QTermWidget::clearScreen()
{
    m_impl->m_session->emulation()->reset();
    m_impl->m_session->refresh();
}

void QTermWidget::setFlowControlEnabled(bool enabled)
{
    m_impl->m_session->setFlowControlEnabled(enabled);
}

QStringList QTermWidget::availableKeyBindings()
{
    return KeyboardTranslatorManager::instance()->allTranslators();
}

QString QTermWidget::keyBindings()
{
    return m_impl->m_session->keyBindings();
}

void QTermWidget::toggleShowSearchBar()
{
    if(m_searchBar->isHidden()) {
        m_searchBar->setText(selectedText(true));
        m_searchBar->show();
    } else {
        m_searchBar->hide();
    }
}

bool QTermWidget::flowControlEnabled(void)
{
    return m_impl->m_session->flowControlEnabled();
}

void QTermWidget::setFlowControlWarningEnabled(bool enabled)
{
    if (flowControlEnabled()) {
        // Do not show warning label if flow control is disabled
        m_impl->m_terminalDisplay->setFlowControlWarningEnabled(enabled);
    }
}

void QTermWidget::setMotionAfterPasting(int action)
{
    m_impl->m_terminalDisplay->setMotionAfterPasting((Konsole::MotionAfterPasting) action);
}

int QTermWidget::historyLinesCount()
{
    return m_impl->m_terminalDisplay->screenWindow()->screen()->getHistLines();
}

int QTermWidget::screenColumnsCount()
{
    return m_impl->m_terminalDisplay->screenWindow()->screen()->getColumns();
}

int QTermWidget::screenLinesCount()
{
    return m_impl->m_terminalDisplay->screenWindow()->screen()->getLines();
}

void QTermWidget::setSelectionStart(int row, int column)
{
    m_impl->m_terminalDisplay->screenWindow()->screen()->setSelectionStart(column, row, true);
}

void QTermWidget::setSelectionEnd(int row, int column)
{
    m_impl->m_terminalDisplay->screenWindow()->screen()->setSelectionEnd(column, row);
}

void QTermWidget::getSelectionStart(int& row, int& column)
{
    m_impl->m_terminalDisplay->screenWindow()->screen()->getSelectionStart(column, row);
}

void QTermWidget::getSelectionEnd(int& row, int& column)
{
    m_impl->m_terminalDisplay->screenWindow()->screen()->getSelectionEnd(column, row);
}

QString QTermWidget::selectedText(bool preserveLineBreaks)
{
    return m_impl->m_terminalDisplay->screenWindow()->screen()->selectedText(preserveLineBreaks);
}

void QTermWidget::setMonitorActivity(bool enabled)
{
    m_impl->m_session->setMonitorActivity(enabled);
}

void QTermWidget::setMonitorSilence(bool enabled)
{
    m_impl->m_session->setMonitorSilence(enabled);
}

void QTermWidget::setSilenceTimeout(int seconds)
{
    m_impl->m_session->setMonitorSilenceSeconds(seconds);
}

Filter::HotSpot* QTermWidget::getHotSpotAt(const QPoint &pos) const
{
    int row = 0, column = 0;
    m_impl->m_terminalDisplay->getCharacterPosition(pos, row, column);
    return getHotSpotAt(row, column);
}

Filter::HotSpot* QTermWidget::getHotSpotAt(int row, int column) const
{
    return m_impl->m_terminalDisplay->filterChain()->hotSpotAt(row, column);
}

QList<QAction*> QTermWidget::filterActions(const QPoint& position)
{
    return m_impl->m_terminalDisplay->filterActions(position);
}

int QTermWidget::recvData(const char *buff, int len) const
{
    return m_impl->m_session->recvData(buff,len);
}

void QTermWidget::setKeyboardCursorShape(KeyboardCursorShape shape)
{
    m_impl->m_terminalDisplay->setKeyboardCursorShape(shape);
}

void QTermWidget::setKeyboardCursorShape(uint32_t shape)
{
    m_impl->m_terminalDisplay->setKeyboardCursorShape((KeyboardCursorShape)shape);
}

void QTermWidget::setBlinkingCursor(bool blink)
{
    m_impl->m_terminalDisplay->setBlinkingCursor(blink);
}

void QTermWidget::setBidiEnabled(bool enabled)
{
    m_impl->m_terminalDisplay->setBidiEnabled(enabled);
}

bool QTermWidget::isBidiEnabled()
{
    return m_impl->m_terminalDisplay->isBidiEnabled();
}

QString QTermWidget::title() const
{
    QString title = m_impl->m_session->userTitle();
    if (title.isEmpty())
        title = m_impl->m_session->title(Konsole::Session::NameRole);
    return title;
}

QString QTermWidget::icon() const
{
    QString icon = m_impl->m_session->iconText();
    if (icon.isEmpty())
        icon = m_impl->m_session->iconName();
    return icon;
}

bool QTermWidget::isTitleChanged() const
{
    return m_impl->m_session->isTitleChanged();
}

void QTermWidget::cursorChanged(Konsole::Emulation::KeyboardCursorShape cursorShape, bool blinkingCursorEnabled)
{
    // TODO: A switch to enable/disable DECSCUSR?
    setKeyboardCursorShape(cursorShape);
    setBlinkingCursor(blinkingCursorEnabled);
}

void QTermWidget::setMargin(int margin)
{
    m_impl->m_terminalDisplay->setMargin(margin);
}

int QTermWidget::getMargin() const
{
    return m_impl->m_terminalDisplay->margin();
}

void QTermWidget::saveHistory(QTextStream *stream, int format)
{
    TerminalCharacterDecoder *decoder;
    if(format == 0) {
        decoder = new PlainTextDecoder;
    } else {
        decoder = new HTMLDecoder;
    }
    decoder->begin(stream);
    m_impl->m_session->emulation()->writeToStream(decoder, 0, m_impl->m_session->emulation()->lineCount());
    delete decoder;
}

void QTermWidget::saveHistory(QIODevice *device, int format)
{
    QTextStream stream(device);
    saveHistory(&stream, format);
}

void QTermWidget::screenShot(QPixmap *pixmap)
{
    QPixmap currPixmap(m_impl->m_terminalDisplay->size());
    m_impl->m_terminalDisplay->render(&currPixmap);
    *pixmap = currPixmap.scaled(pixmap->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

void QTermWidget::repaintDisplay(void)
{
    m_impl->m_terminalDisplay->repaintDisplay();
}

void QTermWidget::screenShot(const QString &fileName)
{
    qreal deviceratio = m_impl->m_terminalDisplay->devicePixelRatio();
    deviceratio = deviceratio*2;
    QPixmap pixmap(m_impl->m_terminalDisplay->size() * deviceratio);
    pixmap.setDevicePixelRatio(deviceratio);
    m_impl->m_terminalDisplay->render(&pixmap);
    pixmap.save(fileName);
}

void QTermWidget::setLocked(bool enabled)
{
    this->setEnabled(!enabled);
    m_impl->m_terminalDisplay->setLocked(enabled);
}

void QTermWidget::setDrawLineChars(bool drawLineChars)
{
    m_impl->m_terminalDisplay->setDrawLineChars(drawLineChars);
}

void QTermWidget::setBoldIntense(bool boldIntense)
{
    m_impl->m_terminalDisplay->setBoldIntense(boldIntense);
}

void QTermWidget::setConfirmMultilinePaste(bool confirmMultilinePaste) {
    m_impl->m_terminalDisplay->setConfirmMultilinePaste(confirmMultilinePaste);
}

void QTermWidget::setTrimPastedTrailingNewlines(bool trimPastedTrailingNewlines) {
    m_impl->m_terminalDisplay->setTrimPastedTrailingNewlines(trimPastedTrailingNewlines);
}

void QTermWidget::setEcho(bool echo) {
    m_echo = echo;
}

void QTermWidget::setKeyboardCursorColor(bool useForegroundColor, const QColor& color) {
    m_impl->m_terminalDisplay->setKeyboardCursorColor(useForegroundColor, color);
}

void QTermWidget::addHighLightText(const QString &text, const QColor &color)
{
    for (int i = 0; i < m_highLightTexts.size(); i++) {
        if (m_highLightTexts.at(i)->text == text) {
            return;
        }
    }
    HighLightText *highLightText = new HighLightText(text,color);
    m_highLightTexts.append(highLightText);
    m_impl->m_terminalDisplay->filterChain()->addFilter(highLightText->regExpFilter);
    m_impl->m_terminalDisplay->updateFilters();
    m_impl->m_terminalDisplay->repaint();
}

bool QTermWidget::isContainHighLightText(const QString &text)
{
    for (int i = 0; i < m_highLightTexts.size(); i++) {
        if (m_highLightTexts.at(i)->text == text) {
            return true;
        }
    }
    return false;
}

void QTermWidget::removeHighLightText(const QString &text)
{
    for (int i = 0; i < m_highLightTexts.size(); i++) {
        if (m_highLightTexts.at(i)->text == text) {
            m_impl->m_terminalDisplay->filterChain()->removeFilter(m_highLightTexts.at(i)->regExpFilter);
            delete m_highLightTexts.at(i);
            m_highLightTexts.removeAt(i);
            m_impl->m_terminalDisplay->updateFilters();
            break;
        }
    }
    m_impl->m_terminalDisplay->repaint();
}

void QTermWidget::clearHighLightTexts(void)
{
    for (int i = 0; i < m_highLightTexts.size(); i++) {
        m_impl->m_terminalDisplay->filterChain()->removeFilter(m_highLightTexts.at(i)->regExpFilter);
        delete m_highLightTexts.at(i);
    }
    m_impl->m_terminalDisplay->updateFilters();
    m_highLightTexts.clear();
    m_impl->m_terminalDisplay->repaint();
}

void QTermWidget::setWordCharacters(const QString &wordCharacters)
{
    m_impl->m_terminalDisplay->setWordCharacters(wordCharacters);
}

QString QTermWidget::wordCharacters(void) {
    return m_impl->m_terminalDisplay->wordCharacters();
}

void QTermWidget::setShowResizeNotificationEnabled(bool enabled) {
    m_impl->m_terminalDisplay->setShowResizeNotificationEnabled(enabled);
}

void QTermWidget::setEnableHandleCtrlC(bool enable) {
    m_impl->m_session->emulation()->setEnableHandleCtrlC(enable);
}

int QTermWidget::lines() {
    return m_impl->m_terminalDisplay->lines();
}

int QTermWidget::columns() {
    return m_impl->m_terminalDisplay->columns();
}

int QTermWidget::getCursorX() {
    return m_impl->m_terminalDisplay->getCursorX();
}

int QTermWidget::getCursorY() {
    return m_impl->m_terminalDisplay->getCursorY();
}

void QTermWidget::setCursorX(int x) {
    m_impl->m_terminalDisplay->setCursorX(x);
}

void QTermWidget::setCursorY(int y) {
    m_impl->m_terminalDisplay->setCursorY(y);
}

QString QTermWidget::screenGet(int row1, int col1, int row2, int col2, int mode) {
    return m_impl->m_terminalDisplay->screenGet(row1, col1, row2, col2, mode);
}

void QTermWidget::setSelectionOpacity(qreal opacity) {
    m_impl->m_terminalDisplay->setSelectionOpacity(opacity);
}

void QTermWidget::setUrlFilterEnabled(bool enable) {
    if(m_UrlFilterEnable == enable) {
        return;
    }
    if(enable) {
        m_impl->m_terminalDisplay->filterChain()->addFilter(urlFilter);
    } else {
        m_impl->m_terminalDisplay->filterChain()->removeFilter(urlFilter);
    }
}

void QTermWidget::setMessageParentWidget(QWidget *parent) {
    messageParentWidget = parent;
    m_impl->m_terminalDisplay->setMessageParentWidget(messageParentWidget);
}

void QTermWidget::reTranslateUi(void) {
    m_searchBar->retranslateUi();
}
