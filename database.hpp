#pragma once
#include <sqlite3.h>
#include <mutex>
#include <optional>
#include <print>
#include <string>
#include <vector>

constexpr std::string_view db_path = "/mnt/juegos/DBs/music_server.db";

// Estructura para resultados de búsqueda
struct Song {
    std::string id;
    std::string title;
    std::string channel;
    int duration_seconds = 0;
    std::string file_path;
};

class Database {
private:
    static inline std::mutex db_mutex_;  // Mutex global para serializar acceso
    
    // Método privado: crear o actualizar artista, retorna true si tuvo éxito
    static bool upsertArtist(sqlite3* db, const std::string& artist_id, const std::string& artist_name) {
        // Verificar si el artista existe por id
        const char* check_sql = "SELECT \"song count\" FROM artist WHERE id = ?;";
        sqlite3_stmt* check_stmt;
        
        int rc = sqlite3_prepare_v2(db, check_sql, -1, &check_stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::print("Error preparando búsqueda de artista: {}\n", sqlite3_errmsg(db));
            return false;
        }
        
        sqlite3_bind_text(check_stmt, 1, artist_id.c_str(), -1, SQLITE_TRANSIENT);
        rc = sqlite3_step(check_stmt);
        
        if (rc == SQLITE_ROW) {
            // Artista existe - incrementar song count
            sqlite3_finalize(check_stmt);
            
            const char* update_sql = "UPDATE artist SET name = ?, \"song count\" = \"song count\" + 1 WHERE id = ?;";
            sqlite3_stmt* update_stmt;
            
            rc = sqlite3_prepare_v2(db, update_sql, -1, &update_stmt, nullptr);
            if (rc != SQLITE_OK) {
                std::print("Error preparando update de artista: {}\n", sqlite3_errmsg(db));
                return false;
            }
            
            sqlite3_bind_text(update_stmt, 1, artist_name.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(update_stmt, 2, artist_id.c_str(), -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(update_stmt);
            sqlite3_finalize(update_stmt);
            
            if (rc != SQLITE_DONE) {
                std::print("Error actualizando artista: {}\n", sqlite3_errmsg(db));
                return false;
            }
            
            std::print("Artista actualizado: {} ({}) (song count++)\n", artist_name, artist_id);
            return true;
            
        } else if (rc == SQLITE_DONE) {
            // Artista no existe - crear con song count = 1
            sqlite3_finalize(check_stmt);
            
            const char* insert_sql = "INSERT INTO artist (id, name, \"song count\") VALUES (?, ?, 1);";
            sqlite3_stmt* insert_stmt;
            
            rc = sqlite3_prepare_v2(db, insert_sql, -1, &insert_stmt, nullptr);
            if (rc != SQLITE_OK) {
                std::print("Error preparando insert de artista: {}\n", sqlite3_errmsg(db));
                return false;
            }
            
            sqlite3_bind_text(insert_stmt, 1, artist_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(insert_stmt, 2, artist_name.c_str(), -1, SQLITE_TRANSIENT);
            rc = sqlite3_step(insert_stmt);
            sqlite3_finalize(insert_stmt);
            
            if (rc != SQLITE_DONE) {
                std::print("Error insertando artista: {}\n", sqlite3_errmsg(db));
                return false;
            }
            
            std::print("Artista creado: {} ({}) (song count=1)\n", artist_name, artist_id);
            return true;
            
        } else {
            std::print("Error verificando artista: {}\n", sqlite3_errmsg(db));
            sqlite3_finalize(check_stmt);
            return false;
        }
    }
    
public:
    static bool addSong(const std::string& song_id,
                        const std::string& title,
                        const std::string& artist_id,
                        const std::string& artist_name,
                        int duration_seconds,
                        const std::string& file_path) {
        std::lock_guard<std::mutex> lock(db_mutex_);  // Solo un thread a la vez
        
        sqlite3* db;
        
        int rc = sqlite3_open(db_path.data(), &db);
        if (rc) {
            std::print("Error abriendo DB: {}\n", sqlite3_errmsg(db));
            return false;
        }
        
        // Primero: crear o actualizar artista
        if (!upsertArtist(db, artist_id, artist_name)) {
            sqlite3_close(db);
            return false;
        }

        // Segundo: insertar canción con FK a artist
        const char* sql = "INSERT INTO song (id, title, channel, duration_seconds, file_path) VALUES (?, ?, ?, ?, ?);";
        sqlite3_stmt* stmt;
        
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::print("Error preparando statement: {}\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return false;
        }

        sqlite3_bind_text(stmt, 1, song_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, artist_name.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 4, duration_seconds);
        sqlite3_bind_text(stmt, 5, file_path.c_str(), -1, SQLITE_TRANSIENT);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::print("Error insertando: {}\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }

        std::print("Canción agregada: {} ({})\n", title, song_id);
        
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return true;
    }
    
    // Buscar canción por ID - retorna location o std::nullopt si no existe
    static std::optional<std::string> getSongLocationById(const std::string& id) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        
        sqlite3* db;
        int rc = sqlite3_open(db_path.data(), &db);
        if (rc) {
            std::print("Error abriendo DB: {}\n", sqlite3_errmsg(db));
            return std::nullopt;
        }
        
        const char* sql = "SELECT file_path FROM song WHERE id = ?;";
        sqlite3_stmt* stmt;
        
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::print("Error preparando búsqueda por ID: {}\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return std::nullopt;
        }
        
        sqlite3_bind_text(stmt, 1, id.c_str(), -1, SQLITE_TRANSIENT);
        
        std::optional<std::string> result;
        rc = sqlite3_step(stmt);
        
        if (rc == SQLITE_ROW) {
            const char* file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (file_path) {
                result = std::string(file_path);
                std::print("Canción ID {} encontrada: {}\n", id, *result);
            }
        } else if (rc == SQLITE_DONE) {
            std::print("Canción ID {} no encontrada\n", id);
        } else {
            std::print("Error buscando canción: {}\n", sqlite3_errmsg(db));
        }
        
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return result;
    }
    
    // Búsqueda de canciones con ranking de similitud
    // Retorna hasta 15 resultados más relevantes
    static std::vector<Song> searchSongs(const std::string& query) {
        std::lock_guard<std::mutex> lock(db_mutex_);
        std::vector<Song> results;
        
        if (query.length() < 2) {
            std::print("Query muy corta (mínimo 2 caracteres)\n");
            return results;
        }
        
        sqlite3* db;
        int rc = sqlite3_open(db_path.data(), &db);
        if (rc) {
            std::print("Error abriendo DB: {}\n", sqlite3_errmsg(db));
            return results;
        }
        
        // SQL con ranking de similitud:
        // 1. Coincidencia exacta (score = 100)
        // 2. Empieza con query (score = 90)
        // 3. Contiene query (score = 80)
        // Ordenar por score descendente, luego por título
        const char* sql = R"(
            SELECT 
                id, title, channel, duration_seconds, file_path,
                CASE 
                    WHEN LOWER(title) = LOWER(?) THEN 100
                    WHEN LOWER(title) LIKE LOWER(?) THEN 90
                    WHEN LOWER(title) LIKE LOWER(?) THEN 80
                    ELSE 0
                END as score
            FROM song
            WHERE score > 0
            ORDER BY score DESC, title ASC
            LIMIT 15;
        )";
        
        sqlite3_stmt* stmt;
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr);
        if (rc != SQLITE_OK) {
            std::print("Error preparando búsqueda: {}\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return results;
        }
        
        // Bind parámetros: exacto, empieza con, contiene
        std::string starts_with = query + "%";
        std::string contains = "%" + query + "%";
        
        sqlite3_bind_text(stmt, 1, query.c_str(), -1, SQLITE_TRANSIENT);      // Exacto
        sqlite3_bind_text(stmt, 2, starts_with.c_str(), -1, SQLITE_TRANSIENT); // Empieza
        sqlite3_bind_text(stmt, 3, contains.c_str(), -1, SQLITE_TRANSIENT);    // Contiene
        
        // Obtener resultados
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            Song song;
            const char* id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            if (id) song.id = id;
            
            const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            const char* channel = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            song.duration_seconds = sqlite3_column_int(stmt, 3);
            const char* file_path = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
            
            if (title) song.title = title;
            if (channel) song.channel = channel;
            if (file_path) song.file_path = file_path;
            
            results.push_back(song);
        }
        
        if (rc != SQLITE_DONE) {
            std::print("Error ejecutando búsqueda: {}\n", sqlite3_errmsg(db));
        }
        
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        
        std::print("Búsqueda '{}': {} resultados\n", query, results.size());
        return results;
    }
};
