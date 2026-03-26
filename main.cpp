#include "session.hpp"
#include "commands.hpp"
#include <iostream>
#include <thread>
#include <vector>

// wtffff

class Server {
  tcp::acceptor acceptor_;
  CommandDispatcher &dispatcher_;

public:
  Server(boost::asio::io_context &io_context, short port,
         CommandDispatcher &dispatcher)
      : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
        dispatcher_(dispatcher) {
    std::print("Servidor escuchando en puerto {}\n", port);
    do_accept();
  }

private:
  void do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
          if (!ec) {
            std::make_shared<Session>(std::move(socket), dispatcher_)->start();
          }

          // Continúa aceptando más conexiones
          do_accept();
        });
  }
};

int main() {
  try {
    // Crear dispatcher y registrar comandos
    CommandDispatcher dispatcher;
    register_all_commands(dispatcher);

    boost::asio::io_context io_context;
    Server server(io_context, 8080, dispatcher);

    // Thread pool - 16 threads ejecutando el event loop
    std::vector<std::thread> threads;
    const int num_threads = 16;

    for (int i = 0; i < num_threads; ++i) {
      threads.emplace_back([&io_context]() { io_context.run(); });
    }

    // Espera a que todos los threads terminen
    for (auto &thread : threads) {
      thread.join();
    }
  } catch (std::exception &e) {
    std::cerr << "Error: " << e.what() << std::endl;
  }

  return 0;
}
