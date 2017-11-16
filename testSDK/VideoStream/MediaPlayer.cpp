#include <stdio.h>
#include "MediaPlayer.h"

MediaPlayer::MediaPlayer(QObject *parent) : QThread(parent),
    pipe_name_(""),
    sink_name_(""),
    sync_flag_(false),
    xwinid_(0),
    state_(GST_STATE_VOID_PENDING),
    msg_bus_(NULL),
    pipeline_(NULL),
    videosink_(NULL),
    mainloop_(NULL),
    isActive_(false) {

}

MediaPlayer::~MediaPlayer() {

}

bool MediaPlayer::start() {
    printf("-- MediaPlayer::start\n");
    if (pipe_name_.empty()) {
        return false;
    }
    if (sink_name_.empty()) {
        return false;
    }
    QThread::start();

    isActive_ = true;

    return true;
}

bool MediaPlayer::stop() {
    printf("-- MediaPlayer::stop\n");
//    gst_element_set_state(pipeline_, GST_STATE_READY);
    g_main_loop_quit(mainloop_);

    isActive_ = false;

    return true;
}

void MediaPlayer::setPipeName(std::string pipe_name) {
    printf("-- MediaPlayer::setPipeName = %s\n", pipe_name.c_str());
    pipe_name_ = pipe_name;
}

void MediaPlayer::setVideoSink(std::string sink_name) {
    printf("-- MediaPlayer::setVideoSink = %s\n", sink_name.c_str());
    sink_name_ = sink_name;
}

void MediaPlayer::setSyncFlag(gboolean sync_flag) {
    printf("-- MediaPlayer::setSyncFlag = %s\n", sync_flag ? "true" : "false");
    sync_flag_ = sync_flag;
}

void MediaPlayer::setWindowHandle(guintptr xwinid) {
    printf("-- MediaPlayer::setWindowHandle = %zu\n", xwinid);
    xwinid_ = xwinid;
}

bool MediaPlayer::isActive() {
    return isActive_;
}

void MediaPlayer::run() {
    std::string description;

    printf("-- GST:: thread entry.\n");

    // Initialize gstreamer
    gst_init(NULL, NULL);

    // Create pipeline
    description = std::string("filesrc location=") + pipe_name_ + std::string(" ! decodebin ! videoconvert ! ") + sink_name_ + std::string(" name=videosink");
    if (!sync_flag_) {
      description += std::string(" sync=false");
    }
    pipeline_ = gst_parse_launch(description.c_str(), NULL);

    // Get bus
    msg_bus_ = gst_element_get_bus(pipeline_);

    // Get sink to set display window
    videosink_ = gst_bin_get_by_name(GST_BIN (pipeline_), "videosink");
    if (videosink_ && xwinid_) {
      printf("-- GST: Set overlay, wID = %zu\n", xwinid_);
      gst_video_overlay_set_window_handle(GST_VIDEO_OVERLAY(videosink_), xwinid_);
    }

    // Add watch
    gst_bus_add_signal_watch(msg_bus_);

    // Connect signal
    g_signal_connect(msg_bus_, "message", G_CALLBACK(bus_callback), this);

    // Set pipeline ready to play.
    gst_element_set_state(pipeline_, GST_STATE_PLAYING);

    // Create a GLib Main Loop and set it to run
    mainloop_ = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(mainloop_);

    // Free resources
    g_main_loop_unref(mainloop_);
    gst_element_set_state (pipeline_, GST_STATE_NULL);
    if (videosink_) {
        gst_object_unref(videosink_);
    }
    gst_object_unref(pipeline_);
    printf("-- GST:: thread exit.\n");
}

gboolean MediaPlayer::bus_callback(GstBus* bus, GstMessage* msg, gpointer data) {
  MediaPlayer* media = (MediaPlayer*)data;

  bus = bus; // Avoid warning: -Wunused-parameter
  printf("-- MSG: GST_MESSAGE_TYPE = %d\n", GST_MESSAGE_TYPE (msg));
  switch (GST_MESSAGE_TYPE (msg)) {
  case GST_MESSAGE_ERROR: {
    printf("-- MSG: ERROR\n");
    gst_element_set_state(media->pipeline_, GST_STATE_READY);
    g_main_loop_quit(media->mainloop_);
    break;
  }
  case GST_MESSAGE_EOS:
    printf("-- MSG: EOS\n");
    gst_element_set_state(media->pipeline_, GST_STATE_READY);
    g_main_loop_quit(media->mainloop_);
    break;
  case GST_MESSAGE_CLOCK_LOST:
    // Get a new clock
    printf("-- MSG: GST_MESSAGE_CLOCK_LOST\n");
    gst_element_set_state (media->pipeline_, GST_STATE_PAUSED);
    gst_element_set_state (media->pipeline_, GST_STATE_PLAYING);
  default:
    // Unhandled message
    break;
    }
  return true;
}
