#include <cstdint>
#include <cstdio>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>
#include <format>
#include <unistd.h>

#include "protocol.h"
#include "world.h"
#include "log.h"


MessageType identify_type(const char* nomeArquivo) {
    std::string path(nomeArquivo);
    if (path.find(".txt") != std::string::npos) return MSG_TXT;
    if (path.find(".jpg") != std::string::npos) return MSG_JPG;
    if (path.find(".mp4") != std::string::npos) return MSG_MP4;
    return MSG_ERROR;
}


int send_file(const char* arquivo, int sock) {
    FILE* file = fopen(arquivo, "rb");
    if (!file) {
        fprintf(stderr, "Could not open file for sending: %s\n", arquivo);
        return -1;
    }

    MessageType type = identify_type(arquivo);
    Frame f_send;
    uint8_t seq = 0;
    while (1) {
        log ("SERVER", "CLIENT", (std::format("Enviando arquivo, sequencia: {}", seq)));
        uint8_t buffer[DATA_SIZE];
        size_t bytes_read = fread(buffer, 1, DATA_SIZE, file);
        if (bytes_read == 0) break;
        if (build_frame(&f_send, seq, type, buffer, (uint8_t)bytes_read) != 0) return -1;
        if (send(sock, &f_send, SERVER, CLIENT, INTERFACE_SERVER, seq) < 0) {
            log("SERVER", "ERRO", std::format("Falha ao enviar frame do pill, seq {}", seq));
            return -2;
        }
        if (++seq > 63) seq = 0;
    }
    fclose(file);

    log("SERVER", "INFO", "Mandou arquivo");

    if (build_frame(&f_send, seq, MSG_END, nullptr, 0) == 0) {
        if (send(sock, &f_send, SERVER, CLIENT, INTERFACE_SERVER, seq) < 0) {
            log("SERVER", "ERRO", std::format("Falha ao enviar MSG_END do pill, seq {}", seq));
            return -2;
        }
    }

    return 0;
}

static void send_game_over(int sock, bool win) {
    Frame f_send;
    uint8_t result = win ? 1 : 0;
    if (build_frame(&f_send, 0, MSG_OVER, &result, 1) == 0)
        send_frame(sock, &f_send, SERVER, CLIENT, INTERFACE_SERVER);
    log ("SERVER", "INFO", "Mandou game over");
}

static void send_ghost_positions(int sock, const std::vector<std::pair<int, int>> &ghost_coords) {
    uint8_t data[8];
    for (int i = 0; i < 4; ++i) {
        data[i * 2]     = (uint8_t)ghost_coords[i].first;
        data[i * 2 + 1] = (uint8_t)ghost_coords[i].second;
    }
    Frame f_send;
    if (build_frame(&f_send, 0, MSG_GHOSTS, data, 8) == 0)
        send_frame(sock, &f_send, SERVER, CLIENT, INTERFACE_SERVER);
}

void send_world(int sock, char world[SIZE_WORLD][SIZE_WORLD]) {
    update_world_csv(world);

    FILE* mundo = fopen("mundo.csv", "rb");
    if (!mundo) {
        fprintf(stderr, "Could not open mundo.csv for sending\n");
        return;
    }

    Frame f_send;
    uint8_t seq = 0;
    while (1) {
        uint8_t buffer[DATA_SIZE];
        size_t bytes_read = fread(buffer, 1, DATA_SIZE, mundo);
        if (bytes_read == 0) break;
        if (build_frame(&f_send, seq, MSG_WORLD, buffer, (uint8_t)bytes_read) != 0) break;

        if (send(sock, &f_send, SERVER, CLIENT, INTERFACE_SERVER, seq) < 0) break;

        if (++seq > 63) seq = 0;
    }
    fclose(mundo);

    if (build_frame(&f_send, seq, MSG_END, nullptr, 0) == 0)
        send_frame(sock, &f_send, SERVER, CLIENT, INTERFACE_SERVER);
}

static void remove_pill(std::vector<PillInfo> &pills, char pill_id) {
    pills.erase(
        std::remove_if(pills.begin(), pills.end(),
            [pill_id](const PillInfo &p) { return p.id == pill_id; }),
        pills.end());
}

static void wait_for_client_and_init(int sock,
    char world[SIZE_WORLD][SIZE_WORLD],
    std::pair<int, int> &pacman_coord,
    std::vector<std::pair<int, int>> &ghost_coords,
    std::vector<PillInfo> &pills) {

    printf("Server pronto. Esperando client (run: sudo ./client)\n");
    fflush(stdout);

    Frame f;
    uint8_t exp_seq = 0;
    while (1) {
        if (recv_frame(sock, &f, SERVER, CLIENT, INTERFACE_SERVER, exp_seq) >= 0
                && f.type == MSG_INIT) {
            uint8_t world_map = f.data[0];
            ghost_coords = init_world(world, pacman_coord, world_map, pills);
            send_world(sock, world);
            if (++exp_seq > 63) exp_seq = 0;
            return;
        }
    }
}

int main() {
    log("SERVER", "INICIANDO NOVA TRANSMISSÃO", "");
    bool ghosts_frozen = false;

    int sock = create_raw_socket(INTERFACE_SERVER);

    char world[SIZE_WORLD][SIZE_WORLD];
    std::pair<int, int> pacman_coord = {{(SIZE_WORLD - 1) / 2}, {(SIZE_WORLD - 1) / 2}};
    bool green_go_left  = false;
    bool red_going_right = true;
    bool blue_going_up   = true;

    std::vector<std::pair<int, int>> ghost_coords;
    std::vector<PillInfo> pills;
    Frame f;

    wait_for_client_and_init(sock, world, pacman_coord, ghost_coords, pills);

    auto last_tick = std::chrono::steady_clock::now();
    const auto ghost_interval = std::chrono::milliseconds(300);

    uint8_t exp_seq = 0;
    while (1) {
        if (recv_frame(sock, &f, SERVER, CLIENT, INTERFACE_SERVER, exp_seq) >= 0) {
            if (f.type == MSG_END){
                log ("SERVER", "INFO", "Recebeu mensagem end");
                continue;
            }

            if (f.type == MSG_INIT) {
                log ("SERVER", "INFO", "Recebeu mensagem init");
                send_world(sock, world);
                continue;
            }

            if (++exp_seq > 63) exp_seq = 0;

            if (f.type == MSG_UP || f.type == MSG_DOWN || f.type == MSG_LEFT || f.type == MSG_RIGHT) {
                log ("SERVER", "INFO", "Recebeu mensagem flecha");
                ghosts_frozen = false;
                int mv = move_pacman(world, pacman_coord, f.type);
                if (mv == -1) {
                    send_game_over(sock, false);
                    break;
                }

                if (mv >= '1' && mv <= '6') {
                    const PillInfo *pill = find_pill_by_id(pills, (char)mv);
                    if (pill) {
                        ghosts_frozen = true;
                        int result = send_file(pill->file_path, sock);
                        if (result == -2) {
                            Frame resync;
                            build_frame(&resync, 0, MSG_RESYNC, nullptr, 0);
                            log("SERVER", "INFO", "Aguardando ack do RESYNC do client");
                            int attempts = 100;
                            while (attempts-- > 0) {
                                send_frame(sock, &resync, SERVER, CLIENT, INTERFACE_SERVER);
                                Frame ack;

                                int r = recv_frame(sock, &ack, NULL, NULL, NULL, 0);
                                if (r == 0 && ack.type == MSG_ACK) {
                                    log("SERVER", "INFO", "Recebido ack RESYNC do client, reenviando arquivo");
                                    result = send_file(pill->file_path, sock);
                                    break;
                                }
                                usleep(500000);
                            }
                        }

                        if (result == 0)
                            remove_pill(pills, pill->id);
                    }
                    send_world(sock, world);
                    if (pills.empty()) {
                        send_game_over(sock, true);
                        break;
                    }
                } else if (mv == 0) {
                    send_world(sock, world);
                }
                continue;
            }

            if (f.type == MSG_OVER)
                break;
            continue;
        }

        if (!ghosts_frozen) {
            auto now = std::chrono::steady_clock::now();
            if (now - last_tick >= ghost_interval) {
                last_tick = now;
                if (move_ghosts(world, ghost_coords, pacman_coord, green_go_left, red_going_right, blue_going_up) == -1) {
                    send_game_over(sock, false);
                    break;
                }
                send_ghost_positions(sock, ghost_coords);
            }
        }
    }

    return 0;
}
