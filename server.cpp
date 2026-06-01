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

#include "protocol.h"
#include "world.h"


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
        uint8_t buffer[DATA_SIZE];
        size_t bytes_read = fread(buffer, 1, DATA_SIZE, file);
        if (bytes_read == 0) break;
        if (build_frame(&f_send, seq, type, buffer, (uint8_t)bytes_read) != 0) break;
        if (send(sock, &f_send, SERVER, CLIENT, INTERFACE_SERVER) < 0) break;
        if (++seq > 63) seq = 0;
    }
    fclose(file);

    if (build_frame(&f_send, seq, MSG_END, nullptr, 0) == 0)
        send(sock, &f_send, SERVER, CLIENT, INTERFACE_SERVER);

    return 0;
}

static void send_game_over(int sock) {
    Frame f_send;
    if (build_frame(&f_send, 0, MSG_OVER, nullptr, 0) == 0)
        send_frame(sock, &f_send, SERVER, CLIENT, INTERFACE_SERVER);
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
        if (send_frame(sock, &f_send, SERVER, CLIENT, INTERFACE_SERVER) < 0) break;
        seq++;
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

int main() {
    bool start = false;
    bool transmitting = false;

    int sock = create_raw_socket(INTERFACE_SERVER);

    struct timeval tv = {0, 10000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char world[SIZE_WORLD][SIZE_WORLD];
    std::pair<int, int> pacman_coord = {{(SIZE_WORLD - 1) / 2}, {(SIZE_WORLD - 1) / 2}};
    bool green_go_left  = false;
    bool red_going_right = true;
    bool blue_going_up   = true;

    std::vector<std::pair<int, int>> ghost_coords;
    std::vector<PillInfo> pills;
    Frame f;
    auto last_tick = std::chrono::steady_clock::now();
    const auto ghost_interval = std::chrono::milliseconds(300);

    printf("Server ready. Waiting for client... (run: sudo ./client)\n");
    fflush(stdout);

    while (1) {
        if (recv_frame(sock, &f, SERVER, CLIENT, INTERFACE_SERVER) >= 0) {
            if (transmitting)
                continue;

            // MSG_END is sent by us after world/file transfer — do not treat it as game over.
            if (f.type == MSG_END)
                continue;

            if (f.type == MSG_INIT) {
                if (!start) {
                    uint8_t world_map = f.data[0];
                    ghost_coords = init_world(world, pacman_coord, world_map, pills);
                    start = true;
                }
                send_world(sock, world);
                continue;
            }

            if ((f.type == MSG_UP || f.type == MSG_DOWN || f.type == MSG_LEFT || f.type == MSG_RIGHT) && start) {
                int mv = move_pacman(world, pacman_coord, f.type);
                if (mv == -1) {
                    send_file("pills/GameOver.mp4", sock);
                    send_game_over(sock);
                    break;
                }

                if (mv >= '1' && mv <= '6') {
                    const PillInfo *pill = find_pill_by_id(pills, (char)mv);
                    if (pill) {
                        transmitting = true;
                        send_file(pill->file_path, sock);
                        remove_pill(pills, pill->id);
                    }
                    send_world(sock, world);
                    if (pills.empty()) {
                        send_file("pills/YouWin.mp4", sock);
                        send_game_over(sock);
                        transmitting = false;
                        break;
                    }
                    transmitting = false;
                } else if (mv == 0) {
                    send_world(sock, world);
                }
                continue;
            }

            if (f.type == MSG_OVER && start)
                break;
            continue;
        }

        if (!transmitting && start) {
            auto now = std::chrono::steady_clock::now();
            if (now - last_tick >= ghost_interval) {
                last_tick = now;
                if (move_ghosts(world, ghost_coords, pacman_coord, green_go_left, red_going_right, blue_going_up) == -1) {
                    send_file("pills/GameOver.mp4", sock);
                    send_game_over(sock);
                    break;
                }
                send_ghost_positions(sock, ghost_coords);
            }
        }
    }

    printf("Game over\n");
    return 0;
}
