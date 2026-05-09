 #include <stdio.h>

 #define SIZE 9

  void initializeBoard(char board[]) {
      for (int i = 0; i < SIZE; i++) {
          board[i] = '1' + i;
      }
  }

  void printBoard(char board[]) {
      printf("\n");
      printf(" %c | %c | %c\n", board[0], board[1], board[2]);
      printf("---+---+---\n");
      printf(" %c | %c | %c\n", board[3], board[4], board[5]);
      printf("---+---+---\n");
      printf(" %c | %c | %c\n", board[6], board[7], board[8]);
      printf("\n");
  }

  int checkWinner(char board[]) {
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

  int boardFull(char board[]) {
      for (int i = 0; i < SIZE; i++) {
          if (board[i] != 'X' && board[i] != 'O') {
              return 0;
          }
      }

      return 1;
  }

  int main() {
      char board[SIZE];
      char currentPlayer = 'X';
      int move;
      int validInput;

      initializeBoard(board);

      printf("Tic-Tac-Toe\n");
      printf("Player X and Player O take turns choosing positions 1-9.\n");

      while (1) {
          printBoard(board);

          printf("Player %c, enter your move (1-9): ", currentPlayer);
          validInput = scanf("%d", &move);

          if (validInput != 1) {
              printf("Invalid input. Please enter a number from 1 to 9.\n");

              while (getchar() != '\n') {
                  // Clear invalid input
              }

              continue;
          }

          if (move < 1 || move > 9) {
              printf("Invalid move. Choose a number from 1 to 9.\n");
              continue;
          }

          if (board[move - 1] == 'X' || board[move - 1] == 'O') {
              printf("That position is already taken. Choose another space.\n");
              continue;
          }

          board[move - 1] = currentPlayer;

          if (checkWinner(board)) {
              printBoard(board);
              printf("Player %c wins!\n", currentPlayer);
              break;
          }

          if (boardFull(board)) {
              printBoard(board);
              printf("The game is a draw.\n");
              break;
          }

          if (currentPlayer == 'X') {
              currentPlayer = 'O';
          } else {
              currentPlayer = 'X';
          }
      }

      return 0;
  }
