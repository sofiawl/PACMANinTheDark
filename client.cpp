#include <stdio.h>
#include <sys/socket.h>
#include <ncursesw/ncurses.h>
#include <cstdlib>

#include "protocol.h"
#include "client_view.h"


// implement this on the main
bool recv_file(int sock, Frame *f, const char* name){
    printf("Debug [recv_file] entrou\n");
    FILE* out = fopen(name, "wb");
    if (!out) {
        fprintf(stderr, "Could not open file for writing\n");
        return false;
    }

    Frame frame;
    while (1) {
        int rv = recv_frame(sock, &frame, CLIENT, SERVER, INTERFACE1);
        if (rv < 0) continue;
        printf("Debug [recv_file] type: %d\n", frame.type);
        if (frame.type == MSG_JPG) {
            fwrite(frame.data, 1, frame.size, out);
            continue;
        }
        if (frame.type == MSG_END) {
            printf("Debug [recv_file] msg_end\n");
            break;
        }
    }
    fflush(out);
    fclose(out);
    printf("Debug [recv_file] saida\n");
    return true;
}

bool receive_world(int sock) {
    FILE* out = fopen("mundo.csv", "wb");
    if (!out) {
        fprintf(stderr, "Could not open mundo.csv for writing\n");
        return false;
    }
    Frame frame;
    while (1) {
        int rv = recv_frame(sock, &frame, CLIENT, SERVER, INTERFACE1);
        // trata timeout
        if (rv < 0) {
            send_init(sock);
            continue;
        }

        if (frame.type == MSG_WORLD) {
            fwrite(frame.data, 1, frame.size, out);
            continue;
        }
        if (frame.type == MSG_END) break;
    }
    fflush(out);
    fclose(out);
    return true;
}

int receive_key(int sock, int key){
    //while (1) {
        // draw world and check for a key immediately (non-blocking, wtimeout=0)

        // map arrow keys to single bytes the server understands
        MessageType type;
        switch (key) {
            case KEY_UP:
                type = MSG_UP;
                break;
            case KEY_DOWN:
                type = MSG_DOWN;
                break;
            case KEY_LEFT:
                type = MSG_LEFT;
                break;
            case KEY_RIGHT:
                type = MSG_RIGHT;
                break;
        }
        if(key == 'q' || key == 'Q'){
            type = MSG_END;
        }
        //printf("key recv_frame (%d)\n",key);

        Frame f;
        if (build_frame(&f, 0, type, nullptr, 1) == 0)
            send(sock, &f, CLIENT, SERVER, INTERFACE1);

        if(key == 'q' || key == 'Q'){
            return -1;
        } else {
            return 0;
        }
}


void exibir_premio_txt(const char* nome_arquivo) {
    FILE* arq = fopen(nome_arquivo, "r");
    if (!arq) {
        mvprintw(0, 0, "Erro ao abrir o arquivo de texto: %s", nome_arquivo);
        refresh();
        return;
    }

    clear();
    move(0, 0);

    char linha[256];
    while (fgets(linha, sizeof(linha), arq)) {
        printw("%s", linha);
    }
    fclose(arq);


    mvprintw(LINES - 1, 0, "--- Pressione qualquer tecla para voltar ao jogo ---");
    refresh();

    // is suposed to stop the game
    int c = getch();

    clear();
}

void mostrar_premio(const char* nome_arquivo, int tipo) {
    printf("Debug [mostrar premio] entrou aqui\n");
    if (tipo == 5) {
        exibir_premio_txt(nome_arquivo);
    }
    else if (tipo == 6 || tipo == 7) {
        char comando[512];

        char chmod_cmd[128];
        sprintf(chmod_cmd, "chmod 777 %s", nome_arquivo);
        system(chmod_cmd);

        // faz com que mostre a pag de video
        sprintf(comando, "su $SUDO_USER -c \"xdg-open %s\" &", nome_arquivo);
        system(comando);

        clear();
        mvprintw(LINES / 2, (COLS / 2) - 15, "Prêmio aberto em uma janela externa!");
        mvprintw((LINES / 2) + 2, (COLS / 2) - 20, "Aperte qualquer tecla no terminal para voltar ao jogo...");
        refresh();

        getch();
        clear();
    }
}

int main() {
    int sock = create_raw_socket(INTERFACE1);

    while(1){

        if (send_init(sock) != 0) {
            printf("Debug [main client]: Tempo expirado para receber o mapa inicial.\n");
            return -1;
        }


        if (!receive_world(sock)) return 1;

        init_client_view();

        int key = draw_client_view_and_get_key("mundo.csv");
        if(key != ERR){
            if(receive_key(sock, key) == -1){
                printf("Gameover!\n");
                break;
            }

            Frame frame;
            recv_file(sock, &frame, "1");
            mostrar_premio("1", frame.type);

            //when I build the other functions hopefully this will work better
            // if this is not here it will start building a world on top of the old one
            close_client_view();
        }
    }


    close_client_view();
    printf("Game over\n");
    return 0;
}






/*
Ordem dos eventos:
1. receber mundo
2. scanf movimento
3. recv_file

First message goes to trash

I am not sure how to implement the timeout
It will send init and wait for response if it doesn't come then sends init again

tá recebendo só ack

*/
