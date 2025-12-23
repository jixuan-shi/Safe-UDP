#pragma once

#include <sys/time.h>
#include <time.h>

namespace safe_udp {
class SlidWinBuffer {
 public:
  SlidWinBuffer() {}
  ~SlidWinBuffer() {}

  int first_byte_;  //该buffer的第一个字节索引值
  int data_length_; //该buffer数据大小
  int seq_num_;     //该buffer序列号
  struct timeval time_sent_;  //该buffer 发送时间戳，为了记录超时重传时间
};
}  // namespace safe_udp