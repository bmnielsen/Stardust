#include <BWAPI/Client/Client.h>
#include <stdexcept>

namespace BWAPI
{
  class ClientImpl {
  };

  Client BWAPIClient;
  Client::Client()
  {}
  Client::~Client()
  {
    this->disconnect();
  }
  bool Client::isConnected() const
  {
    return this->connected;
  }
  bool Client::connect()
  {
    throw std::runtime_error("Client not supported :(");
    return true;
  }
  void Client::disconnect()
  {
  }
  void Client::update()
  {
  }
}
