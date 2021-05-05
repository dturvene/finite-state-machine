# gdb session for evtdemo, run:
#  gdb -x evtdemo.gdb evtdemo

# set session logging and turn it on
set logging file /tmp/evtdemo.gdb.log
set logging on

# if running in emacs, disable pagination
set pagination off

# evtdemo regression script, which exits back to gdb at end
set args -n -s evtdemo.script

# notes for debugging evtdemo

# info threads, which shows
# i th
# 

# break on create_timer called from workers
# b create_timer

# to step through current thread without preempt by others
# set scheduler-locking step

# before hanging on evtq_pop, turn off locking
# set scheduler-locking off

# break on toggle_timer for timer 3
# b toggle_timer if (timerid == 3)

# info breaks and disable num 1
# info b
# dis 1

