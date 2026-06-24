#include <stdio.h>
#include <sys/socket.h>
#include <net/if.h>
#include <ncursesw/ncurses.h>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <sys/time.h>
#include <format>

#include "protocol.h"
#include "log.h"

#define WORLD_WAIT_MS 3000
#define CONNECT_RETRY_MS 500
#include "client_view.h"
#include "world.h"


static bool load_world_csv(const char* csv_path, char world[SIZE_WORLD][SIZE_WORLD]) {
    std::ifstream in(csv_path);
    if (!in.is_open()) return false;

    std::string line;
    for (int i = 0; i < SIZE_WORLD; ++i) {
        if (!std::getline(in, line)) return false;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        for (int j = 0; j < SIZE_WORLD; ++j)
            world[i][j] = (j < (int)line.size()) ? line[j] : '0';
    }
    return true;
}

static void sync_pacman_from_world(char world[SIZE_WORLD][SIZE_WORLD], std::pair<int, int> &pacman_coord) {
    for (int i = 0; i < SIZE_WORLD; ++i) {
        for (int j = 0; j < SIZE_WORLD; ++j) {
            if (world[i][j] == 'P') {
                pacman_coord = {i, j};
                return;
            }
        }
    }
}

static const char GHOST_CHARS[] = "RBGY";

static void apply_ghost_update(char world[SIZE_WORLD][SIZE_WORLD], const uint8_t *data, uint8_t size) {
    if (size < 8) return;
    for (int i = 0; i < SIZE_WORLD; ++i) {
        for (int j = 0; j < SIZE_WORLD; ++j) {
            char c = world[i][j];
            if (c == 'R' || c == 'B' || c == 'G' || c == 'Y')
                world[i][j] = '0';
        }
    }
    for (int g = 0; g < 4; ++g) {
        int r = data[g * 2];
        int c = data[g * 2 + 1];
        if (r >= 0 && r < SIZE_WORLD && c >= 0 && c < SIZE_WORLD)
            world[r][c] = GHOST_CHARS[g];
    }
}

static void reconnect_client_socket(int& sock);

// 0 = nothing, 1 = ghosts moved, -1 = lose, -2 = win, -3 = cable down (world resync needed)
static int poll_network(int& sock, char client_world[SIZE_WORLD][SIZE_WORLD]) {
    int changed = 0;
    Frame frame;

    uint8_t exp_seq = 0;
    int recv_fail_count = 0;
    while (1) {
        int rv = recv_frame(sock, &frame, CLIENT, SERVER, INTERFACE_CLIENT, exp_seq);
        if (rv < 0) {
            recv_fail_count++;
            if (rv == -2) {
                log("CLIENT", "INFO", "Interface perdida durante jogo, pedindo resync do mundo");
                reconnect_client_socket(sock);
                return -3;
            }
            if (recv_fail_count >= 30) {
                log("CLIENT", "INFO", "Link instavel, pedindo resync do mundo");
                return -3;
            }
            break;
        }
        recv_fail_count = 0;

        if (frame.type == MSG_ACK || frame.type == MSG_NACK)
            continue;

        if (frame.type == MSG_GHOSTS) {
            apply_ghost_update(client_world, frame.data, frame.size);
            changed = 1;
            continue;
        }

        if (frame.type == MSG_OVER) {
            log("CLIENT", "INFO", "Recebeu mensagem over");
            bool won = (frame.size > 0 && frame.data[0] != 0);
            return won ? -2 : -1;
        }

        if (++exp_seq > 63) exp_seq = 0;
    }

    return changed;
}

// Blocks until server resends the world. Returns false on game over.
static bool wait_world_resync(int& sock, char client_world[SIZE_WORLD][SIZE_WORLD],
    std::pair<int, int> &pacman_coord) {

    log("CLIENT", "INFO", "Aguardando resync do mundo");
    FILE* mundo = fopen("mundo.csv", "wb");
    if (!mundo) return false;

    Frame frame;
    uint8_t exp_seq = 0;
    bool got_world_data = false;
    bool resync_pending = true;
    int recv_fail_count = 0;

    while (1) {
        if (resync_pending) {
            Frame resync;
            build_frame(&resync, 0, MSG_RESYNC, nullptr, 0);
            send_frame(sock, &resync, CLIENT, SERVER, INTERFACE_CLIENT);
        }

        int rv = recv_frame(sock, &frame, CLIENT, SERVER, INTERFACE_CLIENT, exp_seq);
        if (rv < 0) {
            recv_fail_count++;
            if (rv == -2) {
                reconnect_client_socket(sock);
                resync_pending = true;
                recv_fail_count = 0;
            } else if (recv_fail_count >= 30) {
                resync_pending = true;
                recv_fail_count = 0;
            }
            continue;
        }
        recv_fail_count = 0;

        if (frame.type == MSG_ACK || frame.type == MSG_NACK)
            continue;

        if (frame.type == MSG_RESYNC) {
            log("CLIENT", "INFO", "Recebeu RESYNC do servidor, aguardando mundo");
            fclose(mundo);
            mundo = fopen("mundo.csv", "wb");
            if (!mundo) return false;
            got_world_data = false;
            exp_seq = 0;
            resync_pending = false;
            continue;
        }

        if (frame.type == MSG_OVER) {
            if (mundo) fclose(mundo);
            return false;
        }

        if (++exp_seq > 63) exp_seq = 0;

        if (frame.type == MSG_WORLD) {
            fwrite(frame.data, 1, frame.size, mundo);
            got_world_data = true;
            resync_pending = false;
            continue;
        }

        if (frame.type == MSG_END && got_world_data) {
            fflush(mundo);
            fclose(mundo);
            load_world_csv("mundo.csv", client_world);
            sync_pacman_from_world(client_world, pacman_coord);
            log("CLIENT", "INFO", "Mundo resincronizado");
            return true;
        }
    }
}

static void drain_ghost_updates(int sock, char client_world[SIZE_WORLD][SIZE_WORLD]) {
    Frame frame;
    uint8_t exp_seq = 0; 
    while (recv_frame(sock, &frame, CLIENT, SERVER, INTERFACE_CLIENT, exp_seq) >= 0) {
        if (frame.type == MSG_GHOSTS)
            apply_ghost_update(client_world, frame.data, frame.size);
        if (++exp_seq > 63) exp_seq = 0;
    }
}

static void sync_pacman_from_csv(const char* csv_path, std::pair<int, int> &pacman_coord) {
    char world[SIZE_WORLD][SIZE_WORLD];
    if (!load_world_csv(csv_path, world)) return;
    sync_pacman_from_world(world, pacman_coord);
}

static const char* prize_extension(int type) {
    if (type == MSG_TXT) return ".txt";
    if (type == MSG_JPG) return ".jpg";
    if (type == MSG_MP4) return ".mp4";
    return ".bin";
}

static void reset_prize_transfer(FILE*& prize, bool* got_prize, int* prize_type,
    char* prize_path, size_t prize_path_sz) {
    if (prize) {
        fclose(prize);
        prize = nullptr;
    }
    if (prize_path[0] != '\0')
        remove(prize_path);
    *got_prize = false;
    *prize_type = 0;
    prize_path[0] = '\0';
}

static void reconnect_client_socket(int& sock) {
    log("CLIENT", "INFO", "Aguardando reconexao da interface");
    close(sock);
    sock = -1;
    while (if_nametoindex(INTERFACE_CLIENT) == 0)
        usleep(500000);
    usleep(500000);
    log("CLIENT", "INFO", "Interface voltou, recriando socket");
    sock = create_raw_socket(INTERFACE_CLIENT);
}

// timeout_ms == 0 waits until the full world arrives.
static bool receive_world(int sock, uint8_t *map_data, int timeout_ms) {
    (void)map_data;
    FILE* mundo = fopen("mundo.csv", "wb");
    if (!mundo) {
        fprintf(stderr, "Could not open mundo.csv for writing\n");
        return false;
    }

    Frame frame;
    bool got_world_data = false;
    bool use_timeout = timeout_ms > 0;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    uint8_t exp_seq = 0; 
    while (1) {
        if (use_timeout && !got_world_data && std::chrono::steady_clock::now() >= deadline) {
            fclose(mundo);
            return false;
        }

        int rv = recv_frame(sock, &frame, CLIENT, SERVER, INTERFACE_CLIENT, exp_seq);
        if (rv < 0) continue;
        //printf("seq esperada: %d \n", exp_seq);
        


        if (frame.type == MSG_ACK || frame.type == MSG_NACK) {
            continue;
        } else {
            if (++exp_seq > 63) exp_seq = 0;
        }

        if (frame.type == MSG_WORLD) {
            fwrite(frame.data, 1, frame.size, mundo);
            got_world_data = true;
            if (use_timeout)
                deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
            continue;
        }

        if (frame.type == MSG_END) {
            fflush(mundo);
            fclose(mundo);
            return got_world_data;
        }

    }
}

static bool wait_for_server_and_world(int sock, uint8_t *map_data) {
    printf("Esperando server (run: sudo ./server)\n");
    fflush(stdout);

    while (1) {
        log("CLIENT", "INFO", "Mandou init");
        if (send_init(sock, map_data) != 0) {
            usleep(CONNECT_RETRY_MS * 1000);
            continue;
        }
        if (receive_world(sock, map_data, WORLD_WAIT_MS))
            return true;
        usleep(CONNECT_RETRY_MS * 1000);
    }
}

// 1 = success, 0 = lose, 2 = win, -1 = error
static int sync_after_move(int& sock, char client_world[SIZE_WORLD][SIZE_WORLD],
    std::pair<int, int> &pacman_coord, char *prize_path, size_t prize_path_sz,
    int *prize_type, bool *got_prize) {

    *got_prize = false;
    prize_path[0] = '\0';
    *prize_type = 0;

    FILE* mundo = fopen("mundo.csv", "wb");
    if (!mundo) return -1;

    FILE* prize = nullptr;
    bool world_done = false;
    Frame frame;

    uint8_t exp_seq = 0;
    int recv_fail_count = 0;
    bool resync_pending = false;
    while (1) {
        if (resync_pending) {
            Frame resync;
            build_frame(&resync, 0, MSG_RESYNC, nullptr, 0);
            send_frame(sock, &resync, CLIENT, SERVER, INTERFACE_CLIENT);
        }

        int rv = recv_frame(sock, &frame, CLIENT, SERVER, INTERFACE_CLIENT, exp_seq);
        if (rv < 0) {
            if (prize || *got_prize) {
                recv_fail_count++;
                if (recv_fail_count >= 30) {
                    log("CLIENT", "INFO", "Transferencia do pill interrompida, pedindo RESYNC");
                    reset_prize_transfer(prize, got_prize, prize_type, prize_path, prize_path_sz);
                    exp_seq = 0;
                    recv_fail_count = 0;
                    resync_pending = true;
                }
            }
            if (rv == -2) {
                log("CLIENT", "INFO", "Interface perdida, aguardando reconexao");
                reset_prize_transfer(prize, got_prize, prize_type, prize_path, prize_path_sz);
                exp_seq = 0;
                reconnect_client_socket(sock);
                resync_pending = true;
            }
            if (world_done) return 1;
            continue;
        }

        recv_fail_count = 0;

        if (frame.type == MSG_ACK){
            log("CLIENT", "INFO", "Recebeu ack");
            continue;
        }

        if (frame.type == MSG_NACK){
            log("CLIENT", "INFO", "Recebeu nack");
        }

        if (frame.type == MSG_RESYNC) {
            log("CLIENT", "INFO", "Recebeu RESYNC do servidor");
            reset_prize_transfer(prize, got_prize, prize_type, prize_path, prize_path_sz);
            exp_seq = 0;
            resync_pending = false;
            continue;
        }

        if (++exp_seq > 63) exp_seq = 0;

        if (frame.type == MSG_GHOSTS) {
            log ("CLIENT", "INFO", "Recebeu fantasmas");
            apply_ghost_update(client_world, frame.data, frame.size);
            continue;
        }

        if (frame.type == MSG_OVER) {
            log ("CLIENT", "INFO", "Recebeu mensagem over");
            if (prize) { fclose(prize); prize = nullptr; }
            if (mundo) { fclose(mundo); mundo = nullptr; }
            bool won = (frame.size > 0 && frame.data[0] != 0);
            return won ? 2 : 0;
        }

        if (!world_done) {
            // before world is complete: pill prize or world frames
            if (frame.type == MSG_TXT || frame.type == MSG_JPG || frame.type == MSG_MP4) {
                if (!prize) {
                    *prize_type = frame.type;
                    snprintf(prize_path, prize_path_sz, "pills/recv%s", prize_extension(frame.type));
                    prize = fopen(prize_path, "wb");
                    if (!prize) { fclose(mundo); return -1; }
                    *got_prize = true;
                    resync_pending = false;
                }
                fwrite(frame.data, 1, frame.size, prize);
                continue;
            }

            if (prize && frame.type == MSG_END) {
                fclose(prize);
                prize = nullptr;
                continue;
            }

            if (frame.type == MSG_WORLD) {
                fwrite(frame.data, 1, frame.size, mundo);
                continue;
            }

            if (frame.type == MSG_END) {
                fflush(mundo);
                fclose(mundo);
                mundo = nullptr;
                load_world_csv("mundo.csv", client_world);
                sync_pacman_from_world(client_world, pacman_coord);
                world_done = true;
                continue;
            }
        } else {
            return 1;
        }
    }
}

int receive_key(int sock, int key) {
    MessageType type;
    switch (key) {
        case KEY_UP:    type = MSG_UP; break;
        case KEY_DOWN:  type = MSG_DOWN; break;
        case KEY_LEFT:  type = MSG_LEFT; break;
        case KEY_RIGHT: type = MSG_RIGHT; break;
        case 'q': case 'Q': type = MSG_OVER; break;
        default: return 0;
    }
    log ("CLIENT", "INFO", "Apertou em uma tecla"); 

    Frame f;
    if (build_frame(&f, 0, type, nullptr, 1) == 0)
        send(sock, &f, CLIENT, SERVER, INTERFACE_CLIENT, 0);

    log ("CLIENT", "INFO", "Mandou tecla apertada"); 

    if (key == 'q' || key == 'Q') return -1;
    return 1;
}

static void mostrar_premio(const char* nome_arquivo) {
    char chmod_cmd[128];
    snprintf(chmod_cmd, sizeof(chmod_cmd), "chmod 777 %s", nome_arquivo);
    system(chmod_cmd);

    char comando[512];
    snprintf(comando, sizeof(comando),
        "su $SUDO_USER -c \"xdg-open '%s' >/dev/null 2>&1 </dev/null &\"",
        nome_arquivo);
    system(comando);
}

int main() {
    log("CLIENT", "INICIANDO NOVA TRANSMISSÃO", ""); 
    int sock = create_raw_socket(INTERFACE_CLIENT);

    int num;
    printf("Digite 1 para UFPR map e 2 para default map\n");
    scanf("%d", &num);
    uint8_t data_map_world[DATA_SIZE];
    if (num == UFPR_MAP)
        data_map_world[0] = UFPR_MAP;
    else
        data_map_world[0] = DEFAULT_MAP;

    int move_count = 0;
    int radius = 1;
    std::pair<int, int> pacman_coord = {(SIZE_WORLD - 1) / 2, (SIZE_WORLD - 1) / 2};

    if (!wait_for_server_and_world(sock, data_map_world))
        return 1;

    printf("Connected to server.\n");

    sync_pacman_from_csv("mundo.csv", pacman_coord);
    init_client_view();

    char client_world[SIZE_WORLD][SIZE_WORLD];
    if (!load_world_csv("mundo.csv", client_world)) {
        close_client_view();
        fprintf(stderr, "Could not load initial world\n");
        return 1;
    }

    int game_result = 0; // 0 = none, 1 = lose, 2 = win

    while (1) {
        draw_client_view_world(client_world, pacman_coord, radius);

        int key = ERR;
        while (key == ERR) {
            int net = poll_network(sock, client_world);
            if (net == -3) {
                if (!wait_world_resync(sock, client_world, pacman_coord)) {
                    game_result = 1;
                    break;
                }
                flushinp();
                redraw_client_view_full(client_world, pacman_coord, radius);
                continue;
            }
            if (net == -1) {
                game_result = 1;
                break;
            }
            if (net == -2) {
                game_result = 2;
                break;
            }
            if (net == 1)
                draw_client_view_world(client_world, pacman_coord, radius);

            key = poll_client_key();
        }

        if (game_result)
            break;

        int result = receive_key(sock, key);
        if (result == -1) {
            game_result = 1;
            break;
        }

        if (result == 1) {
            char prize_path[128];
            int prize_type = 0;
            bool got_prize = false;

            int sync = sync_after_move(sock, client_world, pacman_coord,
                    prize_path, sizeof(prize_path), &prize_type, &got_prize);
            if (sync == 0) {
                if (got_prize)
                    mostrar_premio(prize_path);
                game_result = 1;
                break;
            }
            if (sync == 2) {
                if (got_prize)
                    mostrar_premio(prize_path);
                redraw_client_view_full(client_world, pacman_coord, radius);
                game_result = 2;
                break;
            }
            if (sync < 0) {
                close_client_view();
                fprintf(stderr, "Lost sync with server after move\n");
                return 1;
            }

            if (got_prize)
                mostrar_premio(prize_path);

            redraw_client_view_full(client_world, pacman_coord, radius);

            drain_ghost_updates(sock, client_world);

            move_count++;
            radius = 1 + move_count / 5;
        }
    }

    close_client_view();
    if (game_result == 1)
        printf("Você perdeu\n");
    else if (game_result == 2)
        printf("Você venceu\n");
    return 0;
}
