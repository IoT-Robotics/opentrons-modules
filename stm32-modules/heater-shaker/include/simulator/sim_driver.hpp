#pragma once
#include "heater-shaker/host_comms_task.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "simulator/simulator_queue.hpp"

namespace sim_driver {
const std::string SOCKET_DRIVER_NAME = "Socket";
const std::string STDIN_DRIVER_NAME = "Stdin";
class SimDriver {

  public:
    virtual const std::string& get_name() const = 0
    virtual void write() = 0;
    virtual void read(tasks::Tasks<SimulatorMessageQueue>& tasks) = 0;
};

class SocketSimDriver : public SimDriver {
    std::string name = SOCKET_DRIVER_NAME;
    std::string host;
    int port;

  public:
    SocketSimDriver(std::string);
    std::string get_host();
    int get_port();
    std::string get_name();
    void write();
    void read(tasks::Tasks<SimulatorMessageQueue>& tasks);
};

class StdinSimDriver : public SimDriver {
    std::string name = STDIN_DRIVER_NAME;

  public:
    StdinSimDriver();
    std::string get_name();
    void write();
    void read(tasks::Tasks<SimulatorMessageQueue>& tasks);
};
}  // namespace sim_driver