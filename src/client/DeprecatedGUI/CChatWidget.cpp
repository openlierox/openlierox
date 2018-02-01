/////////////////////////////////////////
//
//             OpenLieroX
//
// code under LGPL, based on JasonBs work,
// enhanced by Dark Charlie and Albert Zeyer
//
//
/////////////////////////////////////////


// Net menu - News
// Created 12/8/02
// Jason Boettcher

#include <set>
#include "DeprecatedGUI/CChatWidget.h"
#include "LieroX.h"
#include "sound/SoundsBase.h"
#include "DeprecatedGUI/Graphics.h"
#include "DeprecatedGUI/Menu.h"
#include "GfxPrimitives.h"
#include "DeprecatedGUI/CButton.h"
#include "DeprecatedGUI/CBrowser.h"
#include "DeprecatedGUI/CListview.h"
#include "DeprecatedGUI/CTextbox.h"
#include "DeprecatedGUI/CLabel.h"
#include "DeprecatedGUI/CBox.h"
#include "DeprecatedGUI/CCheckbox.h"
#include "IRC.h"
#include "StringUtils.h"
#include "AuxLib.h"
#include "NotifyUser.h"
#include "DeprecatedGUI/CImage.h"
#include "DeprecatedGUI/CBox.h"
#include "Mutex.h"


namespace DeprecatedGUI {

	enum {
		nc_ChatText,
		nc_UserList,
		nc_ChatInput,
		nc_Back, // Back button is not added by default, it's only for global chat widget
	};

bool GlobalChatWidget_enabled = false;

static std::set<CChatWidget *> chatWidgets; // Instances of CChatWidget
static Mutex chatWidgetsMutex;

//#define DEBUG_CHATWIDGET

#ifdef DEBUG_CHATWIDGET
#define DEBUG_CW_PRINT(X) X
void PrintGlobalChatWidgets()
{
	hints << "chatWidgets: ";
	for( std::set<CChatWidget *> :: iterator cw = chatWidgets.begin(); cw != chatWidgets.end(); cw++ )
	{
		hints << *cw << " ";
	}
	hints << endl;
	for( std::set<CChatWidget *> :: iterator cw = chatWidgets.begin(); cw != chatWidgets.end(); cw++ )
	{
		CListview *lsv = (CListview *)(*cw)->getWidget(nc_UserList);; // Trying to access NULL ptr
		bool oldStyle = lsv->getOldStyle();
		lsv->setOldStyle(oldStyle);
	}
};
#else
#define DEBUG_CW_PRINT(X)
void PrintGlobalChatWidgets() {};
#endif

void ChatWidget_ChatRegisterCallbacks();
void ChatWidget_ChatDeregisterCallbacks();
void ChatWidget_ChatNewMessage(const std::string& msg, int type);

/////////////////////////
// Re-fills the user list
void ChatWidget_ChatRefillUserList()
{
	Mutex::ScopedLock lock(chatWidgetsMutex);
	
	DEBUG_CW_PRINT( hints << "ChatWidget_ChatRefillUserList() " << endl; )
	PrintGlobalChatWidgets();
	
	const IRCClient *irc = GetGlobalIRC();
	if ( !irc || !irc->isConnected() )
			return;

	for( std::set<CChatWidget *> :: iterator cw = chatWidgets.begin(); cw != chatWidgets.end(); cw++ )
	{
		// User list
		CListview *l = (CListview *)(*cw)->getWidget(nc_UserList);
		l->Clear();
		int i = 0;
		for (std::list<std::string>::const_iterator it = irc->getUserList().begin(); it != irc->getUserList().end(); it++) {
			l->AddItem(*it, i, tLX->clListView);
			l->AddSubitem(LVS_TEXT, *it, (DynDrawIntf*)NULL, NULL);
		}
	}
}

CChatWidget::CChatWidget()
{
	DEBUG_CW_PRINT( hints << "CChatWidget::CChatWidget() " << this << endl; )
	m_lastWhoisTime = tLX->currentTime;
}

CChatWidget::~CChatWidget()
{
	DEBUG_CW_PRINT( hints << "CChatWidget::~CChatWidget() " << this << endl; )
	Destroy();
}

void CChatWidget::Create()
{
	CGuiSkinnedLayout::SetOffset( iX, iY );

	if( iWidth < 170 || iHeight < tLX->cFont.GetHeight() + 40 )
		warnings << "CChatWidget::CChatWidget(): too small dimensions given" << endl;

	this->Add( new CBrowser(), nc_ChatText, 5, 5, iWidth - 155, iHeight - tLX->cFont.GetHeight() - 15 );
	this->Add( new CListview(), nc_UserList, iWidth - 145, 5, 145, iHeight - 10);
	this->Add( new CTextbox(), nc_ChatInput, 5,  iHeight - tLX->cFont.GetHeight() - 7, iWidth - 157, tLX->cFont.GetHeight());
	
	CListview *l = (CListview *)this->getWidget(nc_UserList);
	l->setMouseOverEventEnabled(true);

	CBrowser *b = (CBrowser *)this->getWidget(nc_ChatText);
	b->InitializeChatBox();

	{
		Mutex::ScopedLock lock(chatWidgetsMutex);
		chatWidgets.insert(this);
		DEBUG_CW_PRINT( hints << "CChatWidget::Create() " << this << endl; )
		PrintGlobalChatWidgets();
	}
	
	IRCClient *irc = GetGlobalIRC();
	if (irc)
		for (std::list<IRCClient::IRCChatLine>::const_iterator it = irc->getMessageList().begin();
			it != irc->getMessageList().end(); it++)  {
				ChatWidget_ChatNewMessage(it->text, it->type);
		}

	ChatWidget_ChatRegisterCallbacks();
	ChatWidget_ChatRefillUserList();

	CGuiSkinnedLayout::Create();

}

void CChatWidget::Draw(SDL_Surface *bmpDest)
{
	if (!tMenu)
		return;

	CListview *lsv = (CListview *)this->getWidget(nc_ChatText);
	if (!lsv)
		return;

	if (tMenu->bmpChatBackgroundMain.get())
		DrawImage(bmpDest, tMenu->bmpChatBackgroundMain.get(), iX, iY);

	if (tMenu->bmpChatBackground.get())
		DrawImageAdv(bmpDest, tMenu->bmpChatBackground.get(), 0, 0, lsv->getX() + 1, lsv->getY() + 1, lsv->getWidth() - 2, lsv->getHeight() - 2);

	CGuiSkinnedLayout::Draw(bmpDest);
}

void CChatWidget::Destroy()
{
	Mutex::ScopedLock lock(chatWidgetsMutex);

	chatWidgets.erase(this);

	DEBUG_CW_PRINT( hints << "CChatWidget::Destroy() " << this << endl; )
	PrintGlobalChatWidgets();

	/*
	if( chatWidgets.empty() )
		ChatWidget_ChatDeregisterCallbacks();
	*/
	CGuiSkinnedLayout::Destroy();
}

void CChatWidget::EnableChat()
{
	ChatWidget_ChatRegisterCallbacks();
}
	
void CChatWidget::DisableChat()
{
	ChatWidget_ChatDeregisterCallbacks();

	Mutex::ScopedLock lock(chatWidgetsMutex);
	DEBUG_CW_PRINT( hints << "CChatWidget::DisableChat() " << endl; )
	PrintGlobalChatWidgets();
	for( std::set<CChatWidget *> :: iterator cw = chatWidgets.begin(); cw != chatWidgets.end(); cw++ )
	{
		((CListview *)(*cw)->getWidget(nc_UserList))->Clear();
	}
}

void CChatWidget::ProcessChildEvent(int iEvent, CWidget * child)
{
	if( ! child )
		return;

	switch( child->getID() )
	{
			case nc_ChatInput:
				if(iEvent == TXT_ENTER) {
					
					CTextbox *txt = (CTextbox *)this->getWidget(nc_ChatInput);
					std::string text = txt->getText();

                    if(text.size() == 0) // Don't send empty messages
                        break;

					if(GetGlobalIRC() && GetGlobalIRC()->sendChat(text))
						txt->setText(""); // Clear the text box
				}
				break;

			case nc_UserList:
				if (iEvent == LV_DOUBLECLK || iEvent == LV_ENTER)  {
					CTextbox *txt = (CTextbox *)this->getWidget(nc_ChatInput);
					CListview *lsv = (CListview *)this->getWidget(nc_UserList);
					if (txt)  {
						std::string nick = lsv->getCurSIndex();

						// If there already is some text in the chatbox, insert the nick at the end, optionally adding a space
						if (txt->getText().size())  {
							if (isspace((uchar)*(txt->getText().rbegin())))
								txt->setText(txt->getText() + nick);
							else
								txt->setText(txt->getText() + " " + nick);

						// The textbox is empty, insert "nick: "
						} else {
							txt->setText(nick + ": ");
						}

						txt->setCurPos(txt->getText().size());

						this->FocusWidget(nc_ChatInput);
					}
				}
				if (iEvent == LV_MOUSEOVER)  {
					CListview *lsv = (CListview *)this->getWidget(nc_UserList);
					IRCClient *irc = GetGlobalIRC();
					CGuiLayout* layout = dynamic_cast<CGuiLayout*> (getParent());
					if(layout == NULL)
						// we can only set the tooltip on the CGuiLayout
						break;
					
					std::string tooltipTxt;
					if( m_lastWhoisName != lsv->getMouseOverSIndex() && 
						m_lastWhoisTime + 0.5f < tLX->currentTime )
					{
						m_lastWhoisTime = tLX->currentTime;
						m_lastWhoisName = lsv->getMouseOverSIndex();
						//popup->setText("");
					}
					if(lsv->getMouseOverSIndex() != "") {
						tooltipTxt = lsv->getMouseOverSIndex() + "\n... loading ...";
					}
					if( lsv->getMouseOverSIndex() != "" && lsv->getMouseOverSIndex() == m_lastWhoisName &&
						irc != NULL && irc->getWhois(lsv->getMouseOverSIndex()) != "" )
					{
						tooltipTxt = lsv->getMouseOverSIndex() + "\n" + irc->getWhois(lsv->getMouseOverSIndex());
					}
					
					int x = GetMouse()->X;
					int y = GetMouse()->Y;
					layout->setTooltip(MakeRect(x-1,y-1,2,2), VectorD2<int>(x,y), tooltipTxt);
				}
			break;
			case nc_Back:
				if(iEvent == BTN_CLICKED) {

					// Click!
					PlaySoundSample(sfxGeneral.smpClick);
					
					GlobalChatWidget_enabled = false;
				}
				break;
	}
}

void ChatWidget_ChatNewMessage(const std::string& msg, int type)
{
	Mutex::ScopedLock lock(chatWidgetsMutex);

	DEBUG_CW_PRINT( hints << "ChatWidget_ChatNewMessage() " << endl; )
	PrintGlobalChatWidgets();
	
	for( std::set<CChatWidget *> :: iterator cw = chatWidgets.begin(); cw != chatWidgets.end(); cw++ )
	{
		CBrowser *brw = (CBrowser *)(*cw)->getWidget(nc_ChatText);
		if (brw && GetGlobalIRC())  {
			switch (type)  {
			case IRCClient::IRC_TEXT_CHAT:
				brw->AddChatBoxLine(msg, tLX->clSubHeading, TXT_CHAT); // clChatText is almost blue, which looks ugly on blue background
				break;
			case IRCClient::IRC_TEXT_NOTICE:
				brw->AddChatBoxLine(msg, tLX->clNotice, TXT_NOTICE);
				break;
			case IRCClient::IRC_TEXT_ACTION:
				brw->AddChatBoxLine(msg, tLX->clNetworkText, TXT_CHAT);
				break;
			case IRCClient::IRC_TEXT_PRIVATE:
				brw->AddChatBoxLine(msg, tLX->clNetworkText, TXT_PRIVATE);
				break;
			default:
				brw->AddChatBoxLine(msg, tLX->clSubHeading, TXT_CHAT);
			}
		}
	}
	// Notify the user if the message contains his nick
	if (stringtolower(msg).find(stringtolower(GetGlobalIRC()->getNick())) != std::string::npos ||
		type == IRCClient::IRC_TEXT_PRIVATE)
		NotifyUserOnEvent();
}

////////////////////////
// Chat disconnect (callback)
void ChatWidget_ChatDisconnect()
{
	Mutex::ScopedLock lock(chatWidgetsMutex);

	DEBUG_CW_PRINT( hints << "ChatWidget_ChatDisconnect() " << endl; )
	PrintGlobalChatWidgets();

	for( std::set<CChatWidget *> :: iterator cw = chatWidgets.begin(); cw != chatWidgets.end(); cw++ )
	{
		CListview *lsv = (CListview *)((*cw)->getWidget(nc_UserList));
		if (lsv)
			lsv->Clear();
	}
}

////////////////////////
// Chat connect (callback)
void ChatWidget_ChatConnect()
{
	ChatWidget_ChatRefillUserList();
}

///////////////////////
// Update users (callback)
void ChatWidget_ChatUpdateUsers(const std::list<std::string>& users)
{
	Mutex::ScopedLock lock(chatWidgetsMutex);
	DEBUG_CW_PRINT( hints << "ChatWidget_ChatUpdateUsers() " << endl; )
	PrintGlobalChatWidgets();
	
	for( std::set<CChatWidget *> :: iterator cw = chatWidgets.begin(); cw != chatWidgets.end(); cw++ )
	{
		CListview *lsv = (CListview *)(*cw)->getWidget(nc_UserList);
		if (lsv)  {
			lsv->Clear();

			// Add the users
			int i = 0;
			for (std::list<std::string>::const_iterator it = users.begin(); it != users.end(); it++)  {
				lsv->AddItem(*it, i, tLX->clListView);
				lsv->AddSubitem(LVS_TEXT, *it, (DynDrawIntf*)NULL, NULL);
			}
		}
	}
}

///////////////////////////
// Register the IRC event listeners
void ChatWidget_ChatRegisterCallbacks()
{
	Mutex::ScopedLock lock(chatWidgetsMutex);
	DEBUG_CW_PRINT( hints << "ChatWidget_ChatRegisterCallbacks() " << endl; )
	PrintGlobalChatWidgets();

	IRCClient *irc = GetGlobalIRC();

	// Setup the callbacks
	if (irc)  {
		irc->setNewMessageCallback(&ChatWidget_ChatNewMessage);
		irc->setDisconnectCallback(&ChatWidget_ChatDisconnect);
		irc->setConnectCallback(&ChatWidget_ChatConnect);
		irc->setUpdateUserListCallback(&ChatWidget_ChatUpdateUsers);
	}
}

//////////////////////////
// Clear the callbacks
void ChatWidget_ChatDeregisterCallbacks()
{
	Mutex::ScopedLock lock(chatWidgetsMutex);
	DEBUG_CW_PRINT( hints << "ChatWidget_ChatDeregisterCallbacks() " << endl; )
	PrintGlobalChatWidgets();

	IRCClient *irc = GetGlobalIRC();
	if (irc)  {
		irc->clearNewMessageCallback(&ChatWidget_ChatNewMessage);
		irc->clearDisconnectCallback(&ChatWidget_ChatDisconnect);
		irc->clearConnectCallback(&ChatWidget_ChatConnect);
		irc->clearUpdateUserListCallback(&ChatWidget_ChatUpdateUsers);
	}
}

bool CChatWidget::GlobalEnabled()
{
	if( tLX->cIrcChat.isDownOnce() )
	{
		GlobalChatWidget_enabled = ! GlobalChatWidget_enabled;
	}
	tLX->cIrcChat.reset();
	if( WasKeyboardEventHappening(SDLK_ESCAPE,false) )
		GlobalChatWidget_enabled = false;
	return GlobalChatWidget_enabled;
}

void CChatWidget::GlobalSetEnabled(bool enabled)
{
	GlobalChatWidget_enabled = enabled;
}

static CChatWidget * globalChat = NULL;

void CChatWidget::GlobalProcessAndDraw(SDL_Surface * bmpDest)
{
	if( ! globalChat )
	{
		int setupX = 30;
		int setupY = 100;
		int setupW = 580;
		int setupH = 280;
		int borderX = 10;
		int borderY = 40;

		globalChat = new CChatWidget();
		globalChat->Setup(-1, setupX, setupY, setupW, setupH);
		//globalChat->Setup(-1, setupX-borderX, setupY-borderY, setupW+borderX*2, setupH+borderY*2);
		globalChat->SetOffset( globalChat->getX(), globalChat->getY() );
		globalChat->Add( new CImage( DynDrawFromSurfaceCrop(LoadGameImage("data/frontend/background_common.png") , 
										0, 0, globalChat->getWidth()+borderX*2, globalChat->getHeight()+borderY*2) ), 
								-1, -borderX, -borderY, globalChat->getWidth()+borderX*2, globalChat->getHeight()+borderY*2);
		globalChat->Add( new CBox( 6, 4, tLX->clBoxLight, tLX->clBoxDark, tLX->clPink ), -1, -borderX, -borderY, globalChat->getWidth()+borderX*2, globalChat->getHeight()+borderY*2);
		globalChat->Add( new CButton(BUT_CHAT, tMenu->bmpButtons), -1, globalChat->getWidth()/2-20, -borderY+5, 0, 0);
		globalChat->Add( new CButton(BUT_BACK, tMenu->bmpButtons), nc_Back, 0, globalChat->getHeight()+15, 50,15);
		globalChat->Create();
		//globalChat->Setup(-1, globalChat->getX(), globalChat->getY(), globalChat->getWidth(), globalChat->getHeight()+30); // So Back button will be in mouse-checked rect
	}
	globalChat->Process();
	globalChat->Draw(bmpDest);
	DrawCursor(bmpDest);
};

void CChatWidget::GlobalDestroy()
{
	if( globalChat )
	{
		delete globalChat;
		globalChat = NULL;
	}
};


}; // namespace DeprecatedGUI
