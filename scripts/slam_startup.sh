#!/bin/sh
# Minimal PX4 startup for SLAM (FAST-LIVO2) test
# - SYS_AUTOSTART=10015 (gazebo-classic iris)
# - COM_ARM_WO_GPS=1 (no GPS arm)
# - EKF2_AID_MASK=24 (vision + GPS fusion)

set +e
. px4-alias.sh
PATH="$PATH:${R}etc/init.d-posix"

set SYS_AUTOSTART=4001
set RUN_MINIMAL_SHELL no

param set SYS_AUTOSTART 4001
param set COM_ARM_WO_GPS 1
param set EKF2_AID_MASK 24
param set SYS_AUTOCONFIG 1

# Load gazebo-classic iris airframe
. ${R}etc/init.d-posix/airframes/10015_gazebo-classic_iris

# Standard SITL modules
dataman start
. px4-rc.simulator
battery_simulator start
tone_alarm start
rc_update start
manual_control start
sensors start
commander start

if ! pwm_out_sim start -m sim
then
    tune_control play error
fi

. ${R}etc/init.d/rc.vehicle_setup
navigator start
. px4-rc.mavlink
. ${R}etc/init.d/rc.logging
mavlink boot_complete