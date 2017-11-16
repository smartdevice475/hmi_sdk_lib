#ifndef MEDIAPLAYER_H
#define MEDIAPLAYER_H

#include <QThread>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

class MediaPlayer : public QThread
{
    Q_OBJECT
public:
    explicit MediaPlayer(QObject *parent = 0);
    ~MediaPlayer();

    bool start();
    bool stop();

    void setPipeName(std::string pipe_name);
    void setVideoSink(std::string sink_name);
    void setSyncFlag(gboolean sync_flag);
    void setWindowHandle(guintptr xwinid);
signals:

public slots:

private:
    std::string  pipe_name_;
    std::string  sink_name_;
    gboolean sync_flag_;
    guintptr xwinid_;
    GstState state_;

    GstBus*     msg_bus_;
    GstElement* pipeline_;
    GstElement* videosink_;
    GMainLoop*  mainloop_;

    void run();

    static gboolean bus_callback(GstBus* bus, GstMessage* msg, gpointer data);
};

#endif // MEDIAPLAYER_H
