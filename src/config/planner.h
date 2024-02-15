#pragma once


#define INPUT_AXIS_COUNT 2
#define OUTPUT_AXIS_COUNT 2

#define DEFAULT_AXIS_STEPS_PER_UNIT   {-80.0,-80.0}
#define DEFAULT_MAX_FEEDRATE          {300, 300}    // (mm/sec)
#define DEFAULT_MAX_ACCELERATION      {9000,9000} // X, Y, Z, E maximum start speed for accelerated moves. E default values are good for skeinforge 40+, for older versions raise them a lot.

#define DEFAULT_ACCELERATION          100       // X, Y, Z and E max acceleration in mm/s^2 for printing moves

// The speed change that does not require acceleration (i.e. the software might assume it can be done instantaneously)
#define DEFAULT_XYJERK                1.0      // (mm/sec)
#define DEFAULT_ZJERK                 0.01     // (mm/sec)

// Minimum planner junction speed. Sets the default minimum speed the planner plans for at the end
// of the buffer and all stops. This should not be much greater than zero and should only be changed
// if unwanted behavior is observed on a user's machine when running at very slow speeds.
#define MINIMUM_PLANNER_SPEED 0.05// (mm/sec)
