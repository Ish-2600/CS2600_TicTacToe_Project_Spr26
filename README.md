# CS2600 Tic-Tac-Toe MQTT Project

This project is a Tic-Tac-Toe game that uses MQTT.

The game server runs on GCP. Players can send moves from the laptop GUI, the ESP32 keypad, or the Bash random player.

Keep in mind most of these commands are tailered to my computer so edits might be needed.


## Github Link
https://github.com/Ish-2600/CS2600_TicTacToe_Project_Spr26 


## Youtube Demo Link

https://youtu.be/gPoa0Yca0Lw 


## Folders

```text
gcp_server/      Tic-Tac-Toe game server in C
scripts/         Bash random player
laptop_gui/      raylib + Clay laptop GUI
esp32_client/    ESP32 keypad/LCD MQTT client
docs/            Extra documentation
```

## MQTT Info

Broker:

```text
YOURSERVER.duckdns.org
```

Game ID:

```text
ismael
```

Main topics:

```text
ttt/ismael/game/status
ttt/ismael/game/board
ttt/ismael/game/available
ttt/ismael/player/x/move
ttt/ismael/player/o/move
```

## Run GCP Game Server

In the GCP terminal:

```bash
cd ~/CS2600_TicTacToe_Project_Spr26/gcp_server
gcc tictactoe.c -o tictactoe
./tictactoe
```

## Run Bash Random Player

In another GCP terminal:

```bash
cd ~/CS2600_TicTacToe_Project_Spr26/scripts
./random_player.sh
```

The Bash player plays as `O`. (Needs to be changed in the script to change player)

## Run Laptop GUI

On the regular computer:

```bash
cd /Users/ChangeUser/GIT/tic_tac_toe/laptop_gui
gcc main.c -o tictactoe_gui $(pkg-config --libs --cflags raylib)
./tictactoe_gui
```

Use:

```text
MQTT Host: ishserver.duckdns.org
Game ID: ismael
Player: x or o
```

## ESP32

Open this sketch in Arduino IDE:

```text
esp32_client/esp32_tictactoe_client/esp32_tictactoe_client.ino
```

Before uploading, update the Wi-Fi name and password in the sketch.

Controls:

```text
A = play as X
B = play as O
1-9 = send move
C = reconnect MQTT
D = show current player
```