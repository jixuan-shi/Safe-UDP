#include "udp_client.h"

#include <netdb.h>
#include <stdlib.h>

#include <fstream>
#include <iostream>
#include <sstream>

#include <glog/logging.h>

#include "data_segment.h"

namespace safe_udp {
UdpClient::UdpClient() {
  last_in_order_packet_ = -1;
  last_packet_received_ = -1;
  fin_flag_received_ = false;
}

void UdpClient::SendFileRequest(const std::string &file_name) {
  int n;
  int next_seq_expected;
  int segments_in_between = 0;
  initial_seq_number_ = 67;
  if (receiver_window_ == 0) {
    receiver_window_ = 100;
  }

  //环形队列初始化设置
  ring_buffer_capacity_ = receiver_window_ + 10;
  ring_buffer_.resize(ring_buffer_capacity_);

  for (int i = 0; i < ring_buffer_capacity_; ++i) {
      ring_buffer_[i].data_ = nullptr;
      ring_buffer_[i].seq_number_ = -1; 
  }

  ring_received_flags_.assign(ring_buffer_capacity_, false);


  unsigned char *buffer =
      (unsigned char *)calloc(MAX_PACKET_SIZE, sizeof(unsigned char));
  LOG(INFO) << "server_add::" << server_address_.sin_addr.s_addr;
  LOG(INFO) << "server_add_port::" << server_address_.sin_port;
  LOG(INFO) << "server_add_family::" << server_address_.sin_family;
  n = sendto(sockfd_, file_name.c_str(), file_name.size(), 0,
             (struct sockaddr *)&(server_address_), sizeof(struct sockaddr_in));
  if (n < 0) {
    LOG(ERROR) << "Failed to write to socket !!!";
  }
  memset(buffer, 0, MAX_PACKET_SIZE);

  std::fstream file;
  std::string file_path = std::string(CLIENT_FILE_PATH) + file_name;
  file.open(file_path.c_str(), std::ios::out);

  while ((n = recvfrom(sockfd_, buffer, MAX_PACKET_SIZE, 0, NULL, NULL)) > 0) {
    char buffer2[20];
    memcpy(buffer2, buffer, 20);
    if (strstr(buffer2, "FILE NOT FOUND") != NULL) {
      LOG(ERROR) << "File not found !!!";
      free(buffer);
      return;
    }

    std::unique_ptr<DataSegment> data_segment = std::make_unique<DataSegment>();
    data_segment->DeserializeToDataSegment(buffer, n);

    LOG(INFO) << "packet received with seq_number_:"
              << data_segment->seq_number_;

    // Random drop
    if (is_packet_drop_ && rand() % 100 < prob_value_) {
      LOG(INFO) << "Dropping this packet with seq "
                << data_segment->seq_number_;
      if(data_segment->data_ != nullptr){
        free(data_segment->data_);
        data_segment->data_ = nullptr;
      }
      continue;
    }

    // Random delay
    if (is_delay_ && rand() % 100 < prob_value_) {
      int sleep_time = (rand() % 10) * 1000;
      LOG(INFO) << "Delaying this packet with seq " << data_segment->seq_number_
                << " for " << sleep_time << "us";
      usleep(sleep_time);
    }

    if (last_in_order_packet_ == -1) {
      next_seq_expected = initial_seq_number_;
    } else {
      next_seq_expected = ring_buffer_[last_in_order_packet_ % ring_buffer_capacity_].seq_number_ +
                          ring_buffer_[last_in_order_packet_ % ring_buffer_capacity_].length_;
    }

    // Old packet
    if (next_seq_expected > data_segment->seq_number_ &&
        !data_segment->fin_flag_) {
      send_ack(next_seq_expected);
      if(data_segment->data_ != nullptr){
        free(data_segment->data_);
        data_segment->data_ = nullptr;
      }
      continue;
    }

    segments_in_between =
        (data_segment->seq_number_ - next_seq_expected) / MAX_DATA_SIZE;
    int this_segment_index = last_in_order_packet_ + segments_in_between + 1;

    if (this_segment_index - last_in_order_packet_ > receiver_window_) {
      LOG(INFO) << "Packet dropped " << this_segment_index;
      // Drop the packet, if it exceeds receiver window
      if(data_segment->data_ != nullptr){
        free(data_segment->data_);
        data_segment->data_ = nullptr;
      }
      continue;
    }

    if (data_segment->fin_flag_) {
      LOG(INFO) << "Fin flag received !!!";
      fin_flag_received_ = true;
    }

    insert(this_segment_index, *data_segment);

    // for (int i = last_in_order_packet_ + 1; i <= last_packet_received_; i++) {
    //   if (data_segments_[i].seq_number_ != -1) {
    //     if (file.is_open()) {
    //       file << data_segments_[i].data_;
    //       last_in_order_packet_ = i;
    //     }
    //   } else {
    //     break;
    //   }
    // }

    //存入文件当中
    while(true){
      int next_check_index = last_in_order_packet_ + 1;
      int circular_idx = next_check_index % ring_buffer_capacity_;

      if(ring_received_flags_[circular_idx] == true){
        if(ring_buffer_[circular_idx].seq_number_ != -1){
          if(file.is_open()){
            file << ring_buffer_[circular_idx].data_;
          }

          if(ring_buffer_[circular_idx].data_ != nullptr){
            free(ring_buffer_[circular_idx].data_);
            ring_buffer_[circular_idx].data_ = nullptr;
          }

          ring_received_flags_[circular_idx] = false;

          //更新最后一个放好的buffer，滑动窗口
          last_in_order_packet_ = next_check_index;
        }else{
          break;
        }
      }else{
        break;
      }
    }


    // 如果已经接收到 fin_flag_ 且所有数据包都处理完毕，则跳出循环
    if (fin_flag_received_ && last_in_order_packet_ == last_packet_received_) {
      break;
    }

    int ack_val;
    if(last_in_order_packet_ == -1){
      ack_val = initial_seq_number_;
    }else{
      int idx = last_in_order_packet_ % ring_buffer_capacity_;
      ack_val = ring_buffer_[idx].seq_number_ + ring_buffer_[idx].length_;
    }

    send_ack(ack_val);

    memset(buffer, 0, MAX_PACKET_SIZE);
  }

  free(buffer);
  file.close();
}

// int UdpClient::add_to_data_segment_vector(const DataSegment &data_segment) {
//   data_segments_.push_back(data_segment);
//   return data_segments_.size() - 1;
// }

void UdpClient::send_ack(int ackNumber) {
  LOG(INFO) << "Sending an ack :" << ackNumber;
  int n = 0;
  DataSegment ack_segment;
  ack_segment.ack_flag_ = true;
  ack_segment.ack_number_ = ackNumber;
  ack_segment.fin_flag_ = false;
  ack_segment.length_ = 0;
  ack_segment.seq_number_ = 0;
  ack_segment.data_ = nullptr;

  char *data = ack_segment.SerializeToCharArray();
  n = sendto(sockfd_, data, MAX_PACKET_SIZE, 0,
             (struct sockaddr *)&(server_address_), sizeof(struct sockaddr_in));
  if (n < 0) {
    LOG(INFO) << "Sending ack failed !!!";
  }
}

void UdpClient::CreateSocketAndServerConnection(
    const std::string &server_address, const std::string &port) {
  struct hostent *server;
  struct sockaddr_in server_address_;
  int sfd;
  int port_num;
  port_num = atoi(port.c_str());
  sfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sfd < 0) {
    LOG(ERROR) << "Failed to socket !!!";
  }
  server = gethostbyname(server_address.c_str());
  if (server == NULL) {
    LOG(ERROR) << "No such host !!!";
    exit(0);
  }

  memset(&server_address_, 0, sizeof(server_address_));
  server_address_.sin_family = AF_INET;
  memcpy(&server_address_.sin_addr.s_addr, server->h_addr, server->h_length);
  server_address_.sin_port = htons(port_num);

  sockfd_ = sfd;
  this->server_address_ = server_address_;
}

void UdpClient::insert(int index, DataSegment &data_segment) {
  // if (index > last_packet_received_) {
  //   for (int i = last_packet_received_ + 1; i <= index; i++) {
  //     if (i == index) {
  //       data_segments_.push_back(data_segment);
  //     } else {
  //       DataSegment data_segment;
  //       data_segments_.push_back(data_segment);
  //     }
  //   }
  //   last_packet_received_ = index;
  // } else {
  //   data_segments_[index] = data_segment;
  // }

  int circular_index = index % ring_buffer_capacity_;
  if(ring_received_flags_[circular_index] == true){
    if(ring_buffer_[circular_index].data_ != nullptr){
      free(ring_buffer_[circular_index].data_);
      ring_buffer_[circular_index].data_ = nullptr;
    }
  }

  ring_buffer_[circular_index] = data_segment;
  data_segment.data_ = nullptr;
  ring_received_flags_[circular_index] = true;

  if(index > last_packet_received_){
    last_packet_received_ = index;
  }
}
}  // namespace safe_udp
