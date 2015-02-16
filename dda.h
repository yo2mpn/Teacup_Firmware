#ifndef	_DDA_H
#define	_DDA_H

#include	<stdint.h>

#include	"config_wrapper.h"

#ifdef ACCELERATION_REPRAP
	#ifdef ACCELERATION_RAMPING
		#error Cant use ACCELERATION_REPRAP and ACCELERATION_RAMPING together.
	#endif
#endif

#ifdef DELTA_PRINTER
extern const int32_t delta_tower1_x,delta_tower1_y;
extern const int32_t delta_tower2_x,delta_tower2_y;
extern const int32_t delta_tower3_x,delta_tower3_y;
extern const uint32_t DELTA_DIAGONAL_ROD_2;
extern int32_t endstop_adj_x,endstop_adj_y,endstop_adj_z;
extern int32_t delta_height;
extern uint8_t bypass_delta;
#endif

// Enum to denote an axis
enum axis_e { X = 0, Y, Z, E, AXIS_COUNT };

/**
  \typedef axes_uint32_t
  \brief n-dimensional vector used to describe uint32_t axis information.

  Stored value can be anything unsigned. Units should be specified when declared.
*/
typedef uint32_t axes_uint32_t[AXIS_COUNT];

/**
  \typedef axes_int32_t
  \brief n-dimensional vector used to describe int32_t axis information.

  Stored value can be anything unsigned. Units should be specified when declared.
*/
typedef int32_t axes_int32_t[AXIS_COUNT];

/**
	\struct TARGET
	\brief target is simply a point in space/time

	X, Y, Z and E are in micrometers unless explcitely stated. F is in mm/min.
*/
typedef struct {
  axes_int32_t axis;
  uint32_t  F;

  uint16_t  e_multiplier;
  uint16_t  f_multiplier;
  uint8_t   e_relative        :1; ///< bool: e axis relative? Overrides all_relative
} TARGET;

/**
	\struct MOVE_STATE
	\brief this struct is made for tracking the current state of the movement

	Parts of this struct are initialised only once per reboot, so make sure dda_step() leaves them with a value compatible to begin a new movement at the end of the movement. Other parts are filled in by dda_start().
*/
typedef struct {
	// bresenham counters
  axes_int32_t      counter; ///< counter for total_steps vs each axis

  #if ! defined ACCELERATION_TEMPORAL
	/// counts actual steps done
	uint32_t					step_no;
	#else
    axes_uint32_t   steps;      ///< number of steps on each axis
  axes_uint32_t     time;       ///< time of the last step on each axis
  uint32_t          last_time;  ///< time of the last step of any axis
  uint32_t          all_time;
	#endif

	/// Endstop handling.
  uint8_t endstop_stop; ///< Stop due to endstop trigger
  uint8_t debounce_count_x, debounce_count_y, debounce_count_z;
} MOVE_STATE;

/**
	\struct DDA
	\brief this is a digital differential analyser data struct

	This struct holds all the details of an individual multi-axis move, including pre-calculated acceleration data.
	This struct is filled in by dda_create(), called from enqueue(), called mostly from gcode_process() and from a few other places too (eg \file homing.c)
*/
typedef struct {
	/// this is where we should finish
	TARGET						endpoint;

	union {
		struct {
			// status fields
			uint8_t						nullmove			:1; ///< bool: no axes move, maybe we wait for temperatures or change speed
			volatile uint8_t  live          :1; ///< bool: this DDA is running and still has steps to do
      uint8_t           done          :1; ///< bool: this DDA is done.
			#ifdef ACCELERATION_REPRAP
			uint8_t						accel					:1; ///< bool: speed changes during this move, run accel code
			#endif

			// directions
      // As we have muldiv() now, overflows became much less an issue and
      // it's likely time to get rid of these flags and use int instead of
      // uint for distance/speed calculations. --Traumflug 2014-07-04
			uint8_t						x_direction		:1; ///< direction flag for X axis
			uint8_t						y_direction		:1; ///< direction flag for Y axis
			uint8_t						z_direction		:1; ///< direction flag for Z axis
			uint8_t						e_direction		:1; ///< direction flag for E axis
		};
    uint16_t            allflags; ///< used for clearing all flags
	};

	// distances
  axes_uint32_t     delta;       ///< number of steps on each axis

  // uint8_t        fast_axis;   (see below)
  uint32_t          total_steps; ///< steps of the "fast" axis
  uint32_t          fast_um;     ///< movement length of this fast axis

	uint32_t					c; ///< time until next step, 24.8 fixed point

	#ifdef ACCELERATION_REPRAP
	uint32_t					end_c; ///< time between 2nd last step and last step
	int32_t						n;     ///< precalculated step time offset variable
	#endif

  /// precalculated step time offset variable
  int32_t           n;

	#ifdef ACCELERATION_RAMPING
	/// number of steps accelerating
	uint32_t					rampup_steps;
	/// number of last step before decelerating
	uint32_t					rampdown_steps;
	/// 24.8 fixed point timer value, maximum speed
	uint32_t					c_min;
  #ifdef LOOKAHEAD
  // With the look-ahead functionality, it is possible to retain physical
  // movement between G1 moves. These variables keep track of the entry and
  // exit speeds between moves.
  uint32_t          distance;
  uint32_t          crossF;
  // These two are based on the "fast" axis, the axis with the most steps.
  uint32_t          start_steps; ///< would be required to reach start feedrate
  uint32_t          end_steps; ///< would be required to stop from end feedrate
  // Displacement vector, in um, based between the difference of the starting
  // point and the target. Required to obtain the jerk between 2 moves.
  // Note: x_delta and co are in steps, not um.
  axes_int32_t      delta_um;
  // Number the moves to be able to test at the end of lookahead if the moves
  // are the same. Note: we do not need a lot of granularity here: more than
  // MOVEBUFFER_SIZE is already enough.
  uint8_t           id;
  #endif //LOOKAHEAD
	#endif //ACCELERATION_RAMPIN
	#ifdef ACCELERATION_TEMPORAL
  axes_uint32_t     step_interval;   ///< time between steps on each axis
	uint8_t						axis_to_step;    ///< axis to be stepped on the next interrupt
	#endif

  /// Small variables. Many CPUs can access 32-bit variables at word or double
  /// word boundaries only and fill smaller variables in between with gaps,
  /// so keep small variables grouped together to reduce the amount of these
  /// gaps. See e.g. NXP application note AN10963, page 10f.
  uint8_t           fast_axis;       ///< number of the fast axis

	/// Endstop homing
	uint8_t endstop_check; ///< Do we need to check endstops? 0x1=Check X, 0x2=Check Y, 0x4=Check Z
	uint8_t endstop_stop_cond; ///< Endstop condition on which to stop motion: 0=Stop on detrigger, 1=Stop on trigger
} DDA;

/*
	variables
*/

/// startpoint holds the endpoint of the most recently created DDA, so we know where the next one created starts. could also be called last_endpoint
extern TARGET startpoint;

/// the same as above, counted in motor steps
extern TARGET startpoint_steps;

/// current_position holds the machine's current position. this is only updated when we step, or when G92 (set home) is received.
extern TARGET current_position;

/*
	methods
*/

// initialize dda structures
void dda_init(void);

// distribute a new startpoint
void dda_new_startpoint(void);

// create a DDA
void dda_create(DDA *dda, const TARGET *target);

// start a created DDA (called from timer interrupt)
void dda_start(DDA *dda);

// DDA takes one step (called from timer interrupt)
void dda_step(DDA *dda);

// regular movement maintenance
void dda_clock(void);

// update current_position
void update_current_position(void);

#endif	/* _DDA_H */
