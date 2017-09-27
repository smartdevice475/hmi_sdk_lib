#include <stdio.h>
#include "gst_player.h"
#include "gst/video/videooverlay.h"

GstPlayer::GstPlayer()
    : state_(STATE_NULL) {
}

GstPlayer::GstPlayer(const std::string& file_path, const std::string& sink, bool sync, guintptr xwinid)
    : file_path_(file_path)
    , sink_(sink)
    , sync_(sync)
    , xwinid_(xwinid)
    , state_(STATE_NULL) {
    Init();
}

GstPlayer::~GstPlayer() {
    if(state_ != STATE_READY) {
        stop();
    }

    Release();
}

bool GstPlayer::open(const std::string& file_path, const std::string& sink, bool sync, guintptr xwinid) {
    
    if (state_ != STATE_NULL) return false;
    
    file_path_ = file_path;
    sink_ = sink;
    sync_ = sync;
    xwinid_ = xwinid;
    
    return Init();
}

bool GstPlayer::play() {
    if (state_ == STATE_PLAYING) {
        return true;
    }
    else if (state_ == STATE_NULL) {
        Init();
    }
    
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if(GST_STATE_CHANGE_FAILURE != ret) {
        printf("-- GST: playing.\n");
        state_ = STATE_PLAYING;
        return true;
    }
    else {
        printf("-- GST: Failed to play.\n");
        Release();
    }
  
    return false;
}

bool GstPlayer::pause() {
    if (state_ != STATE_PLAYING) {
        printf("-- GST: Is not playing, can not pause.\n");
        return false;
    }
    
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PAUSED);
    if (GST_STATE_CHANGE_FAILURE != ret) {
        printf("-- GST: paused.\n");
        state_ = STATE_PAUSED;
        return true;
    }
    else {
        printf("-- GST: Failed to pause.\n");
    }
    
    return false;
}

bool GstPlayer::stop() {
    if (state_ == STATE_NULL) {
        return true;
    }
    
    return Release();
}

MediaState GstPlayer::get_state() {
    return state_;
}

bool GstPlayer::Init() {
    // Initialize gstreamer
    gst_init(NULL, NULL);
    
    // Create pipeline
    pipeline_ = gst_pipeline_new ("streaming");

    // Create element
    GstElement* ele_filesrc = gst_element_factory_make("filesrc", "file_source");
    GstElement* ele_decodebin = gst_element_factory_make("decodebin", "decode_bin");
    GstElement* ele_videoconvert = gst_element_factory_make("videoconvert", "video_convert");
    GstElement* ele_videosink = gst_element_factory_make(sink_.c_str(), "video_sink");
    if (!ele_filesrc || !ele_decodebin || !ele_videoconvert || !ele_videosink) {
        printf("-- GST: Failed to create element.\n");
        return false;
    }

    // Set filesrc location
    printf("-- GST: location = %s\n", file_path_.c_str());
    g_object_set(G_OBJECT(ele_filesrc), "location", file_path_.c_str(), NULL);

    // Add element to pipeline
    gst_bin_add_many(GST_BIN(pipeline_), ele_filesrc, ele_decodebin, ele_videoconvert, ele_videosink, NULL);

    // Link all element in order
    gst_element_link_many(ele_filesrc, ele_decodebin, ele_videoconvert, ele_videosink, NULL);

    // Set overlay
    if (ele_videosink && xwinid_) {
        gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (ele_videosink), xwinid_);
    }

    // Get bus
    bus_ = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));

    // Add bus to watch
    gst_bus_add_signal_watch (bus_);

    // Connect signal
    g_signal_connect(bus_, "message", G_CALLBACK(bus_callback), this);

    // Set pipeline ready to play.
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_READY);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        printf("-- GST: Failed to set state ready.\n");
        Release();
        return false;
    }

    state_ = STATE_READY;
    
    return true;
}

bool GstPlayer::Release() {
    if (state_ == STATE_NULL) {
        return true;
    }
    if (main_loop_) {
        g_main_loop_unref (main_loop_);
    }
    if (bus_) {
        gst_object_unref (bus_);
    }
    if (pipeline_) {
        gst_element_set_state (pipeline_, GST_STATE_NULL);
        gst_object_unref (pipeline_);
    }
    
    state_ = STATE_NULL;
    
    return true;
}

gboolean GstPlayer::bus_callback(GstBus* bus, GstMessage* msg, gpointer data) {
    GstPlayer* media = (GstPlayer*)data;

    switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR: {
        printf("-- MSG: ERROR\n");
        media->stop();
        break;
    }
    case GST_MESSAGE_EOS:
        printf("-- MSG: EOS\n");
        media->stop();
        break;
    default:
        /* Unhandled message */
        break;
    }
    return true;
}
