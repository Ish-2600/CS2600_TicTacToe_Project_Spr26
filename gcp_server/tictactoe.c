  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>

  #define SIZE 9

  char board[SIZE];
  char currentPlayer = 'X';
  int gameOver = 0;
  char gameId[64] = "ismael";

  void publishMessage(const char *topicSuffix, const char *message) {
      char topic[128];
      char command[256];

      snprintf(topic, sizeof(topic), "ttt/%s/%s", gameId, topicSuffix);

      snprintf(command, sizeof(command),
               "mosquitto_pub -r -h localhost -t %s -m \"%s\"",
               topic, message);

      system(command);
  }

  void initializeBoard() {
      for (int i = 0; i < SIZE; i++) {
          board[i] = '1' + i;
      }

      currentPlayer = 'X';
      gameOver = 0;
  }

  void printBoard() {
      printf("\n");
      printf(" %c | %c | %c\n", board[0], board[1], board[2]);
      printf("---+---+---\n");
      printf(" %c | %c | %c\n", board[3], board[4], board[5]);
      printf("---+---+---\n");
      printf(" %c | %c | %c\n", board[6], board[7], board[8]);
      printf("\n");
  }

  void publishBoard() {
      char message[64];

      snprintf(message, sizeof(message),
               "%c,%c,%c,%c,%c,%c,%c,%c,%c",
               board[0], board[1], board[2],
               board[3], board[4], board[5],
               board[6], board[7], board[8]);

      publishMessage("game/board", message);
  }

  void publishAvailable() {
      char message[64] = "";
      char temp[8];
      int first = 1;

      for (int i = 0; i < SIZE; i++) {
          if (board[i] != 'X' && board[i] != 'O') {
              if (!first) {
                  strcat(message, ",");
              }

              snprintf(temp, sizeof(temp), "%d", i + 1);
              strcat(message, temp);
              first = 0;
          }
      }

      publishMessage("game/available", message);
  }

  void publishTurn() {
      char message[16];

      snprintf(message, sizeof(message), "TURN:%c", currentPlayer);
      publishMessage("game/status", message);
  }

  void publishGameState() {
      publishBoard();

      if (!gameOver) {
          publishTurn();
      }

      publishAvailable();
      printBoard();
  }

  int checkWinner() {
      int wins[8][3] = {
          {0, 1, 2},
          {3, 4, 5},
          {6, 7, 8},
          {0, 3, 6},
          {1, 4, 7},
          {2, 5, 8},
          {0, 4, 8},
          {2, 4, 6}
      };

      for (int i = 0; i < 8; i++) {
          int a = wins[i][0];
          int b = wins[i][1];
          int c = wins[i][2];

          if (board[a] == board[b] && board[b] == board[c]) {
              return 1;
          }
      }

      return 0;
  }

  int boardFull() {
      for (int i = 0; i < SIZE; i++) {
          if (board[i] != 'X' && board[i] != 'O') {
              return 0;
          }
      }

      return 1;
  }

  int getMoveFromMqtt(char player) {
      char command[256];
      char buffer[32];

      snprintf(command, sizeof(command),
               "mosquitto_sub -h localhost -t ttt/%s/player/%c/move -C 1",
               gameId,
               player == 'X' ? 'x' : 'o');

      FILE *pipe = popen(command, "r");

      if (pipe == NULL) {
          printf("Could not read MQTT move.\n");
          return -1;
      }

      if (fgets(buffer, sizeof(buffer), pipe) == NULL) {
          pclose(pipe);
          return -1;
      }

      pclose(pipe);

      return atoi(buffer);
  }

  void processMove(int move) {
      char message[32];

      if (move < 1 || move > 9) {
          publishMessage("game/status", "INVALID_MOVE");
          printf("Invalid move received.\n");
          publishGameState();
          return;
      }

      if (board[move - 1] == 'X' || board[move - 1] == 'O') {
          publishMessage("game/status", "POSITION_TAKEN");
          printf("Position already taken.\n");
          publishGameState();
          return;
      }

      board[move - 1] = currentPlayer;

      if (checkWinner()) {
          gameOver = 1;

          publishBoard();
          publishAvailable();

          snprintf(message, sizeof(message), "WINNER:%c", currentPlayer);
          publishMessage("game/status", message);

          printBoard();
          printf("Player %c wins!\n", currentPlayer);
          return;
      }

      if (boardFull()) {
          gameOver = 1;

          publishBoard();
          publishAvailable();
          publishMessage("game/status", "DRAW");

          printBoard();
          printf("The game is a draw.\n");
          return;
      }

      if (currentPlayer == 'X') {
          currentPlayer = 'O';
      } else {
          currentPlayer = 'X';
      }

      publishGameState();
  }

  int main() {
      int move;

      initializeBoard();

      printf("Tic-Tac-Toe MQTT Server\n");
      printf("Game ID: %s\n", gameId);
      printf("Waiting for moves from MQTT.\n");
      printf("X moves topic: ttt/%s/player/x/move\n", gameId);
      printf("O moves topic: ttt/%s/player/o/move\n", gameId);

      publishGameState();

      while (!gameOver) {
          printf("Waiting for Player %c move...\n", currentPlayer);

          move = getMoveFromMqtt(currentPlayer);

          printf("Received move: %d\n", move);

          processMove(move);
      }

      printf("Game over. Restart the program to play again.\n");

      return 0;
  }
