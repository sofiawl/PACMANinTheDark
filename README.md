# PACMAN In The Dark

## How to run

###
Building the game interface with [ncurses](https://github.com/mcdaniel/curses_tutorial)

So, before running, you should install the necessary packge:

```
sudo apk update
sudo apk install libncurses-dev libncursesw5-dev
make clean && make
```

### On the client:

*You should start the client first*

First:
```
// Get your pc's interface name 
ip link show

// Fill in this information in client.cpp
```

Then:
``` 
g++ client.cpp protocol.cpp -o server
sudo ./client
``` 

### On the server:

First:
```
// Get your pc's interface name 
ip link show

// Then get your pc's MAC (the server)
ip link show (your interface name)

// Get your other pc's MAC (the client)
ip link show (your interface name)

// Fill in those informations in server.cpp
```

Notice: if nothing like *en*something appears in the first command, you should plug the network cable before and then try it again!

Then:
``` 
g++ server.cpp protocol.cpp -o server
sudo ./server
```

PS: we can make an especific make client/ make server to run

## Docs
[RAWsocket, basics with Todt](https://wiki.inf.ufpr.br/todt/doku.php?id=raw_socket)

[RAWsocket, hardcore with Maziero](https://wiki.inf.ufpr.br/maziero/doku.php?id=pua:comunicacao_em_rede&s[]=socket)

## The game
### World
- entry archive: *mundo.csv*
    1. 40x40 world
    2. format:   
              0: Empty
              X: Wall
              P: Pacman
              R: Red ghost position
              B: Blue ghost position
              G: Green ghost position
              Y: Yellow ghost position
              1 ... 6: pills positions
    3. the informations are seperated by a coma:
          0   1   2   ...   39
        0 X   X   X         X
        1 X   P   B
        2 X   0   0         X
        .
        . 
        .
       39 X   X
    
### Ghosts
- Client movimentation (The Pacman). Can we use top, down, right and left arrow pleaaaaseee?
- Ghosts movimentation, the server:
    1. Red: go foward, if hits the wall go left
    2. Blue: go foward, if hits the wall go right
    3. Green: go foward, if hits the wall alternate between left and right
    4. Yellow: aleatory X>
     
### Pills
- Each pill that pacman colects a file is sent by server to the client. 
- THE FILE MUST BE SEEN BY THE USER IN CLIENT, so there must be a video visualizer, a mp4 player...
- The pills: 
    1. 1.txt
    2. 2.txt
    3. 3.jpg
    4. 4.jpg
    5. 5.jpg
    6. 6.jpp
- Please test with really big files, the professors will do that

### Visualization                                        
- Initially: Client can see one space for each side:
    S   S   S
    S   P   S
    S   S   S
- Every *5 movimentations* increases one space

### The end game
- The game ends when PACMAN gets every pills
- The game ends when PACMAN hits a ghost???? maybe easier

## Different views
### Client
- Do not store anything
- Is the machine where the user digits the movement
- Create a message and send it to the server, waits the confirmation message to send something else
- Receve message with new pacman vision
- Show vision on the screen
- Can recive only one file at a time and show it at the same time
- *Can show a window on the side with the history of messages between sever and client*
    - Better for debuging and work presentation
     
### Server
- Create and load world
- Conects client
- Send initial visualization
- Wait to receve movement from client
    - Calculate ghost's movements
    - Send new visualization to the client
    - If the client finds a pill -> send file
- Do not have contact with the user
- Takes care of:
    - Pacman's death
    - Pacman's hit the wall

## Protocol
| init marker |   size   |   sequence   |   type   |   data   |    CRC    |

|     8bits   |  5bits   |    6bits     |   5byts  |  31bytes |   8bits   |

- init marker: *01111110*
- size: data 0 to 31 bytes
    - there can't be more than one message to send move pacman
    - there can be more than one message to send visualization (when it starts to get bigger we will have to divide them into smaller pices)
- sequence: 0 to 63 messages
    - It is a loop, there will be times we will have to the begining of it again to send big stuff: 0 1 2 3 ... 62 63 0 1 2
- CRC: [Use this, but they told us there is a wrong line somewhere](https://en.wikipedia.org/wiki/Cyclic_redundancy_check)
- type: i did't get totally if the type are the options of message movimentation...
    - 0: ack  
    - 1: anck
    - 2: visualization / central position (send thought lines in data then arrange then with sequence)
    - 3: init world
    - 4: data (pieces of the files)
    - 5: txt
    - 6: jpg
    - 7: mp4
    - 8:
    - 9:
    - 10: move right (we can send the visualization WITH the move?)
    - 11: move left
    - 12: move up
    - 13: move down
    - 14: 
    - 15: errors (don't have write permition, there is no space to write) / IF THE FILE ALREADY EXISTS OVERWRITE IT
    - 16: end of transmition

- Todt tip: he told us that 4bytes at a time is a good size to send a file
- PS: the first conection is different, because who starts the communication is the server
- Timeout: ???
 
#### Bonus
- 10% if we implement sliding window of size 5 to send the files

## Final Report
- [ ] Decisions
    - Timeout: why this was the timeout choosen?
    - When pacman dies: the game end? it has more lifes?
    - pills positions is aleatory?
    - what happens when pacman hits the wall?
- [ ] it must be printed for the presentation!

## Everything Explained, to us dummies

### Ethernet frame vs Protocole
[ dest MAC | src MAC | EtherType | payload | CRC ]

   6 bytes    6 bytes    2 bytes   up to 1500b  4b
   
[ dest MAC | src MAC | EtherType |  [ our protocol message ]   | CRC ]
                                
                                  
                                  
1. We call sendto()
2. ethernet frame travels through cable
3. receiver gets the whole frame
4. receiver reads payload → finds our protocol message inside

it is kind of a letter, there is a sender and a receiver, the content of the letter (out data), than a check of the content (the CRC)

#### CRC: Cyclic Redundancy Check
It is a detecting error mechanism in 32 bits, frame check sequence, added at the end of each ethernet frame to ensure data quality

how does it work?
The sender calculates the CRC based on the content of the frame and anex it to the end.
The receptor redo the calculus, if the value isn't the same the content is discarted

- ethernet frame CRC
    - The network card calculates and adds it by itself when sending
    - It's invisible to us (the machine already does it)
    - It protects againts physican errors: cable noise, eletrical interference, bit flips
- our protocole CRC
    - We have to write the code that calculates and checks it
    - It protects againts local error: bugs in our code, wrong message assembly, corrupted data in memory


#### A vector of a matrix, that would be so cool
Soooo, instead of sending each line of a matrix
We just would send ONE LINE that would contain the whole matrix

But how??? Just like i did in world.cpp

The cons are: 
- processing it, but since it is only a 40x40 matrix i think it is worth it
- Do we need to do that to with the archives?? Theeeen this will be a problem. But no, cus of the type in protocol.


#### Unfortunatly had to use 'w', 'a', 's', 'd' to move Pacman and here is why
KEY_UP=259, cast to uint8_t = 3 -> it made an overflow

Server passes f.data[0]=3 to move_pacman, whose switch compares
  against KEY_UP=259 — never matches → Pacman never moves.

#### Making the ghosts move independly 
Before, the ghosts were only moving when pacman moved. Because the client was only sending message to the server when it pressed a key, then the server moved the pacman and updated.

But now, the server has it's one timeclock that moves the ghosts accordingly to his game. The server keeps sending this update world messages to the client so that it updates the view. And if the client clicks a key, then the server receives the key that was pressed, move pacman and send world atualized to the client.

before:
```
    Client                    Server
                            init_world
    receive world <-------- send_world
    
    [begin loop]
    draw world
    
    if key pressed:
    send key  ──────────────>  recv_frame() gets key
                               move_pacman(key)                                    
    receive_world() <───────── send world              ← Pacman update

                                move_ghosts()                           |  This happens every 300ms,    
    receive_world() <────────── send_world()           ← ghost update   |  independently on pacman move.
                                                          
    draw_world()
    [repeat]
```

Everytime the client wants to update the world it sends a message to the server with MSG_INIT, the server does not send anything automatically, it only sends what is asked. And the client always ask for the world in the beginning of the loop. 
now:
```
    Client                        Server
    [begin loop]                  
    init_world    -------->                  
    receive world <--------     send_world
    
    draw world
    
    if key pressed:
    send key  ──────────────>  recv_frame() gets key
                               move_pacman(key)                                    
    receive_file() <───────── send file             

                                move_ghosts()                          
                                                          
    draw_world()
    [repeat]
```  
The socket has a kernel receive buffer. When the server sends two worlds back-to-back, both land in that buffer:

```
  Socket buffer:  [TXT][TXT]...[END]         [TXT][TXT]...[END]
                  receive world 1 ──────►  receive world 2 ──────►
                  
``` 

receive_world reads frames one by one until it hits MSG_END, then returns. World 2 stays untouched in the buffer. On the next loop iteration the client calls receive_world again and finds world 2 already waiting, it reads it immediately without blocking.

So the client processes them sequentially, one per loop iteration:
- Loop iteration 1:  receive world 1 (Pacman moved) → draw → check key
- Loop iteration 2:  receive world 2 (ghosts moved)  → draw → check key

Review this logic in the future, when we are sending archives that are really big and have many messages to send them to the client at the same time. How the buffer will handle??


#### Thing sofia must add
[x] UFPR page is the standard map where the U F P and R are walls that the ghosts and pacman hit. If the client wants they can add another map and then the UPFR map is not used.

[ ] EndGame page, is really necessary?????

[ ] Vision expansion grr

[x] Protection to other keyboard selection from client

[ ] pills 
 
#### Thing Helena must add
[x] Ack / nack

[x] Sincronize client and server 

[x] Start with the client 

[ ] Timeout 

[ ] Check sequence 

[ ] Send big files

[ ] Implement pills 

[ ] Show the files for the client  

[ ] Error handling 

[x] EndGame send throught both ways. If the pacman dies server tells client and client stop the running. If client clicks q client tells server and server stop working

[ ] If pacman hits a ghost send MESG_END

[ ] Log

And then, if everything works perfectly:
[ ] Janelas deslizantes 

## How to do loopback 
- Cria o par de interfaces virtuais (veth0 e veth1)
``` 
sudo ip link add veth0 type veth peer name veth1
``` 

- Ativa as interfaces
``` 
sudo ip link set veth0 up
sudo ip link set veth1 up
``` 

- (Opcional) Atribuir IPs apenas para facilitar, embora o RAWSocket possa usar o endereço MAC
``` 
sudo ip addr add 10.0.0.1/24 dev veth0
sudo ip addr add 10.0.0.2/24 dev veth1
``` 

#### Thing Helena changed 
- Message type 
    - when server sends world = MSG_WORLD (14) - maybe it could also be MSG_VIS I am not sure
    - when client sends pressed key = MSG_UP, MSG_DOWN, MSG_LEFT or MSG_DOWN - nothing in the data part

- Server is always waiting 
    - The server should be the one that starts first, to do that the client sends a frame that tells the server that the client has logged in and wants to play. The frame type is MSG_INIT

#### Things to check
- Bitmask: 

    sudo tcpdump -i veth0 -v > /tmp/log-int.txt &

    runs the game 

    more /tmp/log-int.txt -> here there is everything that is going throught the computers

    ps -ef | grep tcpdump
    sudo kill [number]

- how much time should the receive wait?

- UFPR map
    I am not sure how to implement that but I belive the user should be able to insert a map and if that does not happen then the standart map is the UFPR one. 

- when pacman hits a ghost must also send a package
