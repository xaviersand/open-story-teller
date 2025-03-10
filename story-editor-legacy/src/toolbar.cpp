#include "toolbar.h"
#include "qmenubar.h"
#include "src/dock_widget_base.h"

#include <QMessageBox>

ToolBar::ToolBar()
{
    setObjectName("MainToolBar");

    m_aboutDialog = new QDialog();
    m_aboutUi.setupUi(m_aboutDialog);

    m_aboutUi.versionLabel->setText(OST_EDITOR_VERSION);
//    setIconSize(QSize(10, 10));
//    setFixedHeight(36);
}

void ToolBar::createActions(QMenuBar *menuBar)
{
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));

    // ------------ New
    {
        QIcon icon(":/assets/file-document-plus-outline.svg");
        QAction *act = new QAction(icon, tr("&New project"), this);
        act->setShortcuts(QKeySequence::New);
        act->setStatusTip(tr("Create a new project"));
        connect(act, &QAction::triggered, this, &ToolBar::sigNew);
        fileMenu->addAction(act);
        addAction(act);
    }
    // ------------ Save
    {
        QIcon icon(":/assets/floppy.svg");
        m_saveProjectAction = new QAction(icon, tr("&Save project"), this);
        m_saveProjectAction->setShortcuts(QKeySequence::Save);
        m_saveProjectAction->setStatusTip(tr("Save the current project"));
        connect(m_saveProjectAction, &QAction::triggered, this, &ToolBar::sigSave);
        fileMenu->addAction(m_saveProjectAction);
        addAction(m_saveProjectAction);
    }
    // ------------ Open
    {
        QIcon icon(":/assets/folder-open-outline.svg");
        QAction *act = new QAction(icon, tr("&Open project"), this);
        act->setShortcuts(QKeySequence::Open);
        act->setStatusTip(tr("Open an existing project"));
        connect(act, &QAction::triggered, this, &ToolBar::sigOpen);
        fileMenu->addAction(act);
        addAction(act);
    }
    // ------------ Close
    {
        QIcon icon(":/assets/close-outline.svg");
        m_closeProjectAction = new QAction(icon, tr("&Close project"), this);
        m_closeProjectAction->setShortcuts(QKeySequence::Close);
        m_closeProjectAction->setStatusTip(tr("Close current project"));
        connect(m_closeProjectAction, &QAction::triggered, this, &ToolBar::sigClose);
        fileMenu->addAction(m_closeProjectAction);
        addAction(m_closeProjectAction);
    }

    m_recentProjectsMenu = new QMenu(tr("Recent projects"));
    fileMenu->addMenu(m_recentProjectsMenu);

    fileMenu->addSeparator();

    QAction *quitAct = fileMenu->addAction(tr("&Quit"), this, &ToolBar::sigExit);
    quitAct->setShortcuts(QKeySequence::Quit);
    quitAct->setStatusTip(tr("Quit the application"));

    menuBar->addSeparator();

    // -------------------------------- DEBUG STORY

    QMenu *storyMenu = menuBar->addMenu(tr("&Story"));
    // ------------ Run
    {
        QIcon icon(":/assets/play-circle-green.png");
        m_runAction = new QAction(icon, tr("&Close project"), this);
        m_runAction->setStatusTip(tr("Build and run current project"));
        connect(m_runAction, &QAction::triggered, this, &ToolBar::sigRun);
        storyMenu->addAction(m_runAction);
        addSeparator();
        addAction(m_runAction);
    }

    // -------------------------------- WINDOWS MENU
    m_windowsMenu = menuBar->addMenu(tr("&Windows"));

    auto act = m_windowsMenu->addAction(tr("Reset docks position"));
    connect(act, &QAction::triggered, this, &ToolBar::sigDefaultDocksPosition);

    m_windowsMenu->addSeparator();

    QMenu *helpMenu = menuBar->addMenu(tr("&Help"));

    QAction *aboutAct = helpMenu->addAction(tr("&About"), this, &ToolBar::slotAbout);
    aboutAct->setStatusTip(tr("Show the application's About box"));
}

void ToolBar::GenerateRecentProjectsMenu(const QStringList &recents)
{
    m_recentProjectsMenu->clear();

    for (auto &r: recents)
    {
        auto act = m_recentProjectsMenu->addAction(r);
        connect(act, &QAction::triggered, this, [&, r]() {
            emit sigOpenRecent(r);
        });
    }
}

QVariant ToolBar::GetDocksPreferences() const
{
    QMap<QString, QVariant> prefs;

    for (auto d : m_docksList)
    {
        prefs[d->objectName()] = d->GetPreferedVisibility();
    }

    return prefs;
}

QVariant ToolBar::SetDocksPreferences(const QVariant &prefs)
{
    QMap<QString, QVariant> p = prefs.toMap();

    for (auto d : m_docksList)
    {
        if (p.contains(d->objectName()))
        {
            d->SetPreferedVisibility(p[d->objectName()].toBool());
        }
    }

    return prefs;
}

void ToolBar::SetActionsActive(bool enable)
{
    for (auto d : m_actionDockList)
    {
        d->setEnabled(enable);
    }
    m_windowsMenu->setEnabled(enable);
    m_closeProjectAction->setEnabled(enable);
    m_saveProjectAction->setEnabled(enable);
    m_runAction->setEnabled(enable);
}


void ToolBar::slotAbout()
{
    m_aboutDialog->exec();
}

void ToolBar::AddDockToMenu(QAction *action, DockWidgetBase *dock)
{
    m_windowsMenu->addAction(action);
    m_actionDockList.push_back(action);
    m_docksList.push_back(dock);
    connect(action, &QAction::triggered, this, [dock](bool checked) {
        dock->SetPreferedVisibility(checked);
    });
}
