#pragma once
#include <unordered_map>
#include <functional>
#include <vector>
#include <print>
#include <cstdint>

// Estructura de cada argumento
struct Argument{
    std::vector<uint8_t> bytes; // raw bytes
}; // para leer estos argumentos, depende de cada función lo que espere de argumento y hacer el error handling dentro de cada función

// Estructura del comando con header binario
struct Command {
    uint8_t type;           // Tipo de comando (1 byte)
    uint32_t total_length;  // Longitud total incluyendo header (4 bytes)
    std::vector<Argument> args;  // Argumentos del comando 
}; 

class CommandDispatcher {
    std::unordered_map<uint8_t, std::function<void(void*, std::vector<Argument>)>> commands_;

public:
    // Registrar comando con tipo numérico
    void register_command(uint8_t type, std::function<void(void*, std::vector<Argument>)> handler) {
        commands_[type] = handler;
    }

    // Ejecutar comando
    void execute(const Command& cmd, void* context) {
        auto it = commands_.find(cmd.type);
        
        if (it == commands_.end()) {
            std::print("Error: Comando desconocido tipo: {}\n", cmd.type);
            return;
        }

        try {
            it->second(context, cmd.args);
        } catch (const std::exception& e) {
            std::print("Error ejecutando comando tipo {}: {}\n", cmd.type, e.what());
        }
    }
};
