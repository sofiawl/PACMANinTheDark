#include "log.h"

// Adicionamos o parâmetro 'origem' aqui
void log(const std::string& origem, const std::string& nivel, const std::string& mensagem) {
    std::ofstream arquivo_log("log_redes.txt", std::ios::app);
    
    if (!arquivo_log.is_open()) {
        std::cerr << "Erro ao abrir o arquivo de log!" << std::endl;
        return;
    }

    auto agora = std::chrono::system_clock::now();
    auto tempo_c = std::chrono::system_clock::to_time_t(agora);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(agora.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&tempo_c), "%Y-%m-%d %H:%M:%S");

    // Incluímos a [origem] na formatação da linha
    std::string linha_log = "[" + ss.str() + "." + std::to_string(ms.count()) + "] "
                            "[" + origem + "] "
                            "[" + nivel + "] " + mensagem;

    arquivo_log << linha_log << std::endl;
    
}
