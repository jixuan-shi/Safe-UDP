#include "data_segment.h"
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <string>
#include <arpa/inet.h>

namespace safe_udp {
DataSegment::DataSegment() {
  ack_number_ = -1;
  seq_number_ = -1;
  length_ = -1;
}

char *DataSegment::SerializeToCharArray() {
  if (final_packet_ != nullptr) {
    memset(final_packet_, 0, MAX_PACKET_SIZE);
  } else {
    final_packet_ =
        reinterpret_cast<char *>(calloc(MAX_PACKET_SIZE, sizeof(char)));
    if (final_packet_ == nullptr) {
      return nullptr;
    }
  }
  
  int seq_num = htonl(seq_number_);
  memcpy(final_packet_, &seq_num, sizeof(seq_number_));

  int ack_num = htonl(ack_number_);
  memcpy(final_packet_ + 4, &ack_num, sizeof(ack_number_));

  memcpy((final_packet_ + 8), &ack_flag_, 1);

  memcpy((final_packet_ + 9), &fin_flag_, 1);

  uint16_t len = htons(length_);
  memcpy((final_packet_ + 10), &len, sizeof(length_));

  memcpy((final_packet_ + 12), data_, length_);
  return final_packet_;
}

void DataSegment::DeserializeToDataSegment(unsigned char *data_segment,
                                           int length) {
  seq_number_ = convert_to_uint32(data_segment, 0);
  ack_number_ = convert_to_uint32(data_segment, 4);
  ack_flag_ = convert_to_bool(data_segment, 8);
  fin_flag_ = convert_to_bool(data_segment, 9);
  length_ = convert_to_uint16(data_segment, 10);

  data_ = reinterpret_cast<char *>(calloc(length + 1, sizeof(char)));
  if (data_ == nullptr) {
    return;
  }
  memcpy(data_, data_segment + HEADER_LENGTH, length);
  *(data_ + length) = '\0';
}

uint32_t DataSegment::convert_to_uint32(unsigned char *buffer,
                                        int start_index) {
  uint32_t uint32_value =
      (buffer[start_index] << 24) | (buffer[start_index + 1] << 16) |
      (buffer[start_index + 2] << 8) | (buffer[start_index + 3]);
  return uint32_value;
}

uint16_t DataSegment::convert_to_uint16(unsigned char *buffer,
                                        int start_index) {
  uint16_t uint16_value =
      (buffer[start_index] << 8) | (buffer[start_index + 1]);
  return uint16_value;
}

bool DataSegment::convert_to_bool(unsigned char *buffer, int index) {
  bool bool_value = buffer[index];
  return bool_value;
}
}  // namespace safe_udp