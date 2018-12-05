#!/bin/bash

if ! [ -x "$(command -v tmux)" ]; then
  echo 'Error: tmux is not installed.' >&2
  exit 1
fi

tmux kill-session -t rosc &> /dev/null

tmux new -s rosc -d
tmux split-window -d -t rosc -v

tmux send-keys -t rosc.0 "morse run auto_smart_factory auto_factory_simple_fast.py" enter

tmux send-keys -t rosc.1 "roslaunch auto_smart_factory full_system_simple.launch" enter

