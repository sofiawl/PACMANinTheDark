# PACMAN In The Dark

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
- 
### Server
- 
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

## Action Plan implementation
1. Read RAWsocks stuff
2. Try to send anything to another machine with the network cable
3. we will know what to do next
