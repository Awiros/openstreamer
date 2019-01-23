#ifndef _AWIROS_WEB_STREAM_H
#define _AWIROS_WEB_STREAM_H

#include "streameye.h"

#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>


class awiros_stream {
  private:
    int                 m_port=8111;
    bool                m_status=false;
    seye_srv_t          m_seye;
    std::string         m_srv_add;
    std::vector<uchar>  m_imgbuf;
    bool                m_frame_ready=false;

  public:
    //awiros_stream(){};

    awiros_stream(int port, std::string srv_add);

    void init(int port, std::string srv_add);
    void set_port(int port);
    bool get_state();

    void publish_frame(const cv::Mat&);
    void kill();
};

static const float VERSION = 0.1;

#endif// _AWIROS_WEB_STREAM_H

