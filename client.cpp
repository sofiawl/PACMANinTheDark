#include <stdio.h>
#include <sys/socket.h>
#include <ncursesw/ncurses.h>
#include <cstdlib>
#include <unistd.h>
#include <iostream>
#include <string>

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
        // printf("Debug [recv_file] type: %d\n", frame.type);
        // sleep(10);
        if (frame.type == MSG_JPG || frame.type == MSG_TXT || frame.type == MSG_MP4) {
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
    printf("Debug [receive_key] entrou aqui\n");
    //while (1) {
        // draw world and check for a key immediately (non-blocking, wtimeout=0)
        
        // map arrow keys to single bytes the server understands
        // why is it sending always MSG_RIGHT???
        MessageType type; 
        switch (key) {
            case KEY_UP:     type = MSG_UP; break; 
            case KEY_DOWN:   type = MSG_DOWN; break;
            case KEY_LEFT:   type = MSG_LEFT; break;
            case KEY_RIGHT:  type = MSG_RIGHT; break; 
        }
        if(key == 'q' || key == 'Q'){
            type = MSG_END; 
        }
        //printf("Debug [receive_key] type: %d\n", type); 
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


void show(const std::string& nomeArquivo) {
    std::string comando = "xdg-open ./" + nomeArquivo;

    std::cout << "Tentando abrir: " << nomeArquivo << "..." << std::endl;
    
    int resultado = std::system(comando.c_str());

    if (resultado != 0) {
        std::cout << "Erro ao tentar abrir o arquivo. Verifique se o nome está correto." << std::endl;
    }

    sleep(10);
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
            recv_file(sock, &frame, "teste-recv.txt");
            show("teste-recv.txt");

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


