#pragma once

#include <memory>

namespace BWAPI
{
  class ClientImpl;
  class Client
  {
  public:
    Client();
    ~Client();

    bool isConnected() const;
    bool connect();
    void disconnect();
    void update();

  private:
    std::unique_ptr<ClientImpl> impl;
    
    bool connected = false;
  };
  extern Client BWAPIClient;
}
