﻿#ifndef __SDL_CONNECTOR_H__
#define __SDL_CONNECTOR_H__
#include "ISocketManager.h"
#include "ProtocolDefines.h"
#include "BasicCommunication.h"
#include "Buttons.h"
#include "Navigation.h"
#include "TTS.h"
#include "UI.h"
#include "VehicleInfo.h"
#include "VR.h"
#include "CVideoStream.h"
#include "SocketsToSDL.h"
#include <string>
#include "json/json.h"
#include <stdio.h>
#ifdef ANDROID
#include <unistd.h>
#endif

#define ToSDL SDLConnector::getSDLConnector()

class SDLConnector : public INetworkStatus
{
private:
    SDLConnector();
    ~SDLConnector();

public:
    static SDLConnector * getSDLConnector();
    static void Close();

    bool ConnectToSDL(IMessageInterface * pMsgHandler, INetworkStatus * pNetwork = NULL);
    bool ConnectToVideoStream(IMessageInterface * pMsgHandler, std::string sIP, int iPort, INetworkStatus * pNetwork = NULL);
    void DelConnectToVideoStream();
    void ChangeMsgHandler(IMessageInterface * pMsgHandler);
    bool IsSDLConnected();

private:
    bool m_bReleased;
    SocketsToSDL m_Sockets;
    std::vector<IChannel*> m_channels;
    bool    m_sdl_is_connected;

    VR m_VR;
    BasicCommunication m_Base;
    Buttons m_Buttons;
    Navigation m_Navi;    
    TTS m_TTS;
    VehicleInfo m_Vehicle;
    UI m_UI;
    CVideoStream m_VideoStream;

    INetworkStatus * m_pNetwork;
    IMessageInterface * m_pMsgHandler;

public:
    virtual void onConnected();
    virtual void onNetworkBroken();
    static void* ConnectThread(void*);
    void Connect();

// API
public:
    void OnAppActivated(int appID);
    void OnAppExit(int appID);
    void OnAppOut(int appID);

    // mode: SHORT or LONG
    void OnSoftButtonClick(int id, int mode,std::string strName = "");

    // mode: SHORT or LONG
    void OnButtonClick(std::string buttonname, int mode);

    // reason: timeout clickSB and aborted
    void OnAlertResponse(int alertID, int reason);

    void OnMediaClockResponse(int id,int code);

    // reason: timeout clickSB and rejected
    void OnScrollMessageResponse(int smID, int reason);

    void OnCommandClick(int appID,int cmdID);
    void OnVRCommand(int appID,int cmdID);

    // code: timeout or choice
    void OnPerformInteraction(int code, int performInteractionID, int choiceID);
    void OnVRPerformInteraction(int code, int performInteractionID, int choiceID);

    // code: timeout sborted select someone
    void OnSliderResponse( int code, int sliderid, int sliderPosition);

    // code: timeout retry cancel
    void OnPerformAudioPassThru(int appID, int performaudiopassthruID, int code);

    // code: OK or Interrupted
    void OnTTSSpeek(int speekID, int code);

    void OnVRStartRecord(void);
    void OnVRCancelRecord(void);

    //touch event
    void OnVideoScreenTouch(TOUCH_TYPE touch,int x,int y);

    void OnSetMediaClockTimerResponse(int iCode,int iRequestId);

    // add by fanqiang
    // search device
    void OnStartDeviceDiscovery(void);
    void OnDeviceChosen(std::string name, std::string id);
    void OnFindApplications(std::string name, std::string id);

private:
    void _onButtonClickAction(std::string, std::string, int);
    void _stopPerformAudioPassThru(int);
    void _buttonEventDown(std::string buttonname);
    void _buttonPressed(std::string buttonname, int mode);
    void _buttonEventUp(std::string buttonname);
};

#endif
