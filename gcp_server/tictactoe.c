  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>

  #define SIZE 9

  char board[SIZE];
  char currentPlayer = 'X';

  void publishMessage(const char *topic, const char *message) {
      char command[256];

      snprintf(command, sizeof(command),
               "mosquitto_pub -h localhost -t %s -m \"%s\"",
               topic, message);

      system(command);
  }

  void initializeBoard() {
      for (int i = 0; i < SIZE; i++) {
          board[i] = '1' + i;
      }

      currentPlayer = 'X';
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

      publishMessage("ttt/game/board", message);
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

      publishMessage("ttt/game/available", message);
  }

  void publishTurn() {
      char message[16];

      snprintf(message, sizeof(message), "TURN:%c", currentPlayer);
      publishMessage("ttt/game/status", message);
  }

  void publishGameState() {
      publishBoard();
      publishAvailable();
      publishTurn();
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

  int main() {
      int move;
      int validInput;
      char winnerMessage[16];

      initializeBoard();

      printf("Tic-Tac-Toe with MQTT publishing\n");
      printf("Player X and Player O take turns choosing positions 1-9.\n");

      publishGameState();

      while (1) {
          printBoard();

          printf("Player %c, enter your move (1-9): ", currentPlayer);
          validInput = scanf("%d", &move);

          if (validInput != 1) {
              printf("Invalid input. Please enter a number from 1 to 9.\n");
              publishMessage("ttt/game/status", "INVALID_INPUT");

              while (getchar() != '\n') {
              }

              continue;
          }

          if (move < 1 || move > 9) {
              printf("Invalid move. Choose a number from 1 to 9.\n");
              publishMessage("ttt/game/status", "INVALID_MOVE");
              continue;
          }

          if (board[move - 1] == 'X' || board[move - 1] == 'O') {
              printf("That position is already taken. Choose another space.\n");
              publishMessage("ttt/game/status", "POSITION_TAKEN");
              continue;
          }

          board[move - 1] = currentPlayer;

          publishBoard();
          publishAvailable();

          if (checkWinner()) {
              printBoard();

              snprintf(winnerMessage, sizeof(winnerMessage),
                       "WINNER:%c", currentPlayer);

              publishMessage("ttt/game/status", winnerMessage);

              printf("Player %c wins!\n", currentPlayer);
              break;
          }

          if (boardFull()) {
              printBoard();
              publishMessage("ttt/game/status", "DRAW");
              printf("The game is a draw.\n");
              break;
          }

          if (currentPlayer == 'X') {
              currentPlayer = 'O';
          } else {
              currentPlayer = 'X';
          }

          publishTurn();
      }

      return 0;
  }
