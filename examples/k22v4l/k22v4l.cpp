/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2011 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */


#include <iostream>
#include <signal.h>

#include <opencv2/opencv.hpp>

#include <libfreenect2/libfreenect2.hpp>
#include <libfreenect2/frame_listener_impl.h>
#include <libfreenect2/threading.h>

#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define VIDEO_DEVICE_RGB "/dev/video0"
#define VIDEO_DEVICE_IR "/dev/video1"
#define VIDEO_DEVICE_DEPTH "/dev/video2"

bool k22v4l_shutdown = false;

void sigint_handler(int s)
{
  k22v4l_shutdown = true;
}

int main(int argc, char *argv[])
{
  std::string program_path(argv[0]);
  size_t executable_name_idx = program_path.rfind("k22v4l");

  std::string binpath = "/";

  if(executable_name_idx != std::string::npos)
  {
    binpath = program_path.substr(0, executable_name_idx);
  }


  libfreenect2::Freenect2 freenect2;
  libfreenect2::Freenect2Device *dev = freenect2.openDefaultDevice();

  if(dev == 0)
  {
    std::cout << "no device connected or failure opening the default one!" << std::endl;
    return -1;
  }

  signal(SIGINT,sigint_handler);
  k22v4l_shutdown = false;

  libfreenect2::SyncMultiFrameListener listener(libfreenect2::Frame::Color | libfreenect2::Frame::Ir | libfreenect2::Frame::Depth);
  libfreenect2::FrameMap frames;

  dev->setColorFrameListener(&listener);
  dev->setIrAndDepthFrameListener(&listener);
  dev->start();

  std::cout << "device serial: " << dev->getSerialNumber() << std::endl;
  std::cout << "device firmware: " << dev->getFirmwareVersion() << std::endl;

  // don't do that at home, one int to retrieve various return code
  int ret_code;

  // init V4L loopback output for RGB
  int fd_rgb = open(VIDEO_DEVICE_RGB, O_RDWR);
  assert(fd_rgb >= 0);
  // what should handle
  struct v4l2_capability vid_caps;
  ret_code = ioctl(fd_rgb, VIDIOC_QUERYCAP, &vid_caps);
  assert(ret_code != -1);

  // init V4L loopback output for IR
  int fd_ir = open(VIDEO_DEVICE_IR, O_RDWR);
  assert(fd_ir >= 0);
  ret_code = ioctl(fd_ir, VIDIOC_QUERYCAP, &vid_caps);
  assert(ret_code != -1);

  // init V4L loopback output for DEPTH
  int fd_depth = open(VIDEO_DEVICE_DEPTH, O_RDWR);
  assert(fd_depth >= 0);
  ret_code = ioctl(fd_depth, VIDIOC_QUERYCAP, &vid_caps);
  assert(ret_code != -1);

  std::cout << "init feeds" << std::endl;
  // first run to get pointer to frame
  listener.waitForNewFrame(frames);
  libfreenect2::Frame *rgb = frames[libfreenect2::Frame::Color];
  libfreenect2::Frame *ir = frames[libfreenect2::Frame::Ir];
  libfreenect2::Frame *depth = frames[libfreenect2::Frame::Depth];

  std::cout << "RGB BPP: " << rgb->bytes_per_pixel << std::endl;
  std::cout << "IR BPP: " << ir->bytes_per_pixel << std::endl;
  std::cout << "depth BPP: " << depth->bytes_per_pixel << std::endl;


  // configure for our RBG device
  std::cout << "configure v4l loopback for RGB" << std::endl;
  struct v4l2_format vid_format_rgb;
  vid_format_rgb.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  vid_format_rgb.fmt.pix.width = rgb->width;
  vid_format_rgb.fmt.pix.height = rgb->height;
  vid_format_rgb.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24;
  vid_format_rgb.fmt.pix.sizeimage = rgb->width * rgb->height * 3;
  vid_format_rgb.fmt.pix.field = V4L2_FIELD_NONE;
  vid_format_rgb.fmt.pix.bytesperline = rgb->width * 3;
  vid_format_rgb.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
  ret_code = ioctl(fd_rgb, VIDIOC_S_FMT, &vid_format_rgb);
  assert(ret_code != -1);

  // configure for our IR device
  std::cout << "configure v4l loopback for IR" << std::endl;
  struct v4l2_format vid_format_ir;
  vid_format_ir.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  vid_format_ir.fmt.pix.width = ir->width;
  vid_format_ir.fmt.pix.height = ir->height;
  vid_format_ir.fmt.pix.pixelformat = V4L2_PIX_FMT_Y16; // black and white 16bits
  vid_format_ir.fmt.pix.sizeimage = ir->width * ir->height * 2;
  vid_format_ir.fmt.pix.field = V4L2_FIELD_NONE;
  vid_format_ir.fmt.pix.bytesperline = rgb->width * 2;
  vid_format_ir.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
  ret_code = ioctl(fd_ir, VIDIOC_S_FMT, &vid_format_ir);
  assert(ret_code != -1);

  // configure for our DEPTH device
  std::cout << "configure v4l loopback for DEPTH" << std::endl;
  struct v4l2_format vid_format_depth;
  vid_format_depth.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
  vid_format_depth.fmt.pix.width = depth->width;
  vid_format_depth.fmt.pix.height = depth->height;
  // vid_format_depth.fmt.pix.pixelformat = V4L2_PIX_FMT_GREY; // black and white 8bits
  //vid_format_depth.fmt.pix.pixelformat = V4L2_PIX_FMT_Y16; // black and white 16bits
  vid_format_depth.fmt.pix.pixelformat = V4L2_PIX_FMT_BGR24; // float will go on bgr
  vid_format_depth.fmt.pix.sizeimage = depth->width * depth->height * 3;
  vid_format_depth.fmt.pix.field = V4L2_FIELD_NONE;
  // vid_format_depth.fmt.pix.bytesperline = depth->width * ;
  vid_format_depth.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;
  ret_code = ioctl(fd_depth, VIDIOC_S_FMT, &vid_format_depth);
  assert(ret_code != -1);

  std::cout << "start loop" << std::endl;

  // use opencv to confert pixel format (float to int), need some matrix
  cv::Mat mat_ir, tmp_ir;
  cv::Mat mat_depth, tmp_depth;

  // dummy image for dummy frame
  cv::Mat mat_dummy(240,320, CV_8UC1,128);

  while(!k22v4l_shutdown)
  {

    listener.waitForNewFrame(frames);
    rgb = frames[libfreenect2::Frame::Color];
    ir = frames[libfreenect2::Frame::Ir];
    depth = frames[libfreenect2::Frame::Depth];


    cv::Mat mat_rgb = cv::Mat(rgb->height, rgb->width, CV_8UC3, rgb->data);
    cv::flip(mat_rgb, mat_rgb, 1);

    ret_code = write(fd_rgb, mat_rgb.data, rgb->width * rgb->height * 3);

    // IR: use a matrix to convert from float32 to int16 and normalize data to adjust brightness
    mat_ir = cv::Mat(ir->height, ir->width, CV_32FC1, ir->data);
    cv::Mat ir16;
    mat_ir.convertTo(ir16, CV_16UC1);
    ret_code = write(fd_ir, ir16.data, ir->width * ir->height * 2);

    //cv::normalize(mat_ir, tmp_ir, 0, 255*255, cv::NORM_MINMAX, CV_16UC1); // 255*255 for color depth...
    // ret_code = write(fd_ir, tmp_ir.data, ir->width * ir->height * 2);

    // same with depth
    mat_depth = cv::Mat(depth->height, depth->width, CV_32FC1, depth->data) ;

    // cv::normalize(mat_depth, tmp_depth, 0, 255, cv::NORM_MINMAX, CV_8UC1);
    // ret_code = write(fd_depth, tmp_depth.data, depth->width * depth->height );
    // mat_depth = cv::Mat(depth->height, depth->width, CV_32FC1, depth->data) ;

    std::cout << mat_depth.at<float>(100, 100) << std::endl;

    mat_depth = mat_depth * 10;

    // cv::normalize(mat_depth, tmp_depth, 0, 255*255, cv::NORM_MINMAX, CV_16UC1);
    cv::Mat depth16, depth8, fin_image;
    mat_depth.convertTo(depth16, CV_16UC1);

    // std::cout << "value " <<  +depth16.at<uint16_t>(100, 100) << std::endl;

    cv::Mat low_byte, high_byte, low_byte8, high_byte8;

    cv::bitwise_and(0x000000FF, depth16, low_byte);
    cv::bitwise_and(0x0000FF00, depth16, high_byte);

    // low_byte = cv::Mat(depth->height, depth->width, CV_8UC1);
    // high_byte = cv::Mat(depth->height, depth->width, CV_8UC1);

    high_byte= high_byte / (1 << 8);

    // std::cout << "div " << +low_byte.at<uint16_t>(100, 100) << std::endl;
    // std::cout << "div2 " << +high_byte.at<uint16_t>(100, 100) << std::endl;

    low_byte.convertTo(low_byte8, CV_8UC1);
    high_byte.convertTo(high_byte8, CV_8UC1);

    cv::flip(low_byte8, low_byte8, 1);
    cv::flip(high_byte8, high_byte8, 1);

    // std::cout << "8 div " << +low_byte8.at<uint8_t>(100, 100) << std::endl
    // std::cout << "8 div2 " << +high_byte8.at<uint8_t>(100, 100) << std::en
    // add a third channel, filled with zeros.eam...
    fin_image = cv::Mat(depth->height, depth->width, CV_8UC3);

    std::vector<cv::Mat> fin_vector;

    cv::Mat zero = cv::Mat::zeros(depth->height, depth->width, CV_8UC1);
    zero = zero + 255;

    fin_vector.push_back(low_byte8);
    fin_vector.push_back(high_byte8);
    fin_vector.push_back(zero);

    // depth_split.push_back(zero);
    // depth_split.push_back(zero);
//    cv::merge(depth_split, fin_image);

    cv::merge(fin_vector, fin_image);

    //ret_code = write(fd_depth, depth16.data, depth->width * depth->height * 3);
    ret_code = write(fd_depth, fin_image.data, depth->width * depth->height * 3);

    // one hell of a GUI
    cv::imshow("esc to quit", mat_dummy);

    int key = cv::waitKey(1);
    k22v4l_shutdown = k22v4l_shutdown || (key > 0 && ((key & 0xFF) == 27)); // shutdown on escape

    listener.release(frames);
    //libfreenect2::this_thread::sleep_for(libfreenect2::chrono::milliseconds(100));
  }

  // TODO: restarting ir stream doesn't work!
  // TODO: bad things will happen, if frame listeners are freed before dev->stop() :(
  dev->stop();
  dev->close();

  close(fd_rgb);

  return 0;
}