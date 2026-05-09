#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 700

void updateBoardFromMessage(char board[], const char *message);

void publishMove(const char *mqttHost, const char *gameId, const char *playerRole, int move) {
    char command[512];

    snprintf(command, sizeof(command),
             "mosquitto_pub -h %s -t ttt/%s/player/%s/move -m \"%d\" 2>/dev/null",
             mqttHost, gameId, playerRole, move);

    system(command);
}

int readMqttMessage(const char *mqttHost, const char *topic, char *output, int outputSize) {
    char command[512];
    FILE *pipe;

    snprintf(command, sizeof(command),
             "mosquitto_sub -h %s -t %s -C 1 -W 1 2>/dev/null",
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

int checkConnection(const char *mqttHost, const char *gameId, char board[], char status[], int statusSize) {
    char topic[128];
    char message[128];
    int foundBoard = 0;
    int foundStatus = 0;

    snprintf(topic, sizeof(topic), "ttt/%s/game/board", gameId);

    if (readMqttMessage(mqttHost, topic, message, sizeof(message))) {
        updateBoardFromMessage(board, message);
        foundBoard = 1;
    }

    snprintf(topic, sizeof(topic), "ttt/%s/game/status", gameId);

    if (readMqttMessage(mqttHost, topic, message, sizeof(message))) {
        snprintf(status, statusSize, "%s", message);
        foundStatus = 1;
    }

    if (!foundBoard && !foundStatus) {
        snprintf(status, statusSize,
                 "Could not find broker/game ID. Check host, game ID, and server.");
        return 0;
    }

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

void resetBoard(char board[]) {
    for (int i = 0; i < 9; i++) {
        board[i] = '1' + i;
    }
}

void updateFromMqtt(const char *mqttHost, const char *gameId, char board[], char status[], int statusSize) {
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

void drawTextBox(Rectangle box, const char *label, const char *text, int active, int enabled) {
    Color borderColor = active ? BLUE : GRAY;
    Color fillColor = RAYWHITE;
    Color textColor = enabled ? BLACK : DARKGRAY;

    if (active) {
        fillColor = Fade(SKYBLUE, 0.25f);
    } else if (!enabled) {
        fillColor = Fade(LIGHTGRAY, 0.35f);
    }

    DrawText(label, (int)box.x, (int)box.y - 24, 18, DARKGRAY);
    DrawRectangleRec(box, fillColor);
    DrawRectangleLinesEx(box, 2, borderColor);
    DrawText(text, (int)box.x + 10, (int)box.y + 10, 20, textColor);
}

void handleTextInput(char *text, int maxLength) {
    int key = GetCharPressed();

    while (key > 0) {
        if ((key >= 32) && (key <= 126) && ((int)strlen(text) < maxLength - 1)) {
            int length = strlen(text);
            text[length] = (char)key;
            text[length + 1] = '\0';
        }

        key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
        int length = strlen(text);

        if (length > 0) {
            text[length - 1] = '\0';
        }
    }
}

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Tic-Tac-Toe MQTT GUI");
    SetTargetFPS(60);

    char mqttHost[128] = "ishserver.duckdns.org";
    char gameId[64] = "ismael";
    char playerRole[8] = "x";
    char board[9] = {'1','2','3','4','5','6','7','8','9'};
    char status[128] = "Edit settings, choose player, then connect";
    double lastMqttUpdate = 0.0;

    int activeInput = 0;
    int connected = 0;

    Rectangle hostBox = {40, 95, 330, 45};
    Rectangle gameBox = {40, 170, 330, 45};
    Rectangle xButton = {40, 245, 70, 42};
    Rectangle oButton = {120, 245, 70, 42};
    Rectangle connectButton = {40, 315, 150, 45};
    Rectangle disconnectButton = {205, 315, 150, 45};

    while (!WindowShouldClose()) {
        double now = GetTime();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Vector2 mouse = GetMousePosition();

            if (!connected && CheckCollisionPointRec(mouse, hostBox)) {
                activeInput = 1;
            } else if (!connected && CheckCollisionPointRec(mouse, gameBox)) {
                activeInput = 2;
            } else {
                activeInput = 0;
            }

            if (!connected && CheckCollisionPointRec(mouse, xButton)) {
                strcpy(playerRole, "x");
            }

            if (!connected && CheckCollisionPointRec(mouse, oButton)) {
                strcpy(playerRole, "o");
            }

            if (!connected && CheckCollisionPointRec(mouse, connectButton)) {
                activeInput = 0;
                snprintf(status, sizeof(status), "Checking broker and game ID...");
                resetBoard(board);

                if (checkConnection(mqttHost, gameId, board, status, sizeof(status))) {
                    connected = 1;
                    lastMqttUpdate = now;
                } else {
                    connected = 0;
                }
            }

            if (connected && CheckCollisionPointRec(mouse, disconnectButton)) {
                connected = 0;
                activeInput = 0;
                resetBoard(board);
                snprintf(status, sizeof(status), "Disconnected. Edit settings, then connect");
            }
        }

        if (!connected && activeInput == 1) {
            handleTextInput(mqttHost, sizeof(mqttHost));
        } else if (!connected && activeInput == 2) {
            handleTextInput(gameId, sizeof(gameId));
        }

        if (connected && now - lastMqttUpdate >= 0.35) {
            updateFromMqtt(mqttHost, gameId, board, status, sizeof(status));
            lastMqttUpdate = now;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("Tic-Tac-Toe MQTT GUI", 40, 30, 32, BLACK);

        drawTextBox(hostBox, "MQTT Host", mqttHost, activeInput == 1, !connected);
        drawTextBox(gameBox, "Game ID", gameId, activeInput == 2, !connected);

        DrawText("Player", 40, 220, 18, DARKGRAY);

        DrawRectangleRec(xButton, strcmp(playerRole, "x") == 0 ? BLUE : LIGHTGRAY);
        DrawText("X", (int)xButton.x + 26, (int)xButton.y + 10, 22,
                 strcmp(playerRole, "x") == 0 ? WHITE : BLACK);

        DrawRectangleRec(oButton, strcmp(playerRole, "o") == 0 ? BLUE : LIGHTGRAY);
        DrawText("O", (int)oButton.x + 26, (int)oButton.y + 10, 22,
                 strcmp(playerRole, "o") == 0 ? WHITE : BLACK);

        DrawRectangleRec(connectButton, connected ? LIGHTGRAY : GREEN);
        DrawText("Connect", (int)connectButton.x + 32, (int)connectButton.y + 12, 20,
                 connected ? DARKGRAY : WHITE);

        DrawRectangleRec(disconnectButton, connected ? MAROON : LIGHTGRAY);
        DrawText("Disconnect", (int)disconnectButton.x + 20, (int)disconnectButton.y + 12, 20,
                 connected ? WHITE : DARKGRAY);

        DrawText("Status:", 40, 395, 20, DARKGRAY);
        DrawText(status, 130, 395, 20, MAROON);

        int boardX = 470;
        int boardY = 150;
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

                DrawText(text, x + (cellSize - textWidth) / 2, y + 30, fontSize, BLACK);

                if (connected && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
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

        DrawText("Edit settings while disconnected. Connect before playing.", 40, 640, 18, DARKGRAY);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
