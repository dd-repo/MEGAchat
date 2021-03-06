#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QInputDialog>
#include <QMessageBox>
#include <QContextMenuEvent>
#include <QMenu>
#include <QTMegaChatEvent.h>
#include "uiSettings.h"
#include "chatSettings.h"

using namespace mega;
using namespace megachat;

MainWindow::MainWindow(QWidget *parent, MegaLoggerApplication *logger) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    nContacts = 0;
    activeChats = 0;
    inactiveChats = 0;
    ui->setupUi(this);
    ui->contactList->setSelectionMode(QAbstractItemView::NoSelection);
    mMegaChatApi = NULL;
    mMegaApi = NULL;
    megaChatListenerDelegate = NULL;
    onlineStatus = NULL;
    allItemsVisibility = false;
    mLogger = logger;
    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    if (megaChatListenerDelegate)
    {
        delete megaChatListenerDelegate;
    }
    chatWidgets.clear();
    contactWidgets.clear();
    delete ui;
}

mega::MegaUserList * MainWindow::getUserContactList()
{
    return mMegaApi->getContacts();
}

void MainWindow::setMegaChatApi(megachat::MegaChatApi *megaChatApi)
{
    this->mMegaChatApi = megaChatApi;
}

void MainWindow::setMegaApi(MegaApi *megaApi)
{
    this->mMegaApi = megaApi;
}

void MainWindow::clearContactChatList()
{
    ui->contactList->clear();
    chatWidgets.clear();
    contactWidgets.clear();
}

void MainWindow::orderContactChatList(bool showInactive)
{
    auxChatWidgets = chatWidgets;
    clearContactChatList();
    addContacts();
    QString text;
    if(showInactive)
    {
        addInactiveChats();
        text.append(" Showing <all> elements");
    }
    else
    {
        text.append(" Showing <visible> elements");
    }
    addActiveChats();
    auxChatWidgets.clear();
    this->ui->mOnlineStatusDisplay->setText(text);
}


void MainWindow::addContacts()
{
    MegaUser *contact = NULL;
    MegaUserList *contactList = mMegaApi->getContacts();
    setNContacts(contactList->size());

    for (int i = 0; i < contactList->size(); i++)
    {
        contact = contactList->get(i);
        mega::MegaHandle userHandle = contact->getHandle();
        if (userHandle != this->mMegaChatApi->getMyUserHandle())
        {
            if (contact->getVisibility() == MegaUser::VISIBILITY_HIDDEN && allItemsVisibility != true)
            {
                continue;
            }
            addContact(contact);
        }
    }
    delete contactList;
}

void MainWindow::addInactiveChats()
{
    MegaChatListItemList *chatList = this->mMegaChatApi->getInactiveChatListItems();
    for (int i = 0; i < chatList->size(); i++)
    {
        addChat(chatList->get(i));
    }
    delete chatList;
}

void MainWindow::addActiveChats()
{
    std::list<Chat> listofChats;
    MegaChatListItemList *chatList = this->mMegaChatApi->getActiveChatListItems();
    for (int i = 0; i < chatList->size(); i++)
    {
        listofChats.push_back(Chat(chatList->get(i)->getChatId(), chatList->get(i)->getLastTimestamp()));
    }

    listofChats.sort();
    for(Chat &chat : listofChats)
    {
       addChat(mMegaChatApi->getChatListItem(chat.chatId));
    }
    delete chatList;
}

void MainWindow::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    menu.setAttribute(Qt::WA_DeleteOnClose);
    auto addAction = menu.addAction(tr("Add user to contacts"));
    connect(addAction, SIGNAL(triggered()), this, SLOT(onAddContact()));

    auto actVisibility = menu.addAction(tr("Show/Hide invisible elements"));
    connect(actVisibility, SIGNAL(triggered()), this, SLOT(onChangeItemsVisibility()));

    menu.exec(event->globalPos());
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (this->mMegaChatApi->isSignalActivityRequired() && event->type() == QEvent::MouseButtonRelease)
    {
        this->mMegaChatApi->signalPresenceActivity();
    }
    return false;
}

void MainWindow::on_bSettings_clicked()
{
    ChatSettings *chatSettings = new ChatSettings(this);
    chatSettings->exec();
    chatSettings->deleteLater();
}

void MainWindow::on_bOnlineStatus_clicked()
{
    onlineStatus = new QMenu(this);
    auto actOnline = onlineStatus->addAction("Online");
    actOnline->setData(QVariant(MegaChatApi::STATUS_ONLINE));
    connect(actOnline, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actAway = onlineStatus->addAction("Away");
    actAway->setData(QVariant(MegaChatApi::STATUS_AWAY));
    connect(actAway, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actDnd = onlineStatus->addAction("Busy");
    actDnd->setData(QVariant(MegaChatApi::STATUS_BUSY));
    connect(actDnd, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto actOffline = onlineStatus->addAction("Offline");
    actOffline->setData(QVariant(MegaChatApi::STATUS_OFFLINE));
    connect(actOffline, SIGNAL(triggered()), this, SLOT(setOnlineStatus()));

    auto rect = ui->bOnlineStatus->rect();
    onlineStatus->move(mapToGlobal(QPoint(1,rect.bottom())));
    onlineStatus->resize(rect.width(), 100);
    onlineStatus->setStyleSheet("QMenu {"
        "background-color: qlineargradient("
        "spread:pad, x1:0, y1:0, x2:0, y2:1,"
            "stop:0 rgba(120,120,120,200),"
            "stop:1 rgba(180,180,180,200));"
        "}"
        "QMenu::item:!selected{"
            "color: white;"
        "}"
        "QMenu::item:selected{"
            "background-color: qlineargradient("
            "spread:pad, x1:0, y1:0, x2:0, y2:1,"
            "stop:0 rgba(120,120,120,200),"
            "stop:1 rgba(180,180,180,200));"
        "}");
    onlineStatus->exec();
    onlineStatus->deleteLater();
}

ChatItemWidget *MainWindow::getChatItemWidget(megachat::MegaChatHandle chatHandle, bool reorder)
{
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itChats;
    if (!reorder)
    {
        itChats = chatWidgets.find(chatHandle);
        if (itChats == chatWidgets.end())
        {
            return NULL;
        }
    }
    else
    {
        itChats = auxChatWidgets.find(chatHandle);
        if (itChats == auxChatWidgets.end())
        {
            return NULL;
        }
    }
    return itChats->second;
}


void MainWindow::addContact(MegaUser *contact)
{
    ContactItemWidget *contactItemWidget = new ContactItemWidget(ui->contactList, mMegaChatApi, mMegaApi, contact);
    contactItemWidget->updateToolTip(contact);
    QListWidgetItem *item = new QListWidgetItem();
    contactItemWidget->setWidgetItem(item);
    item->setSizeHint(QSize(item->sizeHint().height(), 28));
    ui->contactList->insertItem(nContacts, item);
    ui->contactList->setItemWidget(item, contactItemWidget);
    contactWidgets.insert(std::pair<mega::MegaHandle, ContactItemWidget *>(contact->getHandle(),contactItemWidget));
    nContacts +=1;
}


void MainWindow::addChat(const MegaChatListItem* chatListItem)
{
    int index = 0;
    if(!chatListItem->isActive())
    {
        index = -(nContacts);
        activeChats +=1;
    }
    else
    {
        index = -(activeChats+inactiveChats+nContacts);
        inactiveChats +=1;
    }

    megachat::MegaChatHandle chathandle = chatListItem->getChatId();
    ChatItemWidget *chatItemWidget = new ChatItemWidget(this, mMegaChatApi, chatListItem);
    chatItemWidget->updateToolTip(chatListItem, NULL);
    QListWidgetItem *item = new QListWidgetItem();
    chatItemWidget->setWidgetItem(item);
    item->setSizeHint(QSize(item->sizeHint().height(), 28));
    chatWidgets.insert(std::pair<megachat::MegaChatHandle, ChatItemWidget *>(chathandle,chatItemWidget));
    ui->contactList->insertItem(index, item);
    ui->contactList->setItemWidget(item, chatItemWidget);

    ChatItemWidget *auxChatItemWidget = getChatItemWidget(chathandle, true);
    if(auxChatItemWidget)
    {
        chatItemWidget->mChatWindow = auxChatItemWidget->mChatWindow;
        auxChatItemWidget->deleteLater();
    }
}

void MainWindow::onChatListItemUpdate(MegaChatApi* api, MegaChatListItem *item)
{
    int change = item->getChanges();
    megachat::MegaChatHandle chatHandle = item->getChatId();
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator itChats;
    itChats = chatWidgets.find(chatHandle);

    if (itChats != chatWidgets.end())
    {
        ChatItemWidget * chatItemWidget = itChats->second;
        switch (change)
        {
            //Last Message update
            case (megachat::MegaChatListItem::CHANGE_TYPE_LAST_MSG):
                {
                    chatItemWidget->updateToolTip(item, NULL);
                    break;
                }
            //Unread count update
            case (megachat::MegaChatListItem::CHANGE_TYPE_UNREAD_COUNT):
                {
                    chatItemWidget->onUnreadCountChanged(item->getUnreadCount());
                    break;
                }
            //Title update
            case (megachat::MegaChatListItem::CHANGE_TYPE_TITLE):
                {
                    chatItemWidget->onTitleChanged(item->getTitle());
                    break;
                }
            //Own priv update
            case (megachat::MegaChatListItem::CHANGE_TYPE_OWN_PRIV):
                {
                    chatItemWidget->updateToolTip(item, NULL);
                    break;
                }
            //Participants update
            case (megachat::MegaChatListItem::CHANGE_TYPE_PARTICIPANTS):
                {
                    chatItemWidget->updateToolTip(item, NULL);
                    break;
                }
            //The chatroom has been left by own user
            case (megachat::MegaChatListItem::CHANGE_TYPE_CLOSED):
                {
                    chatItemWidget->showAsHidden();
                    break;
                }
            //Timestamp of the last activity update
            case (megachat::MegaChatListItem::CHANGE_TYPE_LAST_TS):
                {
                    orderContactChatList(allItemsVisibility);
                }
        }
     }
}

void MainWindow::onChangeItemsVisibility()
{
    allItemsVisibility = !allItemsVisibility;
    orderContactChatList(allItemsVisibility);
}

void MainWindow::onAddContact()
{
    QString email = QInputDialog::getText(this, tr("Add contact"), tr("Please enter the email of the user to add"));
    if (email.isNull())
        return;

    char *myEmail = mMegaApi->getMyEmail();
    QString qMyEmail = myEmail;
    delete [] myEmail;

    if (email == qMyEmail)
    {
        QMessageBox::critical(this, tr("Add contact"), tr("You can't add your own email as contact"));
        return;
    }
    std::string emailStd = email.toStdString();
    mMegaApi->inviteContact(emailStd.c_str(),tr("I'd like to add you to my contact list").toUtf8().data(), MegaContactRequest::INVITE_ACTION_ADD);
}

void MainWindow::setOnlineStatus()
{
    auto action = qobject_cast<QAction*>(QObject::sender());
    assert(action);
    bool ok;
    auto pres = action->data().toUInt(&ok);
    if (!ok || (pres == MegaChatApi::STATUS_INVALID))
    {
        return;
    }
    this->mMegaChatApi->setOnlineStatus(pres);
}

void MainWindow::addChatListener()
{
    megaChatListenerDelegate = new QTMegaChatListener(mMegaChatApi, this);
    mMegaChatApi->addChatListener(megaChatListenerDelegate);
}

void MainWindow::onChatConnectionStateUpdate(MegaChatApi *api, MegaChatHandle chatid, int newState)
{
    if (chatid == megachat::MEGACHAT_INVALID_HANDLE)
    {
        orderContactChatList(allItemsVisibility);
        megachat::MegaChatPresenceConfig *presenceConfig = mMegaChatApi->getPresenceConfig();
        if (presenceConfig)
        {
            onChatPresenceConfigUpdate(mMegaChatApi, presenceConfig);
        }
        delete presenceConfig;
        return;
    }
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator it;
    it = chatWidgets.find(chatid);

    if (it != chatWidgets.end())
    {
        ChatItemWidget * chatItemWidget = it->second;
        chatItemWidget->onlineIndicatorUpdate(newState);
    }
}

void MainWindow::onChatInitStateUpdate(megachat::MegaChatApi* api, int newState)
{
    if (!this->isVisible() && (newState == MegaChatApi::INIT_OFFLINE_SESSION
       || newState == MegaChatApi::INIT_ONLINE_SESSION))
    {
       this->show();
    }
    else if (newState == MegaChatApi::INIT_ERROR)
    {
       this->hide();
       Q_EMIT esidLogout();
    }

    if (newState == MegaChatApi::INIT_OFFLINE_SESSION ||
        newState == MegaChatApi::INIT_ONLINE_SESSION)
    {
        setWindowTitle(api->getMyEmail());
    }
    else
    {
        setWindowTitle("");
    }
}

void MainWindow::onChatOnlineStatusUpdate(MegaChatApi* api, MegaChatHandle userhandle, int status, bool inProgress)
{
    if (status == megachat::MegaChatApi::STATUS_INVALID)
        status = 0;

    if (this->mMegaChatApi->getMyUserHandle() == userhandle && !inProgress)
    {
        ui->bOnlineStatus->setText(kOnlineSymbol_Set);
        if (status >= 0 && status < NINDCOLORS)
            ui->bOnlineStatus->setStyleSheet(kOnlineStatusBtnStyle.arg(gOnlineIndColors[status]));
    }
    else
    {
        std::map<mega::MegaHandle, ContactItemWidget *>::iterator itContacts;
        itContacts = this->contactWidgets.find((mega::MegaHandle) userhandle);
        if (itContacts != contactWidgets.end())
        {
            ContactItemWidget * contactItemWidget = itContacts->second;
            assert(!inProgress);
            contactItemWidget->updateOnlineIndicator(status);
        }
    }
}

void MainWindow::onChatPresenceConfigUpdate(MegaChatApi* api, MegaChatPresenceConfig *config)
{
    int status = config->getOnlineStatus();
    if (status == megachat::MegaChatApi::STATUS_INVALID)
        status = 0;

    ui->bOnlineStatus->setText(config->isPending()
        ? kOnlineSymbol_InProgress
        : kOnlineSymbol_Set);

    ui->bOnlineStatus->setStyleSheet(
        kOnlineStatusBtnStyle.arg(gOnlineIndColors[status]));
}

int MainWindow::getNContacts() const
{
    return nContacts;
}

void MainWindow::setNContacts(int nContacts)
{
    this->nContacts = nContacts;
}


void MainWindow::updateMessageFirstname(MegaChatHandle contactHandle, const char *firstname)
{
    std::map<megachat::MegaChatHandle, ChatItemWidget *>::iterator it;
    for (it = chatWidgets.begin(); it != chatWidgets.end(); it++)
    {
        MegaChatListItem *chatListItem  = mMegaChatApi->getChatListItem(it->first);
        ChatItemWidget *chatItemWidget = it->second;

        if (chatListItem->getLastMessageSender() == contactHandle)
        {
            chatItemWidget->updateToolTip(mMegaChatApi->getChatListItem(it->first), firstname);
        }

        ChatWindow *chatWindow = chatItemWidget->getChatWindow();
        if (chatWindow)
        {
            chatWindow->updateMessageFirstname(contactHandle, firstname);
        }
        delete chatListItem;
    }
}

void MainWindow::updateContactFirstname(MegaChatHandle contactHandle, const char * firstname)
{
    std::map<mega::MegaHandle, ContactItemWidget *>::iterator itContacts;
    itContacts = contactWidgets.find(contactHandle);

    if (itContacts != contactWidgets.end())
    {                
        ContactItemWidget *contactItemWidget = itContacts->second;
        contactItemWidget->updateTitle(firstname);
    }
}
