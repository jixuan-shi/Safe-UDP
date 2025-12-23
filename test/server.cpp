#include <cstdlib>
#include <iostream>
#include <string>
#include <glog/logging.h>

#include "udp_server.h"
#include "json.hpp"

using json = nlohmann::json;

constexpr char SERVER_FILE_PATH[] = "/work/files/server_files/";

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = google::GLOG_INFO;

  int port_num = 8080;
  int recv_window = 0;

  try{
    std::ifstream f("/work/config/server_config.json");
    if(f){
      json config = json::parse(f);
      port_num = config.value("port", 8080);
      recv_window = config.value("send_window", 0);
      LOG(INFO) << "Loaded configuration from server_config.json";
    }else{
      LOG(WARNING) << "server_config.json not found. Using default values.";
    }
  }catch(json::parse_error &e){
    LOG(ERROR) << "Failed to parse server_config.json: " << e.what() 
               << ". Using default values.";
  }

  LOG(INFO) << "Server starting with configuration:";
  LOG(INFO) << "- Port: " << port_num;
  LOG(INFO) << "- Receive Window: " << recv_window;
  LOG(INFO) << "- File Path: " << SERVER_FILE_PATH;


  int sfd = 0;
  char *message_recv;
  // if (argc < 3) {
  //   LOG(INFO) << "Please provide a port number and receive window";
  //   LOG(ERROR) << "Please provide format: <server-port> <receiver-window>";
  //   exit(1);
  // }
  // if (argv[1] != NULL) {
  //   port_num = atoi(argv[1]);
  // }
  // if (argv[2] != NULL) {
  //   recv_window = atoi(argv[2]);
  // }

  safe_udp::UdpServer *udp_server = new safe_udp::UdpServer();
  udp_server->rwnd_ = recv_window;
  sfd = udp_server->StartServer(port_num);
  message_recv = udp_server->GetRequest(sfd);
  // char cwd[1024];
  // if (getcwd(cwd, sizeof(cwd)) != NULL) {
  //   LOG(INFO) << "Current working directory: " << cwd;
  // } else {
  //   perror("getcwd() error");
  //   return 1;
  // }

  std::string file_name =
      std::string(SERVER_FILE_PATH) + std::string(message_recv);
  if (udp_server->OpenFile(file_name)) {
    udp_server->StartFileTransfer();
  } else {
    udp_server->SendError();
  }

  delete udp_server;
  return 0;
}
