﻿#include "global_first.h"
#include <SDLConnector.h>
#include <vector>
#include <iostream>
#include <pthread.h>

#ifdef linux
#include <sys/time.h>
#include <unistd.h>
#endif

static SDLConnector * g_SingleConnector = 0;

SDLConnector::SDLConnector() : m_bReleased(false), m_Sockets(), m_VR(), m_Base(), m_Buttons(), m_Navi(), m_TTS(), m_Vehicle(), m_UI()
{
    m_sdl_is_connected = false;
}

SDLConnector::~SDLConnector()
{
    g_SingleConnector =  0;
    m_bReleased = true;
}

void SDLConnector::Close()
{
    if (g_SingleConnector) {
        delete g_SingleConnector;
        g_SingleConnector =  0;
    }
}


SDLConnector * SDLConnector::getSDLConnector()
{
    if (g_SingleConnector == 0)
        g_SingleConnector = new SDLConnector();
    return g_SingleConnector;
}

void SDLConnector::onConnected()
{
}

void SDLConnector::onNetworkBroken()
{
    /*
    if (m_pNetwork)
        m_pNetwork->onNetworkBroken();

    while (!m_bReleased) {
#if defined(WIN32)|| defined(WINCE)
        Sleep(1000);
#else
        usleep(1000000);
#endif
        if (!ConnectToSDL(m_pMsgHandler,m_pNetwork)) {
            if (m_pNetwork)
                m_pNetwork->onConnected();
            break;
        }
    }
    */
    m_sdl_is_connected = false;
    ConnectToSDL(m_pMsgHandler);
}

bool SDLConnector::IsSDLConnected()
{
    return m_sdl_is_connected;
}

bool SDLConnector::ConnectToSDL(IMessageInterface * pMsgHandler, INetworkStatus * pNetwork)
{
    m_pNetwork = pNetwork;
    m_pMsgHandler = pMsgHandler;

    ChangeMsgHandler(pMsgHandler);
    //std::vector<IChannel*> m_channels;
    m_channels.clear();
    m_channels.push_back(&m_VR);
    m_channels.push_back(&m_Vehicle);
    m_channels.push_back(&m_UI);
    m_channels.push_back(&m_TTS);
    m_channels.push_back(&m_Navi);
    m_channels.push_back(&m_Buttons);
    m_channels.push_back(&m_Base);

    pthread_t  thread_connect;
    pthread_create(&thread_connect,NULL,SDLConnector::ConnectThread,this);
    return m_sdl_is_connected;
}

bool SDLConnector::ConnectToVideoStream(IMessageInterface * pMsgHandler, std::string sIP, int iPort, INetworkStatus * pNetwork)
{
    m_pNetwork = pNetwork;
    m_pMsgHandler = pMsgHandler;

    m_VideoStream.SetCallback(pMsgHandler);

    m_channels.push_back(&m_VideoStream);

    m_sdl_is_connected = m_Sockets.ConnectToVS(&m_VideoStream, sIP, iPort, this);
    if (m_sdl_is_connected) {
//        m_VideoStream.onOpen();
    }

    return m_sdl_is_connected;
}

void SDLConnector::DelConnectToVideoStream()
{
    m_Sockets.DelConnectToVS();
}

void SDLConnector::Connect()
{
    m_sdl_is_connected = m_Sockets.ConnectTo(m_channels, this);
    if (m_sdl_is_connected) {
        m_VR.onOpen();
        m_Vehicle.onOpen();
        m_UI.onOpen();
        m_TTS.onOpen();
        m_Navi.onOpen();
        m_Buttons.onOpen();
#ifdef WIN32
        Sleep(100);
#else
        usleep(100000);
#endif
        //BaseCommunication is the last Channel to register
        m_Base.onOpen();
    }
}

void* SDLConnector::ConnectThread(void* arg)
{
    SDLConnector * connector=(SDLConnector*)arg;
    if (connector==NULL) {
        return NULL;
    }

    while (true) {
        if (!connector->IsSDLConnected()) {
             connector->Connect();
        }
#ifdef WIN32
        Sleep(2000);
#else
        sleep(2);
#endif
    }
    return NULL;
}

void SDLConnector::ChangeMsgHandler(IMessageInterface * pMsgHandler)
{
    m_VR.SetCallback(pMsgHandler);
    m_Vehicle.SetCallback(pMsgHandler);
    m_UI.SetCallback(pMsgHandler);
    m_TTS.SetCallback(pMsgHandler);
    m_Navi.SetCallback(pMsgHandler);
    m_Buttons.SetCallback(pMsgHandler);
    m_Base.SetCallback(pMsgHandler);
}

void SDLConnector::OnAppActivated(int appID)
{
    Json::Value params;
    params["appID"] = appID;
    //m_Base.sendNotification("BasicCommunication.OnAppActivated", params);
    m_Base.sendRequest(m_Base.GenerateId(),"SDL.ActivateApp", params);
}

void SDLConnector::OnSoftButtonClick(int id, int mode,std::string strName)
{
    if (!strName.empty()) {
        _onButtonClickAction(strName, "BUTTONDOWN", id);
        _onButtonClickAction(strName, "BUTTONUP", id);
        if (mode == BUTTON_SHORT)
            _onButtonClickAction(strName, "SHORT", id);
        else
            _onButtonClickAction(strName, "LONG", id);
    } else {
        _onButtonClickAction("CUSTOM_BUTTON", "BUTTONDOWN", id);
        _onButtonClickAction("CUSTOM_BUTTON", "BUTTONUP", id);
        if (mode == BUTTON_SHORT)
            _onButtonClickAction("CUSTOM_BUTTON", "SHORT", id);
        else
            _onButtonClickAction("CUSTOM_BUTTON", "LONG", id);
    }
}

void SDLConnector::_onButtonClickAction(std::string name, std::string mode, int customButtonID)
{
    Json::Value params;
    params["name"] = name;
    params["mode"] = mode;
    params["customButtonID"] = customButtonID;

	m_Buttons.sendNotification("Buttons.OnButtonEvent", params);
}

void SDLConnector::OnAppExit(int appID)
{
    Json::Value params;
    params["reason"] = "USER_EXIT";
    params["appID"] = appID;
	m_Base.sendNotification("BasicCommunication.OnExitApplication", params);
}

void SDLConnector::OnAppOut(int appID)
{
    Json::Value params;
    params["appID"] = appID;
    params["reason"] = "GENERAL";
	m_Base.sendNotification("BasicCommunication.OnAppDeactivated",params);
}

// reason 0:timeout 1:aborted 2:clickSB
void SDLConnector::OnAlertResponse(int id, int reason)
{
    if (reason == RESULT_SUCCESS) {
        Json::Value result;
        result["code"] = 0;
        result["method"] = "UI.Alert";
		m_UI.sendResult(id,result);
    } else {
        Json::Value error;
        Json::Value data;
        data["method"] = "UI.Alert";
        error["message"] = "Alert request aborted";
        error["code"] = 4;
        error["data"] = data;
		m_UI.sendError(id, error);
    }
    m_UI.onSystemContext("MAIN");
}

void SDLConnector::OnMediaClockResponse(int id,int code)
{
    Json::Value result;
    result["code"] = code;
    result["method"] = "UI.SetMediaClockTimer";
    m_UI.sendResult(id, result);
}

void SDLConnector::OnScrollMessageResponse(int id, int reason)
{
    /*
    Json::Value root;
    if(reason == SCROLLMESSAGE_TIMEOUT || reason == SCROLLMESSAGE_CLICK_SOFTBUTTON) {
        Json::Value result;
        result["code"] = 0;
        result["method"] = "UI.ScrollableMessage";
		m_UI.sendResult(id, result);
    }
    else{
        Json::Value error;
        Json::Value data;
        data["method"] = "UI.ScrollableMessage";
        error["message"] = "REJECTED";
        error["code"] = 4;
        error["date"] = data;
		m_UI.sendError(id, error);
    }
    m_UI.onSystemContext("MAIN");
    */

    Json::Value result;
    result["code"] = reason;
    result["method"] = "UI.ScrollableMessage";
    m_UI.sendResult(id, result);
    m_UI.onSystemContext("MAIN");
}

void SDLConnector::OnCommandClick(int appID, int cmdID)
{
    Json::Value params;
    params["appID"] = appID;
    params["cmdID"] = cmdID;

	m_UI.sendNotification("UI.OnCommand", params);
}

void SDLConnector::OnPerformInteraction(int code, int performInteractionID, int choiceID)
{
    Json::Value result;
    result["code"] = code;
    result["method"] = "UI.PerformInteraction";
    if (0 == code) {
        result["choiceID"] = choiceID;
    }
	m_UI.sendResult(performInteractionID, result);
}

void SDLConnector::OnVRPerformInteraction(int code, int performInteractionID, int choiceID)
{
    Json::Value result;
    result["code"] = code;
    result["method"] = "VR.PerformInteraction";
    if (0 == code) {
        result["choiceID"] = choiceID;
    }
    m_VR.sendResult(performInteractionID, result);
}

void SDLConnector::OnVRCommand(int appID, int cmdID)
{
    Json::Value params;
    params["appID"] = appID;
    params["cmdID"] = cmdID;

	m_VR.sendNotification("VR.OnCommand", params);
}

void SDLConnector::OnSliderResponse(int code, int sliderid, int sliderPosition)
{
    std::string info_msg = "";
    if (code == SLIDER_OK) {
        Json::Value result;
        result["code"] = code;
        result["method"] = "UI.Slider";
        result["sliderPosition"] = sliderPosition;
        m_UI.sendResult(sliderid, result);
    } else {
        if (code == SLIDER_TIMEOUT) {
            info_msg = "Slider request timeout.";
        } else {
            info_msg = "Slider request aborted.";
        }
        Json::Value error;
        Json::Value data;
        data["method"] = "UI.Slider";
        error["code"] = code;
        error["message"] = info_msg;
        error["data"] = data;
   
		m_UI.sendError(sliderid, error);
    }
}

void SDLConnector::OnSetMediaClockTimerResponse(int iCode,int iRequestId)
{
    //std::string info_msg = "";
    if (iCode == RESULT_SUCCESS) {
        Json::Value result;
        result["code"] = iCode;
        result["method"] = "UI.SetMediaClockTimer";
        m_UI.sendResult(iRequestId, result);
    } else {
        Json::Value error;
        Json::Value data;
        data["method"] = "UI.SetMediaClockTimer";
        error["code"] = iCode;
        //error["message"] = info_msg;
        error["data"] = data;

        m_UI.sendError(iRequestId, error);
    }
}

void SDLConnector::OnStartDeviceDiscovery()
{
    Json::Value params;
    m_Base.sendNotification("BasicCommunication.OnStartDeviceDiscovery", params);
}

void SDLConnector::OnDeviceChosen(std::string name, std::string id)
{
    Json::Value params;
    if(!name.empty())
        params["name"] = name;
    if(!id.empty())
        params["id"] = id;
    m_Base.sendNotification("BasicCommunication.OnDeviceChosen", params);
}

void SDLConnector::OnFindApplications(std::string name, std::string id)
{
    Json::Value params;
    if(!name.empty())
        params["name"] = name;
    if(!id.empty())
        params["id"] = id;
    m_Base.sendNotification("BasicCommunication.OnFindApplications", params);
}

void SDLConnector::OnPerformAudioPassThru(int appID, int performaududiopassthruID, int code)
{
    _stopPerformAudioPassThru(appID);
    Json::Value root;
    if (code == 0) {
        Json::Value result;
        result["code"] = code;
        result["method"] = "UI.PerformAudioPassThru";
		m_UI.sendResult(performaududiopassthruID, result);
    }else if (code == 5) {
        Json::Value error;
        Json::Value data;

        data["method"] = "UI.PerformAudioPassThru";
        error["code"] = code;
        error["message"] = "PerformAudioPassThru was not completed successful!";
        error["data"] = data;
		m_UI.sendError(performaududiopassthruID, error);
    } else {
        Json::Value error;
        Json::Value data;

        data["method"] = "UI.PerformAudioPassThru";
        error["code"] = code;
        error["message"] = "PerformAudioPassThru was not completed successful!";
        error["data"] = data;
		m_UI.sendError(performaududiopassthruID, error);
    }
}

void SDLConnector::_stopPerformAudioPassThru(int appID)
{
    Json::Value params;

    params["appID"] = appID;

	m_UI.sendNotification("UI.PerformAudioPassThruStop", params);
}

void SDLConnector::OnButtonClick(std::string buttonname, int mode)
{
    _buttonEventDown(buttonname);
    _buttonPressed(buttonname, mode);
    _buttonEventUp(buttonname);
}

void SDLConnector::_buttonEventDown(std::string buttonname)
{
    Json::Value params;

    params["mode"] = "BUTTONDOWN";
    params["name"] = buttonname;

	m_UI.sendNotification("Buttons.OnButtonEvent", params);
}

void SDLConnector::_buttonPressed(std::string buttonname, int mode)
{
    Json::Value params;

    if (BUTTON_LONG == mode)
        params["mode"] = "LONG";
    else
        params["mode"] = "SHORT";
    params["name"] = buttonname;

	m_UI.sendNotification("Buttons.OnButtonEvent", params);
}

void SDLConnector::_buttonEventUp(std::string buttonname)
{
    Json::Value params;

    params["mode"] = "BUTTONUP";
    params["name"] = buttonname;

	m_UI.sendNotification("Buttons.OnButtonEvent", params);
}

void SDLConnector::OnTTSSpeek(int speekID, int code)
{
    if (code == SPEEK_OK) {
		Json::Value result;

        result["method"] = "TTS.Speak";
        result["code"] = code;
		m_TTS.sendResult(speekID, result);
    } else {
        Json::Value error;
        Json::Value data;

        data["method"] = "TTS.Speak";

        error["message"] = "Speech was interrupted";
        error["code"] = code;
        error["data"] = data;

		m_TTS.sendError(speekID, error);
    }
}

void SDLConnector::OnVRStartRecord()
{
	m_VR.sendNotification("VR.StartRecord");
}
void SDLConnector::OnVRCancelRecord()
{
	m_VR.sendNotification("VR.CancelRecord");
}

void SDLConnector::OnVideoScreenTouch(TOUCH_TYPE touch,int x,int y)
{
     Json::Value params;
     Json::Value coord;
     Json::Value event;
     Json::Value ts;
     static int id = 0;
//     [{"c":[{"x":103,"y":247}]

     switch(touch) {
     case TOUCH_START:
         id++;
         params["type"] = "BEGIN";
         event[0]["id"] = id;
         break;
     case TOUCH_END:
         params["type"] = "END";
         event[0]["id"] = id;
         break;
     case TOUCH_MOVE:
         params["type"] = "MOVE";
         event[0]["id"] = id;
         break;
     }
     coord[0]["x"] = x;
     coord[0]["y"] = y;
     event[0]["c"] = coord;

#ifdef WIN32
     SYSTEMTIME  time;
     GetSystemTime(&time);
     int t = (time.wHour*3600+time.wMinute*60+time.wSecond)%1000*1000+time.wMilliseconds;
#else
     timeval val;
     gettimeofday(&val,NULL);
     int t = (int)(val.tv_sec%1000*1000+val.tv_usec/1000);
#endif

     ts[0] = t;
     event[0]["ts"] = ts;
     params["event"] = event;

   //  std::cout<<root.asString();
	 m_UI.sendNotification("UI.OnTouchEvent", params);
}

//{
//    "jsonrpc":"2.0",
//    "method":"UI.OnTouchEvent",
//    "params":
//    {
//        "type":"END",
//        "event":[
//            {"c":[
//                {
//                    "x":441,
//                    "y":158
//                }
//                ],
//            "id":0,
//            "ts":[87214]
//            }
//            ]
//    }
//}
