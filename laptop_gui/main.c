  #include "raylib.h"
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>

  #define SCREEN_WIDTH 900
  #define SCREEN_HEIGHT 700

  void publishMove(const char *mqttHost, const char *gameId, const char *playerRole, int move) {
      char command[512];

      snprintf(command, sizeof(command),
               "mosquitto_pub -h %s -t ttt/%s/player/%s/move -m \"%d\"",
               mqttHost, gameId, playerRole, move);

      system(command);
  }

  int readMqttMessage(const char *mqttHost, const char *topic, char *output, int outputSize) {
      char command[512];
      FILE *pipe;

      snprintf(command, sizeof(command),
               "mosquitto_sub -h %s -t %s -C 1 -W 1",
               mqttHost, topic);

      pipe = popen(command, "r");

      if (pipe == NULL) {
          return 0;
      }

      if (fgets(output, outputSize, pipe) == NULL) {
          pclose(pipe);
          return 0;
      }

      pclose(pipe);

      output[strcspn(output, "\n")] = '\0';

      return 1;
  }

  void updateBoardFromMessage(char board[], const char *message) {
      int index = 0;

      for (int i = 0; message[i] != '\0' && index < 9; i++) {
          if (message[i] != ',') {
              board[index] = message[i];
              index++;
          }
      }
  }

  void updateFromMqtt(const char *mqttHost, const char *gameId, char board[], char status[], int
  statusSize) {
      char topic[128];
      char message[128];

      snprintf(topic, sizeof(topic), "ttt/%s/game/board", gameId);

      if (readMqttMessage(mqttHost, topic, message, sizeof(message))) {
          updateBoardFromMessage(board, message);
      }

      snprintf(topic, sizeof(topic), "ttt/%s/game/status", gameId);

      if (readMqttMessage(mqttHost, topic, message, sizeof(message))) {
          snprintf(status, statusSize, "%s", message);
      }
  }

  int main(void) {
      InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Tic-Tac-Toe MQTT GUI");
      SetTargetFPS(60);

      char mqttHost[128] = "ishserver.duckdns.org";
      char gameId[64] = "ismael";
      char playerRole[8] = "x";
      char board[9] = {'1','2','3','4','5','6','7','8','9'};
      char status[128] = "Waiting for MQTT game state";
      double lastMqttUpdate = 0.0;

      while (!WindowShouldClose()) {
          double now = GetTime();

          if (now - lastMqttUpdate >= 0.25) {
              updateFromMqtt(mqttHost, gameId, board, status, sizeof(status));
              lastMqttUpdate = now;
          }

          BeginDrawing();
          ClearBackground(RAYWHITE);

          DrawText("Tic-Tac-Toe MQTT GUI", 40, 30, 32, BLACK);

          DrawText("MQTT Host:", 40, 90, 20, DARKGRAY);
          DrawText(mqttHost, 190, 90, 20, BLUE);

          DrawText("Game ID:", 40, 125, 20, DARKGRAY);
          DrawText(gameId, 190, 125, 20, BLUE);

          DrawText("Player:", 40, 160, 20, DARKGRAY);
          DrawText(playerRole, 190, 160, 20, BLUE);

          DrawText("Status:", 40, 205, 20, DARKGRAY);
          DrawText(status, 190, 205, 20, MAROON);

          int boardX = 250;
          int boardY = 280;
          int cellSize = 110;

          for (int row = 0; row < 3; row++) {
              for (int col = 0; col < 3; col++) {
                  int index = row * 3 + col;
                  int x = boardX + col * cellSize;
                  int y = boardY + row * cellSize;

                  DrawRectangleLines(x, y, cellSize, cellSize, BLACK);

                  char text[2];
                  text[0] = board[index];
                  text[1] = '\0';

                  int fontSize = 48;
                  int textWidth = MeasureText(text, fontSize);

                  DrawText(
                      text,
                      x + (cellSize - textWidth) / 2,
                      y + 30,
                      fontSize,
                      BLACK
                  );

                  if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                      Vector2 mouse = GetMousePosition();

                      if (mouse.x >= x && mouse.x <= x + cellSize &&
                          mouse.y >= y && mouse.y <= y + cellSize) {
                          int move = index + 1;

                          publishMove(mqttHost, gameId, playerRole, move);

                          snprintf(status, sizeof(status),
                                   "Sent move %d as Player %s", move, playerRole);

                          updateFromMqtt(mqttHost, gameId, board, status, sizeof(status));
                      }
                  }
              }
          }

          DrawText("GUI sends moves and reads board/status from MQTT.", 40, 640, 18, DARKGRAY);

          EndDrawing();
      }

      CloseWindow();

      return 0;
  }
