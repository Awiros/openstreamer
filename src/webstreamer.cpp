#include <webstreamer/webstreamer.h>
#include <fstream>
#include <thread>
#include <mutex>

awiros_stream::awiros_stream(int port, std::string srv_add){
  this->init(port, srv_add);
}

void awiros_stream::init(int port, std::string srv_add){
  if (pthread_cond_init(&(this->m_seye.frame_cond), NULL)) {
    std::cerr << "pthread_cond_init() failed frame_cond" << std::endl;
    return ;
  }
  if (pthread_mutex_init(&(this->m_seye.frame_mutex), NULL)) {
    std::cerr << "pthread_mutex_init() failed frame_muted" << std::endl;
    return ;
  }

  m_port = port;
  m_status = true;
  m_srv_add.assign(srv_add);
  m_seye.webport = m_port;
  m_imgbuf.reserve(1024*1024);
  reinterpret_cast<char*>(m_imgbuf.data());
  m_seye.pimgbuf = (char*)(&m_imgbuf[0]);
  std::cout << "Web Streaming Initialized. PORT: " <<  m_port << std::endl;
  std::thread stream_thread(streameye_thread, &m_seye);
  stream_thread.detach();
} 

void awiros_stream::set_port(int port) {m_port = port;}
bool awiros_stream::get_state() {return m_status;}


void save_buf_to_file(char *buf, int port, long int cycles, int nbytes){
  std::stringstream ss;
  ss << "imgs/" << port  << "_" << cycles << ".jpg"; 
  std::ofstream stream;

  stream.open(ss.str(), std::ios::binary | std::ios::out);
  if( !stream )
    std::cout << "Opening file failed" << std::endl;
  stream.write(buf, nbytes);

  // use operator<< for clarity
  //stream << m_seye.pimgbuf ;
  // test if write was succesful - not *really* necessary
  if( !stream )
    std::cout << "Write failed" << std::endl;

}

void awiros_stream::publish_frame(const cv::Mat& frame) {
  if (frame.empty()){
    m_seye.bufsize = 0;
    return;
  }
  cv::imencode(".jpg", frame, m_imgbuf);
  m_seye.pimgbuf = (char*)(&m_imgbuf[0]);
  m_seye.bufsize = m_imgbuf.size();

  if (pthread_mutex_lock(&(this->m_seye.frame_mutex))){
    std::cerr << "pthread_mutex_lock() failed frame_muted" << std::endl;
    return;
  }
  m_seye.ready_state = true;
  if (pthread_cond_broadcast(&(this->m_seye.frame_cond))){
    std::cerr << "pthread_cond_broadcast() failed frame_muted" << std::endl;
    return;
  }
  if (pthread_mutex_unlock(&(this->m_seye.frame_mutex))){
    std::cerr << "pthread_mutex_lock() failed frame_muted" << std::endl;
    return;
  }

}

void awiros_stream::kill(){

}

