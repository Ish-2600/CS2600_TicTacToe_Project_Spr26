#!/bin/bash

  MQTT_HOST="localhost"
  STATUS_TOPIC="ttt/game/status"
  AVAILABLE_TOPIC="ttt/game/available"
  MOVE_TOPIC="ttt/player/o/move"

  echo "Random player started as O."
  echo "Waiting for TURN:O..."

  mosquitto_sub -h "$MQTT_HOST" -t "$STATUS_TOPIC" | while read STATUS
  do
      echo "Status received: $STATUS"

      if [ "$STATUS" = "TURN:O" ]; then
          AVAILABLE=$(mosquitto_sub -h "$MQTT_HOST" -t "$AVAILABLE_TOPIC" -C 1)

          echo "Available spaces: $AVAILABLE"

          IFS=',' read -ra SPACES <<< "$AVAILABLE"
          COUNT=${#SPACES[@]}

          if [ "$COUNT" -eq 0 ]; then
              echo "No available spaces."
              continue
          fi

          RANDOM_INDEX=$((RANDOM % COUNT))
          MOVE=${SPACES[$RANDOM_INDEX]}

          echo "O chooses: $MOVE"

          mosquitto_pub -h "$MQTT_HOST" -t "$MOVE_TOPIC" -m "$MOVE"
      fi

      if [[ "$STATUS" == WINNER:* ]] || [ "$STATUS" = "DRAW" ]; then
          echo "Game ended: $STATUS"
          break
      fi
  done
