#include <webstreamer/webstreamer.h>
#include <iostream>
using namespace cv;

int main(){
  std::cout << "START" << std::endl;
  std::cout << VERSION << std::endl;
  /* Initialize stream */
  awiros_stream webstream(8081, "localhost");

  VideoCapture cap(0); // open the default camera
  if(!cap.isOpened())  // check if we succeeded
    std::cout << "unable to open cam" << std::endl;
    return -1;

  while(cap.isOpened())
  {
    /* capture fresh frame*/
    Mat frame;
    cap >> frame;
    
    /* publish frames */
    webstream.publish_frame(frame);
  }
}
