#pragma once
#include "command.hpp"
#include "database.hpp"
#include "session.hpp"
#include <cstdlib>
#include <cstdio>
#include <future>
#include <format>
#include <string>
#include <vector>

constexpr std::string_view songs_dir = "/home/sisos/Code/Server/songs/";

enum class CommandCode : uint8_t {
    AddSong = 1,
    Ping = 2,
    Search = 3,
};

inline std::string arg_to_string(const Argument& arg) {
    return std::string(arg.bytes.begin(), arg.bytes.end());
}

inline std::string trim_newline(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }
    return s;
}

inline std::string sanitize_filename(std::string name) {
    for (char& c : name) {
        if (c == '/' || c == '\\' || c == ':' || c == '*' ||
            c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ) {
            c = '_';
        }
    }
    return name;
}

struct VideoMetadata {
    std::string video_id;
    std::string title;
    std::string channel_id;
    std::string channel_name;
    int duration_seconds = 0;
};

inline bool fetch_video_metadata(const std::string& url, VideoMetadata& out) {
    std::string metadata_cmd = std::format(
        "yt-dlp --print '%(id)s' "
        "--print '%(title)s' "
        "--print '%(channel_id)s' "
        "--print '%(channel)s' "
        "--print '%(duration)s' "
        "--no-playlist '{}' 2>/dev/null",
        url);

    FILE* metadata_pipe = popen(metadata_cmd.c_str(), "r");
    if (!metadata_pipe) {
        return false;
    }

    char line[8192] = {0};
    auto read_field = [&](std::string& dst) -> bool {
        if (!fgets(line, sizeof(line), metadata_pipe)) {
            return false;
        }
        dst = trim_newline(std::string(line));
        return true;
    };

    std::string duration_str;
    const bool ok = read_field(out.video_id) &&
                    read_field(out.title) &&
                    read_field(out.channel_id) &&
                    read_field(out.channel_name) &&
                    read_field(duration_str);

    pclose(metadata_pipe);

    if (!ok || out.video_id.empty() || out.title.empty() ||
        out.channel_id.empty() || out.channel_name.empty()) {
        return false;
    }

    try {
        out.duration_seconds = duration_str.empty() ? 0 : std::stoi(duration_str);
    } catch (...) {
        out.duration_seconds = 0;
    }

    return true;
}

inline void append_u32_be(std::vector<char>& out, uint32_t value) {
    out.push_back(static_cast<char>((value >> 24) & 0xFF));
    out.push_back(static_cast<char>((value >> 16) & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
    out.push_back(static_cast<char>(value & 0xFF));
}

inline void append_string(std::vector<char>& out, const std::string& s) {
    append_u32_be(out, static_cast<uint32_t>(s.size()));
    out.insert(out.end(), s.begin(), s.end());
}

inline void enqueue_download_and_register(const std::string& url) {
    auto future = std::async(std::launch::async, [url]() {
        VideoMetadata meta;
        if (!fetch_video_metadata(url, meta)) {
            std::print("✗ Error obteniendo metadata con yt-dlp: {}\n", url);
            return;
        }

        std::string safe_title = sanitize_filename(meta.title);
        if (safe_title.empty()) {
            safe_title = meta.video_id;
        }

        std::string output_path = std::format("{}{}_{}.mp3", songs_dir, safe_title, meta.video_id);

        std::print("Iniciando descarga: {} -> {}\n", url, output_path);

        std::string download_cmd = std::format(
            "yt-dlp -x --audio-format mp3 --audio-quality 0 "
            "--no-playlist '{}' -o '{}' 2>&1",
            url, output_path);

        int result = system(download_cmd.c_str());
        if (result != 0) {
            std::print("✗ Error descargando {} (código: {})\n", meta.title, result);
            return;
        }

        std::print("✓ Descarga completada: {}\n", meta.title);

        if (Database::addSong(meta.video_id,
                              meta.title,
                              meta.channel_id,
                              meta.channel_name,
                              meta.duration_seconds,
                              output_path)) {
            std::print("✓ Registrado en DB: {} ({})\n", meta.title, meta.video_id);
        } else {
            std::print("✗ Error registrando en DB: {} ({})\n", meta.title, meta.video_id);
        }
    });
}

inline void register_all_commands(CommandDispatcher& dispatcher) {
    // ADDSONG: tipo 1
    dispatcher.register_command(static_cast<uint8_t>(CommandCode::AddSong), [](void* ctx, std::vector<Argument> args) {
        if (args.size() != 1) {
            std::print("Error: ADDSONG requiere 1 argumento: <url>\n");
            return;
        }

        const std::string url = arg_to_string(args[0]);

        enqueue_download_and_register(url);
        std::print("OK: ADDSONG en progreso para {}\n", url);
    });

    // PING: tipo 2
    dispatcher.register_command(static_cast<uint8_t>(CommandCode::Ping), [](void* ctx, std::vector<Argument> args) {
        std::print("PONG\n");
    });

    // SEARCH: tipo 3 - Búsqueda de canciones
    dispatcher.register_command(static_cast<uint8_t>(CommandCode::Search), [](void* ctx, std::vector<Argument> args) {
        if (args.size() != 1) {
            std::print("Error: SEARCH requiere 1 argumento: <query>\n");
            return;
        }
        
        std::string query = arg_to_string(args[0]);
        
        // Buscar canciones
        auto results = Database::searchSongs(query);
        
        Session* session = static_cast<Session*>(ctx);
        
        // Construir respuesta binaria
        std::vector<char> response;
        
        // Número de resultados (4 bytes, big-endian)
        uint32_t num_results = results.size();
        append_u32_be(response, num_results);
        
        // Serializar cada resultado
        for (const auto& song : results) {
            // ID de canción (video id): [4 bytes length][data]
            append_string(response, song.id);

            // Title: [4 bytes length][data]
            append_string(response, song.title);

            // Channel/artist ID: [4 bytes length][data]
            append_string(response, song.channel);

            // Duration: 4 bytes big-endian
            append_u32_be(response, static_cast<uint32_t>(song.duration_seconds));

            // File path: [4 bytes length][data]
            append_string(response, song.file_path);
        }
        
        // Enviar respuesta al cliente
        boost::asio::async_write(
            session->socket_,
            boost::asio::buffer(response),
            [num_results](boost::system::error_code ec, std::size_t bytes_sent) {
                if (!ec) {
                    std::print("✓ Resultados de búsqueda enviados: {} canciones ({} bytes)\n", 
                              num_results, bytes_sent);
                } else {
                    std::print("✗ Error enviando resultados: {}\n", ec.message());
                }
            }
        );
    });
}
