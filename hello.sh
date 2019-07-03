
#!/bin/bash
SESSION=BLAM
WORKSPACE=~/catkin_ws
BAGFILE=~/testing_2019-07-02-19-13-03.bag
#BAGFILE=/home/costar/alex/198_multi_level/198_multi_level_3*

## Start up
tmux -2 new-session -d -s $SESSION

tmux split-window -v -p 50
tmux select-pane -t 1
tmux split-window -v -p 50
tmux select-pane -t 0
tmux split-window -h
tmux select-pane -t 2
tmux split-window -h

# Start ROSCORE
tmux send-keys -t 0 "roscore" C-m

# Start ORBSLAM
tmux send-keys -t 2 "sleep 9; source $WORKSPACE/devel/setup.bash;roslaunch blam_example exec_online_base.launch robot_namespace:=base_station" C-m

# Place rosbag
tmux send-keys -t 1 "rosbag play -r 1 $BAGFILE" 


# Prep tf record script
tmux send-keys -t 3 "sleep 9; rviz -d /home/costar/catkin_ws/src/base_station_core/visualizer_rviz/rviz_config/cfg/lamp.rviz" C-m

# Prep close script
tmux send-keys -t 4 "source $WORKSPACE/devel/setup.bash; cd $WORKSPACE/src/localizer_blam/internal/src/loop_closure_tools" C-m
tmux send-keys -t 4 "python "

# Static transform publisher
tmux select-pane -t 4
tmux split-window -h
#tmux send-keys -t 5 "sleep 9;rosrun tf2_ros static_transform_publisher 0 0 0 0 0 0 1 /husky/base_link /velodyne" C-m 

tmux select-pane -t 5
tmux split-window -v
tmux send-keys -t 6 "sleep 9;rosrun tf2_ros static_transform_publisher 0 0 0 0 0 0 /base_station/blam /world" C-m 


# place cursor in image pub
tmux select-pane -t 1

tmux -2 attach-session -t $SESSION

