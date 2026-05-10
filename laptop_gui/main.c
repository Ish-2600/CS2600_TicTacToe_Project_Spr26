#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "renderers/raylib/clay_renderer_raylib.c"

#define SCREEN_WIDTH 900
#define SCREEN_HEIGHT 700

void updateBoardFromMessage(char board[], const char *message);

typedef struct {
    FILE *pipe;
    pid_t pid;
} MqttSubscriber;

static void handleClayErrors(Clay_ErrorData errorData) {
    printf("%.*s\n", errorData.errorText.length, errorData.errorText.chars);
}

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

void stopMqttSubscriber(MqttSubscriber *subscriber) {
    if (subscriber->pipe != NULL) {
        if (subscriber->pid > 0) {
            kill(subscriber->pid, SIGTERM);
        }

        fclose(subscriber->pipe);

        if (subscriber->pid > 0) {
            waitpid(subscriber->pid, NULL, WNOHANG);
        }

        subscriber->pipe = NULL;
        subscriber->pid = 0;
    }
}

int startMqttSubscriber(MqttSubscriber *subscriber, const char *mqttHost, const char *gameId) {
    char topic[128];
    int pipeFd[2];

    stopMqttSubscriber(subscriber);

    snprintf(topic, sizeof(topic), "ttt/%s/game/#", gameId);

    if (pipe(pipeFd) != 0) {
        return 0;
    }

    pid_t pid = fork();

    if (pid < 0) {
        close(pipeFd[0]);
        close(pipeFd[1]);
        return 0;
    }

    if (pid == 0) {
        int devNull = open("/dev/null", O_WRONLY);

        dup2(pipeFd[1], STDOUT_FILENO);

        if (devNull >= 0) {
            dup2(devNull, STDERR_FILENO);
            close(devNull);
        }

        close(pipeFd[0]);
        close(pipeFd[1]);

        execlp("mosquitto_sub", "mosquitto_sub", "-h", mqttHost, "-t", topic, "-v", NULL);
        _exit(1);
    }

    close(pipeFd[1]);

    subscriber->pipe = fdopen(pipeFd[0], "r");

    if (subscriber->pipe == NULL) {
        close(pipeFd[0]);
        kill(pid, SIGTERM);
        waitpid(pid, NULL, WNOHANG);
        return 0;
    }

    subscriber->pid = pid;

    int fd = fileno(subscriber->pipe);
    int flags = fcntl(fd, F_GETFL, 0);

    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }

    return 1;
}

void handleMqttLine(const char *gameId, const char *line, char board[], char status[], int statusSize) {
    char topic[128];
    char payload[128];
    char boardTopic[128];
    char statusTopic[128];

    if (sscanf(line, "%127s %127[^\n]", topic, payload) != 2) {
        return;
    }

    snprintf(boardTopic, sizeof(boardTopic), "ttt/%s/game/board", gameId);
    snprintf(statusTopic, sizeof(statusTopic), "ttt/%s/game/status", gameId);

    if (strcmp(topic, boardTopic) == 0) {
        updateBoardFromMessage(board, payload);
    } else if (strcmp(topic, statusTopic) == 0) {
        snprintf(status, statusSize, "%s", payload);
    }
}

void pollMqttSubscriber(MqttSubscriber *subscriber, const char *gameId, char board[], char status[], int statusSize) {
    char line[256];

    if (subscriber->pipe == NULL) {
        return;
    }

    while (fgets(line, sizeof(line), subscriber->pipe) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        handleMqttLine(gameId, line, board, status, statusSize);
    }

    clearerr(subscriber->pipe);
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

void drawStatusText(const char *status, int x, int y, int maxCharsPerLine) {
    char line[96];
    int lineLength = 0;
    int lineY = y;

    for (int i = 0; status[i] != '\0'; i++) {
        line[lineLength++] = status[i];

        if (lineLength >= maxCharsPerLine || status[i + 1] == '\0') {
            line[lineLength] = '\0';
            DrawText(line, x, lineY, 18, MAROON);
            lineLength = 0;
            lineY += 24;
        }
    }
}

int main(void) {
    Clay_Raylib_Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, "Tic-Tac-Toe MQTT GUI", 0);
    SetTargetFPS(60);

    uint64_t clayMemorySize = Clay_MinMemorySize();
    void *clayMemory = malloc(clayMemorySize);

    if (clayMemory == NULL) {
        fprintf(stderr, "Could not allocate Clay memory.\n");
        Clay_Raylib_Close();
        return 1;
    }

    Clay_Arena clayArena = Clay_CreateArenaWithCapacityAndMemory(clayMemorySize, clayMemory);

    Clay_Initialize(
        clayArena,
        (Clay_Dimensions){ SCREEN_WIDTH, SCREEN_HEIGHT },
        (Clay_ErrorHandler){ handleClayErrors }
    );

    Font fonts[1] = { GetFontDefault() };
    Clay_SetMeasureTextFunction(Raylib_MeasureText, fonts);

    char mqttHost[128] = "ishserver.duckdns.org";
    char gameId[64] = "ismael";
    char playerRole[8] = "x";
    char board[9] = {'1','2','3','4','5','6','7','8','9'};
    char status[128] = "Edit settings, choose player, then connect";
    int activeInput = 0;
    int connected = 0;
    MqttSubscriber subscriber = {0};

    Rectangle hostBox = {44, 105, 310, 45};
    Rectangle gameBox = {44, 185, 310, 45};
    Rectangle xButton = {44, 265, 70, 42};
    Rectangle oButton = {124, 265, 70, 42};
    Rectangle connectButton = {44, 340, 140, 45};
    Rectangle disconnectButton = {198, 340, 150, 45};

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

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
                    if (startMqttSubscriber(&subscriber, mqttHost, gameId)) {
                        connected = 1;
                    } else {
                        connected = 0;
                        snprintf(status, sizeof(status), "Could not start MQTT subscriber.");
                    }
                } else {
                    connected = 0;
                }
            }

            if (connected && CheckCollisionPointRec(mouse, disconnectButton)) {
                stopMqttSubscriber(&subscriber);
                connected = 0;
                activeInput = 0;
                resetBoard(board);
                snprintf(status, sizeof(status), "Disconnected. Edit settings,  then connect");
            }
        }

        if (!connected && activeInput == 1) {
            handleTextInput(mqttHost, sizeof(mqttHost));
        } else if (!connected && activeInput == 2) {
            handleTextInput(gameId, sizeof(gameId));
        }

        if (connected) {
            pollMqttSubscriber(&subscriber, gameId, board, status, sizeof(status));
        }

        Clay_SetLayoutDimensions((Clay_Dimensions){
            (float)GetScreenWidth(),
            (float)GetScreenHeight()
        });

        Vector2 mouse = GetMousePosition();
        Clay_SetPointerState(
            (Clay_Vector2){ mouse.x, mouse.y },
            IsMouseButtonDown(MOUSE_BUTTON_LEFT)
        );

        Clay_BeginLayout();

        CLAY(CLAY_ID("Root"), {
            .layout = {
                .sizing = {
                    .width = CLAY_SIZING_GROW(0),
                    .height = CLAY_SIZING_GROW(0)
                },
                .padding = CLAY_PADDING_ALL(24),
                .childGap = 20,
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            },
            .backgroundColor = { 248, 248, 246, 255 }
        }) {
            CLAY(CLAY_ID("SettingsPanel"), {
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_FIXED(360),
                        .height = CLAY_SIZING_GROW(0)
                    }
                },
                .backgroundColor = { 236, 240, 243, 255 },
                .cornerRadius = CLAY_CORNER_RADIUS(8)
            }) {}

            CLAY(CLAY_ID("BoardPanel"), {
                .layout = {
                    .sizing = {
                        .width = CLAY_SIZING_GROW(0),
                        .height = CLAY_SIZING_GROW(0)
                    }
                },
                .backgroundColor = { 255, 255, 255, 255 },
                .cornerRadius = CLAY_CORNER_RADIUS(8)
            }) {}
        }

        Clay_RenderCommandArray clayCommands = Clay_EndLayout(deltaTime);

        BeginDrawing();
        Clay_Raylib_Render(clayCommands, fonts);

        DrawText("Tic-Tac-Toe MQTT GUI", 44, 34, 28, BLACK);

        drawTextBox(hostBox, "MQTT Host", mqttHost, activeInput == 1, !connected);
        drawTextBox(gameBox, "Game ID", gameId, activeInput == 2, !connected);

        DrawText("Player", 44, 240, 18, DARKGRAY);

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

        DrawText("Status", 44, 420, 20, DARKGRAY);
        drawStatusText(status, 44, 452, 31);

        DrawText("Board", 500, 88, 24, DARKGRAY);

        int boardX = 500;
        int boardY = 135;
        int cellSize = 105;

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
                    }
                }
            }
        }

        DrawText("Edit settings while disconnected. Connect before playing.", 44, 642, 18, DARKGRAY);

        EndDrawing();
    }

    stopMqttSubscriber(&subscriber);
    Clay_Raylib_Close();
    free(clayMemory);

    return 0;
}
