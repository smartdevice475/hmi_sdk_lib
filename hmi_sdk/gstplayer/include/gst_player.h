#ifndef SRC_GST_PLAYER_H
#define SRC_GST_PLAYER_H

#include <gst/gst.h>
#include <string>

enum MediaState
{
    STATE_NULL,
    STATE_READY,
    STATE_PLAYING,
    STATE_PAUSED,
};

class GstPlayer {
public:
    GstPlayer(const std::string& file_path, bool sync = true);
    ~GstPlayer();
    
    bool play();
    bool pause();
    bool stop();
    MediaState get_state();
private:
    GMainLoop*  main_loop_;
    GstElement* pipeline_;
    GstBus* bus_;
  
    double volume_;
    MediaState state_;
    bool sync_;
    std::string file_path_;
    
    bool Init();
    bool Release();
    static gboolean bus_callback(GstBus* bus, GstMessage* msg, gpointer data);
};

#endif // SRC_GST_PLAYER_H
