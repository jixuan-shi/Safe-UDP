#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <iostream>
#include <fstream>

#include "udp_client.h"
#include "json.hpp"

#include <glog/logging.h>

using json = nlohmann::json;

int main(int argc, char *argv[]) {
  google::InitGoogleLogging(argv[0]);
  FLAGS_logtostderr = true;
  FLAGS_minloglevel = google::GLOG_INFO;
  LOG(INFO) << "Starting the client !!!";

  std::string server_ip = "127.0.0.1";
  int port_num_init = 8080;
  std::string file_name = "天龙八部.txt";
  int receiver_window = 100;
  int simulation_mode = 3;
  int drop_percentage = 0;

  // if (argc != 7) {
  //   LOG(ERROR) << "Please provide format: <server-ip> <server-port> "
  //                 "<file-name> <receiver-window> <control-param> <drop/delay%>";
  //   exit(1);
  // }

  try{
    std::ifstream f("/work/config/client_config.json");
    if(f){
      json config = json::parse(f);
      server_ip = config["server"].value("ip", "127.0.0,1");
      port_num_init = config["server"].value("port", 8080);
      file_name = config.value("file_to_request", "天龙八部.txt");
      receiver_window = config.value("receive_window", 100);
      simulation_mode = config["simulation"].value("simulation_mode", 0);
      drop_percentage = config["simulation"].value("drop_percentage", 0);
      LOG(INFO) << "Loaded configuration from client_config.json";
    }else{
      LOG(WARNING) << "client_config.json not found. Using default values.";
    }
  }catch(json::exception& e){
    LOG(ERROR) << "Failed to parse client_config.json: " << e.what()
               << ". Using default values.";
  }

  LOG(INFO) << "Client starting with configuration:";
  LOG(INFO) << "- Server: " << server_ip << ":" << port_num_init;
  LOG(INFO) << "- File to Request: " << file_name;
  LOG(INFO) << "- Receive Window: " << receiver_window;
  LOG(INFO) << "- Simulation Mode: " << simulation_mode;
  LOG(INFO) << "- Drop Percentage: " << drop_percentage;

  safe_udp::UdpClient *udp_client = new safe_udp::UdpClient();
  // std::string server_ip(argv[1]);
  // std::string port_num(argv[2]);
  // std::string file_name(argv[3]);
  udp_client->receiver_window_ = std::move(receiver_window);

  int control_param = std::move(simulation_mode);
  LOG(INFO) << "control_param: " << control_param;
  if (control_param == 0) {
    udp_client->is_delay_ = false;
    udp_client->is_packet_drop_ = false;
  } else if (control_param == 1) {
    udp_client->is_packet_drop_ = true;
    udp_client->is_delay_ = false;
  } else if (control_param == 2) {
    udp_client->is_packet_drop_ = false;
    udp_client->is_delay_ = true;
  } else if (control_param == 3) {
    udp_client->is_packet_drop_ = true;
    udp_client->is_delay_ = true;
  } else {
    LOG(ERROR) << "Invalid argument, should be range in 0-3 !!!";
    return 0;
  }

  udp_client->prob_value_ = std::move(drop_percentage);

  std::string port_num_str = std::to_string(port_num_init);
  udp_client->CreateSocketAndServerConnection(std::move(server_ip), std::move(port_num_str));
  udp_client->SendFileRequest(std::move(file_name));

  delete udp_client;
  return 0; 
}