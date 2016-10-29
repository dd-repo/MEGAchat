#include "sdk_test.h"

#include "../../third-party/sdk/include/megaapi.h"  // TODO: include it as a system lib, but keeping symbols resolved by IDE
#include "../../src/megachatapi.h"
#include "../../src/karereCommon.h" // for logging with karere facility

#include <signal.h>

void sigintHandler(int)
{
    printf("SIGINT Received\n");
    fflush(stdout);
}

int main(int argc, char **argv)
{
//    ::mega::MegaClient::APIURL = "https://staging.api.mega.co.nz/";

    MegaChatApiTest t;
    t.init();

    t.TEST_resumeSession();
    t.TEST_setOnlineStatus();
    t.TEST_getChatRoomsAndMessages();
    t.TEST_editAndDeleteMessages();
    t.TEST_groupChatManagement();

    // Create a group chat
    /*  - Establish contact relationship: invite, accept
     *  - Create group chat
     *  - Invite the contact
     *  - Update permissions of peer
     *  - Remove the peer from chat
     *  - Set title
     *  - Send a message
     *  - Edit a message
     *  - Delete a message
     *  - Send 5 messages
     *  - Truncate history from 3rd message
     *  - Attach node
     *  - Attach contact
     */



//    // 8. Send a message and wait for confirmation from server
//    string msg = "HOLA - This is a testing message automatically sent";
//    flag = &chatroomListener->msgConfirmed; *flag = false;
//    chatroomListener->msgId = megachat::INVALID_HANDLE;   // will be set at confirmation
//    t.megaChatApi[0]->sendMessage(chatid, msg.c_str(), MegaChatMessage::TYPE_NORMAL);
//    assert(t.waitForResponse(flag));    // for confirmation, sendMessage() is synchronous

//    sleep(60);

//    // 9. Edit the sent message
//    MegaChatHandle msgId = chatroomListener->msgId;
//    msg = "Edited message: this is a test";
//    flag = &chatroomListener->msgConfirmed; *flag = false;
//    chatroomListener->msgId = megachat::INVALID_HANDLE;   // will be set at confirmation
//    t.megaChatApi[0]->editMessage(chatid, msgId, msg.c_str());
//    assert(t.waitForResponse(flag));

//    // 9.1. Delete the message
//    flag = &chatroomListener->msgConfirmed; *flag = false;
//    chatroomListener->msgId = megachat::INVALID_HANDLE;   // will be set at confirmation
//    t.megaChatApi[0]->deleteMessage(chatid, msgId);
//    assert(t.waitForResponse(flag));




    // TODO: log in the other account, check the message with msgId has arrived


    t.terminate();

    return 0;
}

MegaChatApiTest::MegaChatApiTest()
{
    logger = new MegaLoggerSDK("SDK.log");
    MegaApi::setLoggerObject(logger);

    chatLogger = new MegaChatLoggerSDK("SDKchat.log");
    MegaChatApi::setLoggerObject(chatLogger);

    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        // get credentials from environment variables
        std::string varName = "MEGA_EMAIL";
        varName += std::to_string(i);
        char *buf = getenv(varName.c_str());
        if (buf)
        {
            email[i].assign(buf);
        }
        if (!email[i].length())
        {
            cout << "TEST - Set your username at the environment variable $" << varName << endl;
            exit(-1);
        }

        varName.assign("MEGA_PWD");
        varName += std::to_string(i);
        buf = getenv(varName.c_str());
        if (buf)
        {
            pwd[i].assign(buf);
        }
        if (!pwd[i].length())
        {
            cout << "TEST - Set your password at the environment variable $" << varName << endl;
            exit(-1);
        }
    }
}

void MegaChatApiTest::init()
{
    // do some initialization
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        char path[1024];
        getcwd(path, sizeof path);
        megaApi[i] = new MegaApi(APP_KEY.c_str(), path, USER_AGENT.c_str());
        megaApi[i]->setLogLevel(MegaApi::LOG_LEVEL_DEBUG);
        megaApi[i]->addRequestListener(this);
        megaApi[i]->log(MegaApi::LOG_LEVEL_INFO, "___ Initializing tests for chat ___");

        megaChatApi[i] = new MegaChatApi(megaApi[i]);
        megaChatApi[i]->setLogLevel(MegaChatApi::LOG_LEVEL_DEBUG);
        megaChatApi[i]->addChatRequestListener(this);
        megaChatApi[i]->addChatListener(this);
        signal(SIGINT, sigintHandler);
        megaApi[i]->log(MegaChatApi::LOG_LEVEL_INFO, "___ Initializing tests for chat SDK___");
    }
}

char *MegaChatApiTest::login(int accountIndex, const char *session)
{
    // 1. login
    bool *flag = &requestFlags[accountIndex][MegaRequest::TYPE_LOGIN]; *flag = false;
    session ? megaApi[accountIndex]->fastLogin(session) : megaApi[accountIndex]->login(email[accountIndex].c_str(), pwd[accountIndex].c_str());
    assert(waitForResponse(flag));

    // 2. fetchnodes
    flag = &requestFlags[accountIndex][MegaRequest::TYPE_FETCH_NODES]; *flag = false;
    megaApi[accountIndex]->fetchNodes();
    assert(waitForResponse(flag));

    // 3. Initialize chat engine
    flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_INITIALIZE]; *flag = false;
    megaChatApi[accountIndex]->init();
    assert(waitForResponse(flag));

    // 4. Connect to chat servers
    flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_CONNECT]; *flag = false;
    megaChatApi[accountIndex]->connect();
    assert(waitForResponse(flag));

    return megaApi[accountIndex]->dumpSession();
}

void MegaChatApiTest::logout(int accountIndex, bool closeSession)
{
    bool *flag = &requestFlags[accountIndex][MegaRequest::TYPE_LOGOUT]; *flag = false;
    closeSession ? megaApi[accountIndex]->logout() : megaApi[accountIndex]->localLogout();
    assert(waitForResponse(flag));

    flag = &requestFlagsChat[accountIndex][MegaChatRequest::TYPE_LOGOUT]; *flag = false;
    closeSession ? megaChatApi[accountIndex]->logout() : megaChatApi[accountIndex]->localLogout();
    assert(waitForResponse(flag));
}

void MegaChatApiTest::terminate()
{
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        megaApi[i]->removeRequestListener(this);
        megaChatApi[i]->removeChatRequestListener(this);
        megaChatApi[i]->removeChatListener(this);

        delete megaChatApi[i];
        delete megaApi[i];

        megaApi[i] = NULL;
        megaChatApi[i] = NULL;
    }
}

void MegaChatApiTest::printChatRoomInfo(const MegaChatRoom *chat)
{
    if (!chat)
    {
        return;
    }

    char hstr[sizeof(handle) * 4 / 3 + 4];
    MegaChatHandle chatid = chat->getChatId();
    Base64::btoa((const byte *)&chatid, sizeof(handle), hstr);

    cout << "Chat ID: " << hstr << " (" << chatid << ")" << endl;
    cout << "\tOwn privilege level: " << MegaChatRoom::privToString(chat->getOwnPrivilege()) << endl;
    if (chat->isGroup())
    {
        cout << "\tGroup chat: yes" << endl;
    }
    else
    {
        cout << "\tGroup chat: no" << endl;
    }
    cout << "\tPeers:";

    if (chat->getPeerCount())
    {
        cout << "\t\t(userhandle)\t(privilege)\t(name)" << endl;
        for (unsigned i = 0; i < chat->getPeerCount(); i++)
        {
            MegaChatHandle uh = chat->getPeerHandle(i);
            Base64::btoa((const byte *)&uh, sizeof(handle), hstr);
            cout << "\t\t\t" << hstr;
            cout << "\t" << MegaChatRoom::privToString(chat->getPeerPrivilege(i));
            cout << "\t\t" << chat->getPeerName(i) << endl;
        }
    }
    else
    {
        cout << " no peers (only you as participant)" << endl;
    }
    if (chat->getTitle())
    {
        cout << "\tTitle: " << chat->getTitle() << endl;
    }
    cout << "\tOnline state: " << MegaChatRoom::statusToString(chat->getOnlineStatus()) << endl;
    cout << "\tUnread count: " << chat->getUnreadCount() << " message/s" << endl;
    cout << "-------------------------------------------------" << endl;
    fflush(stdout);
}

void MegaChatApiTest::printMessageInfo(const MegaChatMessage *msg)
{
    const char *content = msg->getContent() ? msg->getContent() : "<empty>";

    cout << "id: " << msg->getMsgId() << ", content: " << content;
    cout << ", tempId: " << msg->getTempId() << ", index:" << msg->getMsgIndex();
    cout << ", status: " << msg->getStatus() << ", uh: " << msg->getUserHandle();
    cout << ", type: " << msg->getType() << ", edited: " << msg->isEdited();
    cout << ", deleted: " << msg->isDeleted() << ", changes: " << msg->getChanges();
    cout << ", ts: " << msg->getTimestamp() << endl;
    fflush(stdout);
}

void MegaChatApiTest::printChatListItemInfo(const MegaChatListItem *item)
{
    const char *title = item->getTitle() ? item->getTitle() : "<empty>";

    cout << "id: " << item->getChatId() << ", title: " << title;
    cout << ", status: " << item->getOnlineStatus() << ", visibility: " << item->getVisibility();
    cout << ", unread: " << item->getUnreadCount() << endl;
    fflush(stdout);
}

bool MegaChatApiTest::waitForResponse(bool *responseReceived, int timeout)
{
    timeout *= 1000000; // convert to micro-seconds
    int tWaited = 0;    // microseconds
    while(!(*responseReceived))
    {
        usleep(pollingT);

        if (timeout)
        {
            tWaited += pollingT;
            if (tWaited >= timeout)
            {
                return false;   // timeout is expired
            }
        }
    }

    return true;    // response is received
}

void MegaChatApiTest::TEST_resumeSession()
{
    // Create a new session
    char *session = login(0);

    // Resume an existing session
    logout(0, false); // keeps session alive
    char *tmpSession = login(0, session);
    assert (!strcmp(session, tmpSession));
    delete [] tmpSession;   tmpSession = NULL;

    // Close session
    logout(0, true);
    delete [] session; session = NULL;
}

void MegaChatApiTest::TEST_setOnlineStatus()
{
    login(0);

    bool *flag = &requestFlagsChat[0][MegaChatRequest::TYPE_SET_ONLINE_STATUS]; *flag = false;
    megaChatApi[0]->setOnlineStatus(MegaChatApi::STATUS_BUSY);
    assert(waitForResponse(flag));

    logout(0, true);
}

void MegaChatApiTest::TEST_getChatRoomsAndMessages()
{
    login(0);

    MegaChatRoomList *chats = megaChatApi[0]->getChatRooms();
    cout << chats->size() << " chat/s received: " << endl;

    // Open chats and print history
    for (int i = 0; i < chats->size(); i++)
    {
        // Open a chatroom
        const MegaChatRoom *chatroom = chats->get(i);
        MegaChatHandle chatid = chatroom->getChatId();
        TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
        assert(megaChatApi[0]->openChatRoom(chatid, chatroomListener));

        // Print chats
        printChatRoomInfo(chatroom);

        // Load history
        cout << "Loading messages for chat " << chatroom->getTitle() << " (id: " << chatroom->getChatId() << ")" << endl;
        while (1)
        {
            bool *flag = &chatroomListener->historyLoaded[0]; *flag = false;
            if (!megaChatApi[0]->loadMessages(chatid, 16))
            {
                break;  // no more history
            }
            assert(waitForResponse(flag));
        }

        // Close the chatroom
        megaChatApi[0]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;

        // Now, load history locally (it should be cached by now)
        chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
        assert(megaChatApi[0]->openChatRoom(chatid, chatroomListener));
        cout << "Loading messages locally for chat " << chatroom->getTitle() << " (id: " << chatroom->getChatId() << ")" << endl;
        while (1)
        {
            bool *flag = &chatroomListener->historyLoaded[0]; *flag = false;
            if (!megaChatApi[0]->loadMessages(chatid, 16))
            {
                break;  // no more history
            }
            assert(waitForResponse(flag));
        }
        megaChatApi[0]->closeChatRoom(chatid, chatroomListener);
        delete chatroomListener;
    }

    logout(0, true);
}

void MegaChatApiTest::TEST_editAndDeleteMessages()
{
    login(0);
    login(1);

    MegaUser *peer0 = megaApi[0]->getContact(email[1].c_str());
    MegaUser *peer1 = megaApi[1]->getContact(email[0].c_str());
    assert(peer0 && peer1);

    MegaChatRoom *chatroom0 = megaChatApi[0]->getChatRoomByUser(peer0->getHandle());
    if (!chatroom0) // chat 1on1 doesn't exist yet --> create it
    {
        MegaChatPeerList *peers = MegaChatPeerList::createInstance();
        peers->addPeer(peer0->getHandle(), MegaChatPeerList::PRIV_STANDARD);

        bool *chatCreated = &chatUpdated[0]; *chatCreated = false;
        bool *chatReceived = &chatUpdated[1]; *chatReceived = false;
        megaChatApi[0]->createChat(false, peers, this);
        assert(waitForResponse(chatCreated));
        assert(waitForResponse(chatReceived));

        chatroom0 = megaChatApi[0]->getChatRoomByUser((MegaChatHandle)peer0);
    }

    MegaChatHandle chatid0 = chatroom0->getChatId();
    assert (chatid0 != MEGACHAT_INVALID_HANDLE);
    delete chatroom0; chatroom0 = NULL;

    MegaChatRoom *chatroom1 = megaChatApi[1]->getChatRoomByUser(peer1->getHandle());
    MegaChatHandle chatid1 = chatroom1->getChatId();
    assert (chatid0 == chatid1);

    // 1. A sends a message to B while B has the chat opened.
    // --> check the confirmed in A, the received message in B, the delivered in A

    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid0);
    assert(megaChatApi[0]->openChatRoom(chatid0, chatroomListener));
    assert(megaChatApi[1]->openChatRoom(chatid1, chatroomListener));

    sleep(300);

    string msg0 = "HOLA " + email[0] + " - This is a testing message automatically sent to you";
    bool *flagConfirmed = &chatroomListener->msgConfirmed[0]; *flagConfirmed = false;
    bool *flagReceived = &chatroomListener->msgReceived[1]; *flagReceived = false;
    bool *flagDelivered = &chatroomListener->msgDelivered[0]; *flagDelivered = false;
    chatroomListener->msgId[0] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[1] = MEGACHAT_INVALID_HANDLE;   // will be set at reception

    MegaChatMessage *msgSent = megaChatApi[0]->sendMessage(chatid0, msg0.c_str());
    assert(msgSent);
    delete msgSent; msgSent = NULL;

    assert(waitForResponse(flagConfirmed));    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId0 = chatroomListener->msgId[0];
    assert (msgId0 != MEGACHAT_INVALID_HANDLE);

    assert(waitForResponse(flagReceived));    // for reception
    MegaChatHandle msgId1 = chatroomListener->msgId[1];
    assert (msgId0 == msgId1);
    MegaChatMessage *msgReceived = megaChatApi[1]->getMessage(chatid1, msgId0);   // message should be already received, so in RAM
    assert(msgReceived && !strcmp(msg0.c_str(), msgReceived->getContent()));
    assert(waitForResponse(flagDelivered));    // for delivery
    delete msgReceived; msgReceived = NULL;

    // edit the message
    msg0 = "This is an edited message to " + email[0];
    bool *flagEdited = &chatroomListener->msgEdited[0]; *flagEdited = false;
    flagReceived = &chatroomListener->msgReceived[1]; *flagReceived = false;
    flagDelivered = &chatroomListener->msgDelivered[0]; *flagDelivered = false;
    chatroomListener->msgId[0] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[1] = MEGACHAT_INVALID_HANDLE;   // will be set at reception

    MegaChatMessage *msgEdited = megaChatApi[0]->editMessage(chatid0, msgId0, msg0.c_str());
    assert(msgEdited);  // rejected because of age (more than one hour) --> shouldn't happen
    delete msgEdited; msgEdited = NULL;

    assert(waitForResponse(flagEdited));    // for confirmation, editMessage() is synchronous
    msgId0 = chatroomListener->msgId[0];
    assert (msgId0 != MEGACHAT_INVALID_HANDLE);
    msgEdited = megaChatApi[0]->getMessage(chatid0, msgId0);
    assert (msgEdited && msgEdited->isEdited());

    assert(waitForResponse(flagReceived));    // for reception
    msgId1 = chatroomListener->msgId[1];
    assert (msgId0 == msgId1);
    msgReceived = megaChatApi[1]->getMessage(chatid1, msgId1);   // message should be already received, so in RAM
    assert(msgReceived && !strcmp(msgEdited->getContent(), msgReceived->getContent()));
    assert(msgReceived->isEdited());
    assert(waitForResponse(flagDelivered));    // for delivery



    megaChatApi[0]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[1]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    // 2. A sends a message to B while B doesn't have the chat opened.
    // Then, B opens the chat --> check the received message in B, the delivered in A


    logout(1, true);
    logout(0, true);
}

/**
 * @brief MegaChatApiTest::TEST_groupChatManagement
 */
void MegaChatApiTest::TEST_groupChatManagement()
{
    login(0);
    login(1);

    // Prepare peers, privileges...
    MegaUser *peer = megaApi[0]->getContact(email[1].c_str());
    assert(peer);
    MegaChatPeerList *peers = MegaChatPeerList::createInstance();
    peers->addPeer(peer->getHandle(), MegaChatPeerList::PRIV_STANDARD);
    MegaChatHandle chatid = MEGACHAT_INVALID_HANDLE;

    // Create the GroupChat
    bool *flag = &requestFlagsChat[0][MegaChatRequest::TYPE_CREATE_CHATROOM]; *flag = false;
    bool *chatReceived = &chatUpdated[1]; *chatReceived = false;
    megaChatApi[0]->createChat(true, peers);
    assert(waitForResponse(flag));
    assert(waitForResponse(chatReceived));

    // Check we got a new chat ID...
    delete peers;   peers = NULL;
    chatid = this->chatid;
    assert (chatid != MEGACHAT_INVALID_HANDLE);
    // ...and the auxiliar account also received the chatroom
    MegaChatRoom *chatroom = megaChatApi[1]->getChatRoom(chatid);
    assert (chatroom);
    delete chatroom;

    // Open chatroom
    TestChatRoomListener *chatroomListener = new TestChatRoomListener(megaChatApi, chatid);
    assert(megaChatApi[0]->openChatRoom(chatid, chatroomListener));
    assert(megaChatApi[1]->openChatRoom(chatid, chatroomListener));

    // Load some message to feed history
    flag = &chatroomListener->historyLoaded[0]; *flag = false;
    megaChatApi[0]->loadMessages(chatid, 16);
    assert(waitForResponse(flag));
    flag = &chatroomListener->historyLoaded[1]; *flag = false;
    megaChatApi[1]->loadMessages(chatid, 16);
    assert(waitForResponse(flag));


    // Send a message and wait for reception by target user
    string msg0 = "HOLA " + email[0] + " - This is a testing message automatically sent";
    bool *msgConfirmed = &chatroomListener->msgConfirmed[0]; *msgConfirmed = false;
    bool *msgReceived = &chatroomListener->msgReceived[1]; *msgReceived = false;
    bool *msgDelivered = &chatroomListener->msgDelivered[0]; *msgDelivered = false;
    chatroomListener->msgId[0] = MEGACHAT_INVALID_HANDLE;   // will be set at confirmation
    chatroomListener->msgId[1] = MEGACHAT_INVALID_HANDLE;   // will be set at reception
    megaChatApi[0]->sendMessage(chatid, msg0.c_str());
    assert(waitForResponse(msgConfirmed));    // for confirmation, sendMessage() is synchronous
    MegaChatHandle msgId = chatroomListener->msgId[0];
    assert (msgId != MEGACHAT_INVALID_HANDLE);
    assert(waitForResponse(msgReceived));    // for reception
    assert (msgId == chatroomListener->msgId[1]);
    MegaChatMessage *msg = megaChatApi[1]->getMessage(chatid, msgId);   // message should be already received, so in RAM
    assert(msg && !strcmp(msg0.c_str(), msg->getContent()));
    assert(waitForResponse(msgDelivered));    // for delivery


    // Close the chatroom
    megaChatApi[0]->closeChatRoom(chatid, chatroomListener);
    megaChatApi[1]->closeChatRoom(chatid, chatroomListener);
    delete chatroomListener;

    // Remove the GroupChat
    flag = &requestFlagsChat[0][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flag = false;
    megaChatApi[0]->leaveChat(chatid);
    assert(waitForResponse(flag));

    flag = &requestFlagsChat[1][MegaChatRequest::TYPE_REMOVE_FROM_CHATROOM]; *flag = false;
    megaChatApi[1]->leaveChat(chatid);
    assert(waitForResponse(flag));

    logout(1, true);
    logout(0, true);
}

MegaLoggerSDK::MegaLoggerSDK(const char *filename)
{
    sdklog.open(filename, ios::out | ios::app);
}

MegaLoggerSDK::~MegaLoggerSDK()
{
    sdklog.close();
}

void MegaLoggerSDK::log(const char *time, int loglevel, const char *source, const char *message)
{
    sdklog << "[" << time << "] " << SimpleLogger::toStr((LogLevel)loglevel) << ": ";
    sdklog << message << " (" << source << ")" << endl;

//    bool errorLevel = ((loglevel == logError) && !testingInvalidArgs);
//    ASSERT_FALSE(errorLevel) << "Test aborted due to an SDK error.";
}

void MegaChatApiTest::onRequestFinish(MegaChatApi *api, MegaChatRequest *request, MegaChatError *e)
{
    int apiIndex = -1;
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }
    if (apiIndex == -1)
    {
        cout << "Instance of MegaChatApi not recognized" << endl;
        return;
    }

    requestFlagsChat[apiIndex][request->getType()] = true;
    lastError[apiIndex] = e->getErrorCode();

    switch(request->getType())
    {
        case MegaChatRequest::TYPE_CREATE_CHATROOM:
            chatid = request->getChatHandle();
            break;
    }
}

void MegaChatApiTest::onOnlineStatusUpdate(MegaChatApi *api, int status)
{
    int apiIndex = -1;
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }
    if (apiIndex == -1)
    {
        cout << "Instance of MegaChatApi not recognized" << endl;
        return;
    }

    cout << "[api: " << apiIndex << "] Online status updated: " << status << endl;
}

void MegaChatApiTest::onChatRoomUpdate(MegaChatApi *api, MegaChatRoom *chat)
{
    int apiIndex = -1;
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }
    if (apiIndex == -1)
    {
        cout << "Instance of MegaChatApi not recognized" << endl;
        return;
    }

    if (chat != NULL)
    {
        cout << "[api: " << apiIndex << "] Chat added or updated (" << chat->getChatId() << ")" << endl;
        chatUpdated[apiIndex] = chat->getChatId();
    }
    else
    {
        cout << "[api: " << apiIndex << "] " << megaChatApi[0]->getChatRooms()->size() << " chat/s received" << endl;
    }
}

void MegaChatApiTest::onChatListItemUpdate(MegaChatApi *api, MegaChatListItem *item)
{
    int apiIndex = -1;
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }
    if (apiIndex == -1)
    {
        cout << "Instance of MegaChatApi not recognized" << endl;
        return;
    }

    if (item)
    {
        cout << "[api: " << apiIndex << "] Chat list item added or updated - ";
        printChatListItemInfo(item);
        chatUpdated[apiIndex] = item->getChatId();
    }
}

TestChatRoomListener::TestChatRoomListener(MegaChatApi **apis, MegaChatHandle chatid)
{
    this->megaChatApi = apis;
    this->chatid = chatid;

    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        this->historyLoaded[i] = false;
        this->msgLoaded[i] = false;
        this->msgConfirmed[i] = false;
        this->msgDelivered[i] = false;
        this->msgReceived[i] = false;
        this->msgEdited[i] = false;
        this->msgId[i] = MEGACHAT_INVALID_HANDLE;
        this->chatUpdated[i] = false;
    }
}

void TestChatRoomListener::onChatRoomUpdate(MegaChatApi *api, MegaChatRoom *chat)
{
    int apiIndex = -1;
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }
    if (apiIndex == -1)
    {
        cout << "Instance of MegaChatApi not recognized" << endl;
        return;
    }

    cout << "[api: " << apiIndex << "] Chat updated (" << chat->getChatId() << ")" << endl;
    chatUpdated[apiIndex] = chat->getChatId();
}

void TestChatRoomListener::onMessageLoaded(MegaChatApi *api, MegaChatMessage *msg)
{
    int apiIndex = -1;
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }
    if (apiIndex == -1)
    {
        cout << "Instance of MegaChatApi not recognized" << endl;
        return;
    }

    if (msg)
    {
        cout << "[api: " << apiIndex << "] Message loaded - ";
        MegaChatApiTest::printMessageInfo(msg);

        msgLoaded[apiIndex] = true;
        msgId[apiIndex] = msg->getMsgId();
    }
    else
    {
        historyLoaded[apiIndex] = true;
        cout << "[api: " << apiIndex << "] Loading of messages completed" << endl;
    }
}

void TestChatRoomListener::onMessageReceived(MegaChatApi *api, MegaChatMessage *msg)
{
    int apiIndex = -1;
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }
    if (apiIndex == -1)
    {
        cout << "Instance of MegaChatApi not recognized" << endl;
        return;
    }

    cout << "[api: " << apiIndex << "] Message received - ";
    MegaChatApiTest::printMessageInfo(msg);

    msgReceived[apiIndex] = true;
    msgId[apiIndex] = msg->getMsgId();
}

void TestChatRoomListener::onMessageUpdate(MegaChatApi *api, MegaChatMessage *msg)
{
    int apiIndex = -1;
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == this->megaChatApi[i])
        {
            apiIndex = i;
            break;
        }
    }
    if (apiIndex == -1)
    {
        cout << "TEST - Instance of MegaChatApi not recognized" << endl;
        return;
    }

    cout << "[api: " << apiIndex << "] Message updated - ";
    MegaChatApiTest::printMessageInfo(msg);

    msgConfirmed[apiIndex] = (msg->getStatus() == MegaChatMessage::STATUS_SERVER_RECEIVED);
    msgDelivered[apiIndex] = (msg->getStatus() == MegaChatMessage::STATUS_DELIVERED);
    msgId[apiIndex] = msg->getMsgId();
}

void MegaChatApiTest::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
{
    int apiIndex = -1;
    for (int i = 0; i < NUM_ACCOUNTS; i++)
    {
        if (api == megaApi[i])
        {
            apiIndex = i;
            break;
        }
    }
    if (apiIndex == -1)
    {
        cout << "TEST - Instance of MegaApi not recognized" << endl;
        return;
    }

    requestFlags[apiIndex][request->getType()] = true;
    lastError[apiIndex] = e->getErrorCode();

    switch(request->getType())
    {
    }
}


MegaChatLoggerSDK::MegaChatLoggerSDK(const char *filename)
{
    sdklog.open(filename, ios::out | ios::app);
}

MegaChatLoggerSDK::~MegaChatLoggerSDK()
{
    sdklog.close();
}

void MegaChatLoggerSDK::log(int loglevel, const char *message)
{
    string levelStr;

    switch (loglevel)
    {
        case MegaChatApi::LOG_LEVEL_ERROR: levelStr = "err"; break;
        case MegaChatApi::LOG_LEVEL_WARNING: levelStr = "warn"; break;
        case MegaChatApi::LOG_LEVEL_INFO: levelStr = "info"; break;
        case MegaChatApi::LOG_LEVEL_VERBOSE: levelStr = "verb"; break;
        case MegaChatApi::LOG_LEVEL_DEBUG: levelStr = "debug"; break;
        case MegaChatApi::LOG_LEVEL_MAX: levelStr = "debug-verbose"; break;
        default: levelStr = ""; break;
    }

    // message comes with a line-break at the end
    sdklog  << message;
}