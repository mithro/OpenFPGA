/* This is a core file that defines the major data types used by VPR

 This file is divided into generally 4 major sections:

 1.  Global data types and constants
 2.  Packing specific data types
 3.  Placement specific data types
 4.  Routing specific data types

 Key background file:

 An understanding of libvpr/physical_types.h is crucial to understanding this file.  physical_types.h contains information about the architecture described in the architecture description language

 Key data structures:

 logical_block - One node in the input technology-mapped netlist
 net - Connectivity data structure for the user netlist
 block - An already clustered logic block
 rr_node - The basic building block of the interconnect in the FPGA architecture

 Cluster-specific main data structure:

 t_pb: Stores the mapping between the user netlist and the logic blocks on the FPGA achitecture.  For example, if a user design has 10 clusters of 5 LUTs each, you will have 10 t_pb instances of type cluster and within each of those clusters another 5 t_pb instances of type LUT.
 The t_pb hierarchy follows what is described by t_pb_graph_node

 Each top-level pb stores the entire routing resource graph (rr_graph).  The traceback information is included in this rr_graph so if you needed to determine connectivity down to the wire level, this is the data structure that you would traverse.
 The rr_graph is generated based on the pb_graph_node netlist of that pb.  Each pb_graph_node has a member variable called pin_count that serves as the index for the rr_node (in retrospect, I should have used rr_node_index instead of pin_count for the member variable to be more descriptive).  
 This makes it easy to identify which rr_node corresponds to which pb_graph_pin.  Additional sources and sinks are generated at the inputs and outputs of the complex logic block to match with what has already been packed into the cluster.
 

 */

#ifndef VPR_TYPES_H
#define VPR_TYPES_H

#include "arch_types.h"
#include <map>
#include <vector>
#include <array>

/*******************************************************************************
 * Global data types and constants
 ******************************************************************************/
typedef struct s_power_opts t_power_opts;
typedef struct s_net_power t_net_power;

#ifndef SPEC
#define DEBUG 1			/* Echoes input & checks error conditions */
/* Only causes about a 1% speed degradation in V 3.10 */
#endif

/*#define CREATE_ECHO_FILES*//* prints echo files */
/*#define DEBUG_FAILED_PACKING_CANDIDATES*//*Displays candidates during packing that failed */
/*#define PRINT_SINK_DELAYS*//*prints the sink delays to files */
/*#define PRINT_SLACKS*//*prints out all slacks in the circuit */
/*#define PRINT_PLACE_CRIT_PATH*//*prints out placement estimated critical path */
/*#define PRINT_NET_DELAYS*//*prints out delays for all connections */
/*#define PRINT_TIMING_GRAPH*//*prints out the timing graph */
/*#define PRINT_REL_POS_DISTR *//*prints out the relative distribution graph for placements */
/*#define DUMP_BLIF_ECHO*//*dump blif of internal representation of user circuit.  Useful for ensuring functional correctness via logical equivalence with input blif*/
/*#define HACK_LUT_PIN_SWAPPING*//* Hack to enable LUT input pin swapping for delay purposes */

#ifdef SPEC
#define NO_GRAPHICS		/* Rips out graphics (for non-X11 systems)      */
#define NDEBUG			/* Turns off assertion checking for extra speed */
#endif

#define TOKENS " \t\n"		/* Input file parsing. */

/*#define VERBOSE 1*//* Prints all sorts of intermediate data */

typedef size_t bitfield;

#define MINOR 0			/* For update_screen.  Denotes importance of update. */
#define MAJOR 1

#define MAX_SHORT 32767

/* Values large enough to be way out of range for any data, but small enough
 to allow a small number to be added to them without going out of range. */
#define HUGE_POSITIVE_FLOAT 1.e30
#define HUGE_NEGATIVE_FLOAT -1.e30

/* Used to avoid floating-point errors when comparing values close to 0 */
#define EPSILON 1.e-15
#define NEGATIVE_EPSILON -1.e-15

#define HIGH_FANOUT_NET_LIM 64 /* All nets with this number of sinks or more are considered high fanout nets */

#define FIRST_ITER_WIRELENTH_LIMIT 0.85 /* If used wirelength exceeds this value in first iteration of routing, do not route */

#define EMPTY -1

/*******************************************************************************
 * Packing specific data types and constants
 * Packing takes the circuit described in the technology mapped user netlist
 * and maps it to the complex logic blocks found in the arhictecture
 ******************************************************************************/

#define NO_CLUSTER -1
#define NEVER_CLUSTER -2
#define NOT_VALID -10000  /* Marks gains that aren't valid */
/* Ensure no gain can ever be this negative! */
#ifndef UNDEFINED						 
#define UNDEFINED -1    
#endif

/* netlist blocks are assigned one of these types */
enum logical_block_types {
	VPACK_INPAD = -2, VPACK_OUTPAD, VPACK_COMB, VPACK_LATCH, VPACK_EMPTY
};

/* Selection algorithm for selecting next seed  */
enum e_cluster_seed {
	VPACK_TIMING, VPACK_MAX_INPUTS
};

enum e_block_pack_status {
	BLK_PASSED, BLK_FAILED_FEASIBLE, BLK_FAILED_ROUTE, BLK_STATUS_UNDEFINED
};

struct s_rr_node;
/* defined later, but need to declare here because it is used */
struct s_pack_molecule;
/* defined later, but need to declare here because it is used */

/* Stores statistical information for pb such as cost information */
typedef struct s_pb_stats {
	/* Packing statistics */
	std::map<int, float> gain; /* Attraction (inverse of cost) function */

	std::map<int, float> timinggain; /* [0..num_logical_blocks-1]. The timing criticality score of this logical_block. 
	 Determined by the most critical vpack_net between this logical_block and any logical_block in the current pb */
	std::map<int, float> connectiongain; /* [0..num_logical_blocks-1] Weighted sum of connections to attraction function */
	std::map<int, float> prevconnectiongainincr; /* [0..num_logical_blocks-1] Prev sum to weighted sum of connections to attraction function */
	std::map<int, float> sharinggain; /* [0..num_logical_blocks-1]. How many nets on this logical_block are already in the pb under consideration */

	/* [0..num_logical_blocks-1]. This is the gain used for hill-climbing. It stores*
	 * the reduction in the number of pins that adding this logical_block to the the*
	 * current pb will have. This reflects the fact that sometimes the *
	 * addition of a logical_block to a pb may reduce the number of inputs     *
	 * required if it shares inputs with all other BLEs and it's output is  *
	 * used by all other child pbs in this parent pb.                               */
	std::map<int, float> hillgain;

	/* [0..num_marked_nets] and [0..num_marked_blocks] respectively.  List  *
	 * the indices of the nets and blocks that have had their num_pins_of_  *
	 * net_in_pb and gain entries altered.                             */
	int *marked_nets, *marked_blocks;
	int num_marked_nets, num_marked_blocks;
	int num_child_blocks_in_pb;

	int tie_break_high_fanout_net; /* If no marked candidate atoms, use this high fanout net to determine the next candidate atom */

	/* [0..num_logical_nets-1].  How many pins of each vpack_net are contained in the *
	 * currently open pb?                                          */
	std::map<int, int> num_pins_of_net_in_pb;

	/* Record of pins of class used TODO: Jason Luu: Should really be using hash table for this for speed, too lazy to write one now, performance isn't too bad since I'm at most iterating over the number of pins of a pb which is effectively a constant for reasonable architectures */
	int **input_pins_used; /* [0..pb_graph_node->num_pin_classes-1][0..pin_class_size] number of input pins of this class that are used */
	int **output_pins_used; /* [0..pb_graph_node->num_pin_classes-1][0..pin_class_size] number of output pins of this class that are used */

	int **lookahead_input_pins_used; /* [0..pb_graph_node->num_pin_classes-1][0..pin_class_size] number of input pins of this class that are speculatively used */
	int **lookahead_output_pins_used; /* [0..pb_graph_node->num_pin_classes-1][0..pin_class_size] number of output pins of this class that are speculatively used */

	/* Array of feasible blocks to select from [0..max_array_size-1] 
	 Sorted in ascending gain order so that the last block is the most desirable (this makes it easy to pop blocks off the list
	 */
	struct s_pack_molecule **feasible_blocks;
	int num_feasible_blocks; /* [0..num_marked_models-1] */
} t_pb_stats;

/* An FPGA complex block is represented by a hierarchy of physical blocks.  
 These include leaf physical blocks that a netlist block can map to (such as LUTs, flip-flops, memory slices, etc),
 parent physical blocks that contain children physical blocks (such as a BLE) that may be leaves or parents of other physical blocks,
 and the top-level phyiscal block which represents the complex block itself (such as a clustered logic block).

 All physical blocks are represented by this s_pb data structure.
 */
typedef struct s_pb {
	char *name; /* Name of this physical block */
	t_pb_graph_node *pb_graph_node; /* pointer to pb_graph_node this pb corresponds to */
	int logical_block; /* If this is a terminating pb, gives the logical (netlist) block that it contains */

	int mode; /* mode that this pb is set to */

	struct s_pb **child_pbs; /* children pbs attached to this pb [0..num_child_pb_types - 1][0..child_type->num_pb - 1] */
	struct s_pb *parent_pb; /* pointer to parent node */

	struct s_rr_node *rr_graph; /* pointer to rr_graph connecting pbs of cluster */
	struct s_pb **rr_node_to_pb_mapping; /* [0..num_local_rr_nodes-1] pointer look-up of which pb this rr_node belongs based on index, NULL if pb does not exist  */
	struct s_pb_stats *pb_stats; /* statistics for current pb */

	struct s_net *local_nets; /* Records post-packing connections, valid only for top-level */
	int num_local_nets; /* Records post-packing connections, valid only for top-level */

	int clock_net; /* Records clock net driving a flip-flop, valid only for lowest-level, flip-flop PBs */

	int *lut_pin_remap; /* [0..num_lut_inputs-1] applies only to LUT primitives, stores how LUT inputs were swapped during CAD flow, 
	 LUT inputs can be swapped by changing the logic in the LUT, this is useful because the fastest LUT input compared to the slowest is often significant (2-5x),
	 so this optimization is crucial for handling LUT based FPGAs.
	 */

    /* Xifan TANG: SPICE model support*/
    char* spice_name_tag;
    void* phy_pb;

    /* Xifan TANG: FPGA-SPICE and SynVerilog */
    int num_reserved_conf_bits;
    int num_conf_bits;
    int num_mode_bits;
    int num_inpads;
    int num_outpads;
    int num_iopads;
} t_pb;

struct s_tnode;

/* Technology-mapped user netlist block */
typedef struct s_logical_block {
	char *name; /* Taken from the first vpack_net which it drives. */
	enum logical_block_types type; /* I/O, combinational logic, or latch */
	t_model* model; /* Technology-mapped type (eg. LUT, Flip-flop, memory slice, inpad, etc) */

	int **input_nets; /* [0..num_input_ports-1][0..num_port_pins-1] List of input nets connected to this logical_block. */
	int **output_nets; /* [0..num_output_ports-1][0..num_port_pins-1] List of output nets connected to this logical_block. */
	int clock_net; /* Clock net connected to this logical_block. */

	int used_input_pins; /* Number of used input pins */

	int clb_index; /* Complex block index that this logical block got mapped to */

	int index; /* Index in array that this block can be found */
	t_pb* pb; /* pb primitive that this block is packed into */

	/* timing information */
	struct s_tnode ***input_net_tnodes; /* [0..num_input_ports-1][0..num_pins -1] correspnding input net tnode */
	struct s_tnode ***output_net_tnodes; /* [0..num_output_ports-1][0..num_pins -1] correspnding output net tnode */
	struct s_tnode *clock_net_tnode; /* correspnding clock net tnode */

	struct s_linked_vptr *truth_table; /* If this is a LUT (.names), then this is the logic that the LUT implements */
	struct s_linked_vptr *packed_molecules; /* List of t_pack_molecules that this logical block is a part of */

	t_pb_graph_node *expected_lowest_cost_primitive; /* predicted ideal primitive to use for this logical block */

    /* Xifan TANG: SPICE model support*/
    /* For mapping */
    t_spice_model* mapped_spice_model;
    int mapped_spice_model_index; /* index of spice_model in completed FPGA netlist */
    int temp_used;
    /* for Register/flip-flop */
    char* trigger_type;
    int init_val;
    boolean is_clock;

} t_logical_block;

enum e_pack_pattern_molecule_type {
	MOLECULE_SINGLE_ATOM, MOLECULE_FORCED_PACK
};

/**
 * Represents a grouping of logical_blocks that match a pack_pattern, these groups are intended to be placed as a single unit during packing 
 * Store in linked list
 * 
 * A chain is a special type of pack pattern.  A chain can extend across multiple logic blocks.  Must segment the chain to fit in a logic block by identifying the actual atom that forms the root of the new chain.
 * Assumes that the root of a chain is the primitive that starts the chain or is driven from outside the logic block
 */
typedef struct s_pack_molecule {
	enum e_pack_pattern_molecule_type type; /* what kind of molecule is this? */
	t_pack_patterns *pack_pattern; /* If this is a forced_pack molecule, pattern this molecule matches */
	t_model_chain_pattern *chain_pattern; /* If this is a chain molecule, chain that this molecule matches */
	t_logical_block **logical_block_ptrs; /* [0..num_blocks-1] ptrs to logical blocks that implements this molecule, index on pack_pattern_block->index of pack pattern */
	boolean valid; /* Whether or not this molecule is still valid */

	int num_blocks; /* number of logical blocks of molecule */
	int root; /* root index of molecule, logical_block_ptrs[root] is ptr to root logical block */

	float base_gain; /* Intrinsic "goodness" score for molecule independant of rest of netlist */

	int num_ext_inputs; /* number of input pins used by molecule that are not self-contained by pattern molecule matches */
	struct s_pack_molecule *next;
} t_pack_molecule;

/**
 * Stats keeper for placement information during packing
 * Contains linked lists to placement locations based on status of primitive
 */
typedef struct s_cluster_placement_stats {
	int num_pb_types; /* num primitive pb_types inside complex block */
	t_pack_molecule *curr_molecule; /* current molecule being considered for packing */
	t_cluster_placement_primitive **valid_primitives; /* [0..num_pb_types-1] ptrs to linked list of valid primitives, for convenience, each linked list head is empty */
	t_cluster_placement_primitive *in_flight; /* ptrs to primitives currently being considered */
	t_cluster_placement_primitive *tried; /* ptrs to primitives that are open but current logic block unable to pack to */
	t_cluster_placement_primitive *invalid; /* ptrs to primitives that are invalid */
} t_cluster_placement_stats;

/* Built-in library models */
#define MODEL_LOGIC "names"
#define MODEL_LATCH "latch"
#define MODEL_INPUT "input"
#define MODEL_OUTPUT "output"

/******************************************************************
 * Timing data types
 *******************************************************************/

// #define PATH_COUNTING 'P'
/* Uncomment this to turn on path counting. Its value determines how path criticality
 is calculated from forward and backward weights.  Possible values:
 'S' - sum of forward and backward weights
 'P' - product of forward and backward weights
 'L' - natural log of the product of forward and backward weights
 'R' - product of the natural logs of forward and backward weights
 See path_delay.h for further path-counting options. */

/* Timing graph information */

typedef struct s_tedge {
	/* Edge in the timing graph. */
	int to_node; /* index of node at the sink end of this edge */
	float Tdel; /* delay to go to to_node along this edge */
} t_tedge;

typedef enum {
	/* Types of tnodes (timing graph nodes). */
	TN_INPAD_SOURCE, /* input to an input I/O pad */
	TN_INPAD_OPIN, /* output from an input I/O pad */
	TN_OUTPAD_IPIN, /* input to an output I/O pad */
	TN_OUTPAD_SINK, /* output from an output I/O pad */
	TN_CB_IPIN, /* input pin to complex block */
	TN_CB_OPIN, /* output pin from complex block */
	TN_INTERMEDIATE_NODE, /* Used in post-packed timing graph only: 
	 connection between intra-cluster pins. */
	TN_PRIMITIVE_IPIN, /* input pin to a primitive (e.g. a LUT) */
	TN_PRIMITIVE_OPIN, /* output pin from a primitive (e.g. a LUT) */
	TN_FF_IPIN, /* input pin to a flip-flop - goes to TN_FF_SINK */
	TN_FF_OPIN, /* output pin from a flip-flop - comes from TN_FF_SOURCE */
	TN_FF_SINK, /* sink (D) pin of flip-flop */
	TN_FF_SOURCE, /* source (Q) pin of flip-flop */
	TN_FF_CLOCK, /* clock pin of flip-flop */
	TN_CONSTANT_GEN_SOURCE /* source of a constant logic 1 or 0 */
} e_tnode_type;

typedef struct s_prepacked_tnode_data {
	/* Data only used by prepacked tnodes. Stored separately so it
	 doesn't need to be allocated in the post-packed netlist. */
	int model_port, model_pin; /* technology mapped model pin */
	t_model_ports *model_port_ptr;
#ifndef PATH_COUNTING
	long num_critical_input_paths, num_critical_output_paths; /* count of critical paths fanning into/out of this tnode */
	float normalized_slack; /* slack (normalized with respect to max slack) */
	float normalized_total_critical_paths; /* critical path count (normalized with respect to max count) */
	float normalized_T_arr; /* arrival time (normalized with respect to max time) */
#endif
} t_prepacked_tnode_data;

typedef struct s_tnode {
	/* Node in the timing graph. Note: we combine 2 members into a bit field. */
	e_tnode_type type; /* see the above enum */
	t_tedge *out_edges; /* [0..num_edges - 1] array of edges fanning out from this tnode.
	 Note: there is a correspondence in indexing between out_edges and the
	 net data structure: out_edges[iedge] = net[inet].node_block[iedge + 1]
	 There is an offset of 1 because net[inet].node_block includes the driver
	 node at index 0, while out_edges is part of the driver node and does
	 not bother to refer to itself. */
	int num_edges;
	float T_arr; /* Arrival time of the last input signal to this node. */
	float T_req; /* Required arrival time of the last input signal to this node 
	 if the critical path is not to be lengthened. */
	int block; /* logical block primitive which this tnode is part of */

#ifdef PATH_COUNTING
	float forward_weight, backward_weight; /* Weightings of the importance of paths 
	 fanning into and out of this node, respectively. */
#endif

	/* Valid values for TN_FF_SINK, TN_FF_SOURCE, TN_FF_CLOCK, TN_INPAD_SOURCE, and TN_OUTPAD_SINK only: */
	int clock_domain; /* Index of the clock in g_sdc->constrained_clocks which this flip-flop or I/O is constrained on. */
	float clock_delay; /* The time taken for a clock signal to get to the flip-flop or I/O (assumed 0 for I/Os). */

	/* Used in post-packing timing graph only: */
	t_pb_graph_pin *pb_graph_pin; /* pb_graph_pin that this block is connected to */

	/* Used in pre-packing timing graph only: */
	t_prepacked_tnode_data * prepacked_data;
} t_tnode;

/* Other structures storing timing information */

typedef struct s_clock {
	/* Stores information on clocks given timing constraints.
	 Used in SDC parsing and timing analysis. */
	char * name;
	boolean is_netlist_clock; /* Is this a netlist or virtual (external) clock? */
	int fanout;
} t_clock;

typedef struct s_io {
	/* Stores information on I/Os given timing constraints.
	 Used in SDC parsing and timing analysis. */
	char * name; /* I/O port name with an SDC constraint */
	char * clock_name; /* Clock it was constrained on */
	float delay; /* Delay through the I/O in this constraint */
	int file_line_number; /* line in the SDC file I/O was constrained on - used for error reporting */
} t_io;

typedef struct s_timing_stats {
	/* Timing statistics for final reporting for each constraint
	 (pair of constrained source and sink clock domains).

	 cpd holds the critical path delay, the longest path between the
	 pair of domains, or equivalently the path with the least slack.

	 least_slack holds the slack of the connection with the least slack
	 over all paths in this constraint, even if this connection is part
	 of another constraint and has a lower slack from that constraint.

	 The "critical path" of the entire design is the path with the least
	 slack in the constraint with the least slack
	 (see get_critical_path_delay()). */

	float ** cpd;
	float ** least_slack;
} t_timing_stats;

typedef struct s_slack {
	/* Matrices storing slacks and criticalities of each sink pin on each net
	 [0..num_nets-1][1..num_pins-1] for both pre- and post-packed netlists. */
	float ** slack;
	float ** timing_criticality;
#ifdef PATH_COUNTING
	float ** path_criticality;
#endif
} t_slack;

typedef struct s_override_constraint {
	/* A special-case constraint to override the default, calculated, timing constraint.  Holds data from
	 set_clock_groups, set_false_path, set_max_delay, and set_multicycle_path commands. Can hold data for
	 clock-to-clock, clock-to-flip-flop, flip-flop-to-clock or flip-flop-to-flip-flop constraints, each of
	 which has its own array (g_sdc->cc_constraints, g_sdc->cf_constraints, g_sdc->fc_constraints, and g_sdc->ff_constraints). */
	char ** source_list; /* Array of net names of flip-flops or clocks */
	char ** sink_list;
	int num_source;
	int num_sink;
	float constraint;
	int num_multicycles;
	int file_line_number; /* line in the SDC file clock was constrained on - used for error reporting */
} t_override_constraint;

typedef struct s_timing_constraints { /* Container structure for all SDC timing constraints. 
 See top-level comment to read_sdc.c for details on members. */
	int num_constrained_clocks; /* number of clocks with timing constraints */
	t_clock * constrained_clocks; /* [0..g_sdc->num_constrained_clocks - 1] array of clocks with timing constraints */

	float ** domain_constraint; /* [0..num_constrained_clocks - 1 (source)][0..num_constrained_clocks - 1 (destination)] */

	int num_constrained_inputs; /* number of inputs with timing constraints */
	t_io * constrained_inputs; /* [0..num_constrained_inputs - 1] array of inputs with timing constraints */

	int num_constrained_outputs; /* number of outputs with timing constraints */
	t_io * constrained_outputs; /* [0..num_constrained_outputs - 1] array of outputs with timing constraints */

	int num_cc_constraints; /* number of special-case clock-to-clock constraints overriding default, calculated, timing constraints */
	t_override_constraint * cc_constraints; /*  [0..num_cc_constraints - 1] array of such constraints */

	int num_cf_constraints; /* number of special-case clock-to-flipflop constraints */
	t_override_constraint * cf_constraints; /*  [0..num_cf_constraints - 1] array of such constraints */

	int num_fc_constraints; /* number of special-case flipflop-to-clock constraints */
	t_override_constraint * fc_constraints; /*  [0..num_fc_constraints - 1] */

	int num_ff_constraints; /* number of special-case flipflop-to-flipflop constraints */
	t_override_constraint * ff_constraints; /*  [0..num_ff_constraints - 1] array of such constraints */
} t_timing_constraints;

/***************************************************************************
 * Placement and routing data types
 ****************************************************************************/

/* Timing data structures end */
enum sched_type {
	AUTO_SCHED, USER_SCHED
};
/* Annealing schedule */

enum pic_type {
	NO_PICTURE, PLACEMENT, ROUTING
};
/* What's on screen? */

/* Map netlist to FPGA or timing analyze only */
enum e_operation {
	RUN_FLOW, TIMING_ANALYSIS_ONLY
};

enum pfreq {
	PLACE_NEVER, PLACE_ONCE, PLACE_ALWAYS
};

/* Are the pads free to be moved, locked in a random configuration, or *
 * locked in user-specified positions?                                 */
enum e_pad_loc_type {
	FREE, RANDOM, USER
};

/* Power data for t_net structure */
struct s_net_power {
	/* Signal probability - long term probability that signal is logic-high*/
	float probability;

	/* Transistion density - average # of transitions per clock cycle
	 * For example, a clock would have density = 2
	 */
	float density;
};

/* name:  ASCII net name for informative annotations in the output.          *
 * num_sinks:  Number of sinks on this net.                                  *
 * node_block: [0..num_sinks]. Contains the blocks to which the nodes of this 
 *         net connect.  The source block is node_block[0] and the sink blocks
 *         are the remaining nodes.
 * node_block_port: [0..num_sinks]. Contains port index (on a block) to 
 *          which each net terminal connects. 
 * node_block_pin: [0..num_sinks]. Contains the index of the pin (on a block) to 
 *          which each net terminal connects. 
 * is_global: not routed
 * is_const_gen: constant generator (does not affect timing) */
typedef struct s_net {
	char *name;
	int num_sinks;
	int *node_block;
	int *node_block_port;
	int *node_block_pin;
	boolean is_global;
	boolean is_const_gen;
	t_net_power * net_power;
    /* Xifan TANG: SPICE modeling */
    t_spice_net_info* spice_net_info;
    /* Xifan TANG: CLB_IPIN_REMAP */
    int** prefer_side; /* [0..num_sinks][0..3] */
    /* Xifan TANG: OPIN occupancy stats */
    int num_mapped_opins;
} t_net;

/* s_grid_tile is the minimum tile of the fpga                         
 * type:  Pointer to type descriptor, NULL for illegal, IO_TYPE for io 
 * offset: Number of grid tiles above the bottom location of a block 
 * usage: Number of blocks used in this grid tile
 * blocks[]: Array of logical blocks placed in a physical position, EMPTY means
 no block at that index */
typedef struct s_grid_tile {
	t_type_ptr type;
	int offset;
	int usage;
	int *blocks;
} t_grid_tile;

/* Stores the bounding box of a net in terms of the minimum and  *
 * maximum coordinates of the blocks forming the net, clipped to *
 * the region (1..nx, 1..ny).                                    */
typedef struct s_bb t_bb;
struct s_bb {
	int xmin;
	int xmax;
	int ymin;
	int ymax;
};

/* capacity:   Capacity of this region, in tracks.               *
 * occupancy:  Expected number of tracks that will be occupied.  *
 * cost:       Current cost of this usage.                       */
struct s_place_region {
	float capacity;
	float inv_capacity;
	float occupancy;
	float cost;
};

/*
 Represents a clustered logic block of a user circuit that fits into one unit of space in an FPGA grid block
 name: identifier for this block
 type: the type of physical block this user circuit block can map into
 nets: nets that connect to other user circuit blocks
 x: x-coordinate
 y: y-coordinate
 z: occupancy coordinate
 pb: Physical block representing the clustering of this CLB
 isFixed: TRUE if this block's position is fixed by the user and shouldn't be moved during annealing
 */
struct s_block {
	char *name;
	t_type_ptr type;
	int *nets;
	int x;
	int y;
	int z;

    /* Xifan TANG: CLB_IPIN_REMAP */
    int* nets_sink_index;
    int** pin_prefer_side; /* [0..num_pins-1][0..3] */

	t_pb *pb;
    
    /* Xifan TANG: FPGA-SPICE 
     * pb for physical model  
     */
    void* phy_pb;

	boolean isFixed;

};
typedef struct s_block t_block;

/* Names of various files */
struct s_file_name_opts {
	char *ArchFile;
	char *CircuitName;
	char *BlifFile;
	char *NetFile;
	char *PlaceFile;
	char *RouteFile;
	char *ActFile;
	char *PowerFile;
	char *CmosTechFile;
	char *out_file_prefix;
    /* For shell-like interface */
	char *SDCFile;
};

/* Options for packing
 * TODO: document each packing parameter         */
enum e_packer_algorithm {
	PACK_GREEDY, PACK_BRUTE_FORCE
};

struct s_packer_opts {
	char *blif_file_name;
	char *sdc_file_name;
	char *output_file;
	boolean global_clocks;
	boolean hill_climbing_flag;
	boolean sweep_hanging_nets_and_inputs;
	boolean timing_driven;
	enum e_cluster_seed cluster_seed_type;
	float alpha;
	float beta;
	int recompute_timing_after;
	float block_delay;
	float intra_cluster_net_delay;
	float inter_cluster_net_delay;
	boolean auto_compute_inter_cluster_net_delay;
	boolean skip_clustering;
	boolean allow_unrelated_clustering;
	boolean allow_early_exit;
	boolean connection_driven;
	boolean doPacking;
	enum e_packer_algorithm packer_algorithm;
	float aspect;
    /* Xifan TANG: PACK_CLB_PIN_REMAP */
    boolean pack_clb_pin_remap;
    /* END */
};

/* Annealing schedule information for the placer.  The schedule type      *
 * is either USER_SCHED or AUTO_SCHED.  Inner_num is multiplied by        *
 * num_blocks^4/3 to find the number of moves per temperature.  The       *
 * remaining information is used only for USER_SCHED, and have the        *
 * obvious meanings.                                                      */
struct s_annealing_sched {
	enum sched_type type;
	float inner_num;
	float init_t;
	float alpha_t;
	float exit_t;
};

enum e_place_algorithm {
	BOUNDING_BOX_PLACE, NET_TIMING_DRIVEN_PLACE, PATH_TIMING_DRIVEN_PLACE
};

struct s_placer_opts {
	enum e_place_algorithm place_algorithm;
	float timing_tradeoff;
	int block_dist;
	float place_cost_exp;
	int place_chan_width;
	enum e_pad_loc_type pad_loc_type;
	char *pad_loc_file;
	enum pfreq place_freq;
	int recompute_crit_iter;
	boolean enable_timing_computations;
	int inner_loop_recompute_divider;
	float td_place_exp_first;
	int seed;
	float td_place_exp_last;
	boolean doPlacement;
    /* Xifan TANG: CLB_PIN_REMAP */
    boolean place_clb_pin_remap;
    /* END */
};

/* Various options for the placer.                                           *
 * place_algorithm:  BOUNDING_BOX_PLACE or NET_TIMING_DRIVEN_PLACE, or       *
 *                   PATH_TIMING_DRIVEN_PLACE                                *
 * timing_tradeoff:  When TIMING_DRIVEN_PLACE mode, what is the tradeoff     *
 *                   timing driven and BOUNDING_BOX_PLACE.                   *
 * block_dist:  Initial guess of how far apart blocks on the critical path   *
 *              This is used to compute the initial slacks and criticalities *
 * place_cost_exp:  Power to which denominator is raised for linear_cong.    *
 * place_chan_width:  The channel width assumed if only one placement is     *
 *                    performed.                                             *
 * pad_loc_type:  Are pins FREE, fixed randomly, or fixed from a file.       *
 * pad_loc_file:  File to read pin locations form if pad_loc_type            *
 *                     is USER.                                              *
 * place_freq:  Should the placement be skipped, done once, or done for each *
 *              channel width in the binary search.                          *
 * recompute_crit_iter: how many temperature stages pass before we recompute *
 *               criticalities based on average point to point delay         *
 * enable_timing_computations: in bounding_box mode, normally, timing        *
 *               information is not produced, this causes the information    *
 *               to be computed. in *_TIMING_DRIVEN modes, this has no effect*
 * inner_loop_crit_divider: (move_lim/inner_loop_crit_divider) determines how*
 *               many inner_loop iterations pass before a recompute of       *
 *               criticalities is done.                                      *
 * td_place_exp_first: exponent that is used on the timing_driven criticlity *
 *               it is the value that the exponent starts at.                *
 * td_place_exp_last: value that the criticality exponent will be at the end *
 * doPlacement: TRUE if placement is supposed to be done in the CAD flow, FALSE otherwise */

enum e_route_type {
	GLOBAL, DETAILED
};
enum e_router_algorithm {
	BREADTH_FIRST, TIMING_DRIVEN, NO_TIMING
};
enum e_base_cost_type {
	INTRINSIC_DELAY, DELAY_NORMALIZED, DEMAND_ONLY
};

#define NO_FIXED_CHANNEL_WIDTH -1

typedef struct s_router_opts t_router_opts;
struct s_router_opts {
	float first_iter_pres_fac;
	float initial_pres_fac;
	float pres_fac_mult;
	float acc_fac;
	float bend_cost;
	int max_router_iterations;
	int bb_factor;
	enum e_route_type route_type;
	int fixed_channel_width;
	enum e_router_algorithm router_algorithm;
	enum e_base_cost_type base_cost_type;
	float astar_fac;
	float max_criticality;
	float criticality_exp;
	boolean verify_binary_search;
	boolean full_stats;
	boolean doRouting;
    /* Xifan Tang: option to enable adaption to tileable route channel width */
    boolean use_tileable_route_chan_width;
};

/* All the parameters controlling the router's operation are in this        *
 * structure.                                                               *
 * first_iter_pres_fac:  Present sharing penalty factor used for the        *
 *                 very first (congestion mapping) Pathfinder iteration.    *
 * initial_pres_fac:  Initial present sharing penalty factor for            *
 *                    Pathfinder; used to set pres_fac on 2nd iteration.    *
 * pres_fac_mult:  Amount by which pres_fac is multiplied each              *
 *                 routing iteration.                                       *
 * acc_fac:  Historical congestion cost multiplier.  Used unchanged         *
 *           for all iterations.                                            *
 * bend_cost:  Cost of a bend (usually non-zero only for global routing).   *
 * max_router_iterations:  Maximum number of iterations before giving       *
 *                up.                                                       *
 * bb_factor:  Linear distance a route can go outside the net bounding      *
 *             box.                                                         *
 * route_type:  GLOBAL or DETAILED.                                         *
 * fixed_channel_width:  Only attempt to route the design once, with the    *
 *                       channel width given.  If this variable is          *
 *                       == NO_FIXED_CHANNEL_WIDTH, do a binary search      *
 *                       on channel width.                                  *
 * router_algorithm:  BREADTH_FIRST or TIMING_DRIVEN.  Selects the desired  *
 *                    routing algorithm.                                    *
 * base_cost_type: Specifies how to compute the base cost of each type of   *
 *                 rr_node.  INTRINSIC_DELAY -> base_cost = intrinsic delay *
 *                 of each node.  DELAY_NORMALIZED -> base_cost = "demand"  *
 *                 x average delay to route past 1 CLB.  DEMAND_ONLY ->     *
 *                 expected demand of this node (old breadth-first costs).  *
 *                                                                          *
 * The following parameters are used only by the timing-driven router.      *
 *                                                                          *
 * astar_fac:  Factor (alpha) used to weight expected future costs to       *
 *             target in the timing_driven router.  astar_fac = 0 leads to  *
 *             an essentially breadth-first search, astar_fac = 1 is near   *
 *             the usual astar algorithm and astar_fac > 1 are more         *
 *             aggressive.                                                  *
 * max_criticality: The maximum criticality factor (from 0 to 1) any sink   *
 *                  will ever have (i.e. clip criticality to this number).  *
 * criticality_exp: Set criticality to (path_length(sink) / longest_path) ^ *
 *                  criticality_exp (then clip to max_criticality).         
 * doRouting: True if routing is supposed to be done, FALSE otherwise */

typedef struct s_det_routing_arch t_det_routing_arch;
struct s_det_routing_arch {
	enum e_directionality directionality; /* UDSD by AY */
	int Fs;
	enum e_switch_block_type switch_block_type;
	int sub_Fs;
    boolean wire_opposite_side;
	enum e_switch_block_type switch_block_sub_type;
	int num_segment;
	short num_switch;
	short global_route_switch;
	short delayless_switch;
	short wire_to_ipin_switch;
	float R_minW_nmos;
	float R_minW_pmos;
    int num_swseg_pattern; /*Xifan TANG: Switch Segment Pattern Support*/
    short opin_to_wire_switch; /* mrFPGA: Xifan TANG*/
    bool tileable; /* Xifan Tang: tileable rr_graph support */
};

/* Defines the detailed routing architecture of the FPGA.  Only important   *
 * if the route_type is DETAILED.                                           *
 * (UDSD by AY) directionality: Should the tracks be uni-directional or     *
 *                            bi-directional?                               *
 * switch_block_type:  Pattern of switches at each switch block.  I         *
 *           assume Fs is always 3.  If the type is SUBSET, I use a         *
 *           Xilinx-like switch block where track i in one channel always   *
 *           connects to track i in other channels.  If type is WILTON,     *
 *           I use a switch block where track i does not always connect     *
 *           to track i in other channels.  See Steve Wilton, Phd Thesis,   *
 *           University of Toronto, 1996.  The UNIVERSAL switch block is    *
 *           from Y. W. Chang et al, TODAES, Jan. 1996, pp. 80 - 101.       *
 * num_segment:  Number of distinct segment types in the FPGA.              *
 * num_switch:  Number of distinct switch types (pass transistors or        *
 *              buffers) in the FPGA.                                       *
 * delayless_switch:  Index of a zero delay switch (used to connect things  *
 *                    that should have no delay).                           *
 * wire_to_ipin_switch:  Index of a switch used to connect wire segments    *
 *                       to clb or pad input pins (IPINs).                  *
 * R_minW_nmos:  Resistance (in Ohms) of a minimum width nmos transistor.   *
 *               Used only in the FPGA area model.                          *
 * R_minW_pmos:  Resistance (in Ohms) of a minimum width pmos transistor.   */

enum e_drivers {
	MULTI_BUFFERED, SINGLE
};
/* legacy routing drivers by Andy Ye (remove or integrate in future) */

enum e_direction {
	INC_DIRECTION = 0, DEC_DIRECTION = 1, BI_DIRECTION = 2
};
/* UDSD by AY */

typedef struct s_seg_details {
	int length;
	int start;
	boolean longline;
	boolean *sb;
	boolean *cb;
	short wire_switch;
	short opin_switch;
	float Rmetal;
	float Cmetal;
	boolean twisted;
	enum e_direction direction; /* UDSD by AY */
	enum e_drivers drivers; /* UDSD by AY */
	int group_start;
	int group_size;
	int index;
	float Cmetal_per_m; /* Used for power */
    /* mrFPGA */
    short seg_switch;
    /* end */
} t_seg_details;

/* Lists detailed information about segmentation.  [0 .. W-1].              *
 * length:  length of segment.                                              *
 * start:  index at which a segment starts in channel 0.                    *
 * longline:  TRUE if this segment spans the entire channel.                *
 * sb:  [0..length]:  TRUE for every channel intersection, relative to the  *
 *      segment start, at which there is a switch box.                      *
 * cb:  [0..length-1]:  TRUE for every logic block along the segment at     *
 *      which there is a connection box.                                    *
 * wire_switch:  Index of the switch type that connects other wires *to*    *
 *               this segment.                                              *
 * opin_switch:  Index of the switch type that connects output pins (OPINs) *
 *               *to* this segment.                                         *
 * Cmetal: Capacitance of a routing track, per unit logic block length.     *
 * Rmetal: Resistance of a routing track, per unit logic block length.      *
 * (UDSD by AY) direction: The direction of a routing track.                *
 * (UDSD by AY) drivers: How do signals driving a routing track connect to  *
 *                       the track?                                         *
 * index: index of the segment type used for this track.                    */
typedef struct s_linked_f_pointer t_linked_f_pointer;
struct s_linked_f_pointer {
	struct s_linked_f_pointer *next;
	float *fptr;
};

/* A linked list of float pointers.  Used for keeping track of   *
 * which pathcosts in the router have been changed.              */

/* Uncomment lines below to save some memory, at the cost of debugging ease. */
/*enum e_rr_type {SOURCE, SINK, IPIN, OPIN, CHANX, CHANY}; */
/* typedef short t_rr_type */

typedef enum e_rr_type {
	SOURCE = 0, SINK, IPIN, OPIN, CHANX, CHANY, INTRA_CLUSTER_EDGE, NUM_RR_TYPES
} t_rr_type;

constexpr std::array<const char*, NUM_RR_TYPES + 1> rr_node_typename { {
  "SOURCE", "SINK", "IPIN", "OPIN", "CHANX", "CHANY", "INTRA_CLUSTER_EDGE", "NUM_RR_TYPES"
} };

/* Type of a routing resource node.  x-directed channel segment,   *
 * y-directed channel segment, input pin to a clb to pad, output   *
 * from a clb or pad (i.e. output pin of a net) and:               *
 * SOURCE:  A dummy node that is a logical output within a block   *
 *          -- i.e., the gate that generates a signal.             *
 * SINK:    A dummy node that is a logical input within a block    *
 *          -- i.e. the gate that needs a signal.                  */

typedef struct s_trace {
	int index;
	short iswitch;
	int iblock;
	int num_siblings;
	struct s_trace *next;
} t_trace;

/* Basic element used to store the traceback (routing) of each net.        *
 * index:   Array index (ID) of this routing resource node.                *
 * iswitch: Index of the switch type used to go from this rr_node to       *
 *          the next one in the routing.  OPEN if there is no next node    *
 *          (i.e. this node is the last one (a SINK) in a branch of the    *
 *          net's routing).                                                *
 * iblock:  Index of block that this trace applies to if applicable, OPEN  *
 *          otherwise                                                      *
 * num_siblings: Number of traceback sibling nodes (including self). This  *
 *               count is used to help extract individual route paths for  *
 *               each net. A '0' indicates a terminal node, '1' means a    *
 *               single child, '+1' defines branch with 2 or more children.*
 * next:    Pointer to the next traceback element in this route.           */

#define NO_PREVIOUS -1

typedef struct s_rr_node t_rr_node;
struct s_rr_node {
	short xlow;
	short xhigh;
	short ylow;
	short yhigh;

	short ptc_num;
    std::vector<short> track_ids; /* Tileable arch support: Track indices in each GSB */

	short cost_index;
	short occ;
	short capacity;
	short fan_in;
	short num_edges;
	t_rr_type type;
	int *edges;
	short *switches;

    short driver_switch; /* Xifan TANG: Switch Segment Pattern Support*/
    int unbuf_switched; /* Xifan TANG: Switch Segment Pattern Support*/
    /* mrFPGA: Xifan TANG */
    int buffered;
    /* end */
	float R;
	float C;

	enum e_direction direction; /* UDSD by AY */
	enum e_drivers drivers; /* UDSD by AY */
	int num_wire_drivers; /* UDSD by WMF */
	int num_opin_drivers; /* UDSD by WMF (could use "short") */
    /* Xifan TANG: SPICE model support */
    int num_drive_rr_nodes;
    t_rr_node** drive_rr_nodes;
    int* drive_switches;
    /* Xifan TANG: for parasitic net estimation */
    boolean vpack_net_num_changed;
    boolean is_parasitic_net;
    /* Xifan TANG: pb_pin_eq_auto_detect support */
    boolean is_in_heap;
    /* SPECIAL: For switch box muxes */
    int sb_num_drive_rr_nodes;
    t_rr_node** sb_drive_rr_nodes;
    int* sb_drive_switches;
    t_pb* pb;
    /* BC: Supports SDC for SBs/CBs. PBs use the one inside of the pb_graph*/
    char* name_mux;
    int id_path;
    // int seg_index; /* Valid only for CHANX or CHANY*/
    /* END */

	/* Used by clustering only (TODO, may wish to extend to regular router) */
	int prev_node;
	int prev_edge;
	int net_num;
	int vpack_net_num;
    /* Note that prev_node changes after routing!!! 
     * because logic equivalent pins may swap with each other!!! */
    /* Xifan TANG: I backup the results in packing here, 
     * and keep prev_node&prev_edge well correspond to routing results!*/
	int prev_node_in_pack; 
	int prev_edge_in_pack;
	int net_num_in_pack;
    /* END */
	t_pb_graph_pin *pb_graph_pin;
	t_tnode *tnode;
	float pack_intrinsic_cost;

	int z; /* For IPIN, source, and sink nodes, helps identify which location this rr_node belongs to */
};
/* Main structure describing one routing resource node.  Everything in       *
 * this structure should describe the graph -- information needed only       *
 * to store algorithm-specific data should be stored in one of the           *
 * parallel rr_node_?? structures.                                           *
 *                                                                           *
 * xlow, xhigh, ylow, yhigh:  Integer coordinates (see route.c for           *
 *       coordinate system) of the ends of this routing resource.            *
 *       xlow = xhigh and ylow = yhigh for pins or for segments of           *
 *       length 1.  These values are used to decide whether or not this      *
 *       node should be added to the expansion heap, based on things         *
 *       like whether it's outside the net bounding box or is moving         *
 *       further away from the target, etc.                                  *
 * type:  What is this routing resource?                                     *
 * ptc_num:  Pin, track or class number, depending on rr_node type.          *
 *           Needed to properly draw.                                        *
 * cost_index: An integer index into the table of routing resource indexed   *
 *             data (this indirection allows quick dynamic changes of rr     *
 *             base costs, and some memory storage savings for fields that   *
 *             have only a few distinct values).                             *
 * occ:        Current occupancy (usage) of this node.                       *
 * capacity:   Capacity of this node (number of routes that can use it).     *
 * num_edges:  Number of edges exiting this node.  That is, the number       *
 *             of nodes to which it connects.                                *
 * edges[0..num_edges-1]:  Array of indices of the neighbours of this        *
 *                         node.                                             *
 * switches[0..num_edges-1]:  Array of switch indexes for each of the        *
 *                            edges leaving this node.                       *
 *                                                                           *
 * The following parameters are only needed for timing analysis.             *
 * R:  Resistance to go through this node.  This is only metal               *
 *     resistance (end to end, so conservative) -- it doesn't include the    *
 *     switch that leads to another rr_node.                                 *
 * C:  Total capacitance of this node.  Includes metal capacitance, the      *
 *     input capacitance of all switches hanging off the node, the           *
 *     output capacitance of all switches to the node, and the connection    *
 *     box buffer capacitances hanging off it.                               *
 * (UDSD by AY) direction: if the node represents a track, this field        *
 *                         indicates the direction of the track. Otherwise   *
 *                         the value contained in the field should be        *
 *                         ignored.                                          *
 * (UDSD by AY) drivers: if the node represents a track, this field          *
 *                       indicates the driving architecture of the track.    *
 *                       Otherwise the value contained in the field should   *
 *                       be ignored.                                         */

typedef struct s_rr_indexed_data {
	float base_cost;
	float saved_base_cost;
	int ortho_cost_index;
	int seg_index;
	float inv_length;
	float T_linear;
	float T_quadratic;
	float C_load;


	/* Power Estimation: Wire capacitance in (Farads * tiles / meter)
	 * This is used to calculate capacitance of this segment, by
	 * multiplying it by the length per tile (meters/tile).
	 * This is only the wire capacitance, not including any switches */
	float C_tile_per_m;
} t_rr_indexed_data;

/* Data that is pointed to by the .cost_index member of t_rr_node.  It's     *
 * purpose is to store the base_cost so that it can be quickly changed       *
 * and to store fields that have only a few different values (like           *
 * seg_index) or whose values should be an average over all rr_nodes of a    *
 * certain type (like T_linear etc., which are used to predict remaining     *
 * delay in the timing_driven router).                                       *
 *                                                                           *
 * base_cost:  The basic cost of using an rr_node.                           *
 * ortho_cost_index:  The index of the type of rr_node that generally        *
 *                    connects to this type of rr_node, but runs in the      *
 *                    orthogonal direction (e.g. vertical if the direction   *
 *                    of this member is horizontal).                         *
 * seg_index:  Index into segment_inf of this segment type if this type of   *
 *             rr_node is an CHANX or CHANY; OPEN (-1) otherwise.            *
 * inv_length:  1/length of this type of segment.                            *
 * T_linear:  Delay through N segments of this type is N * T_linear + N^2 *  *
 *            T_quadratic.  For buffered segments all delay is T_linear.     *
 * T_quadratic:  Dominant delay for unbuffered segments, 0 for buffered      *
 *               segments.                                                   *
 * C_load:  Load capacitance seen by the driver for each segment added to    *
 *          the chain driven by the driver.  0 for buffered segments.        */

enum e_cost_indices {
	SOURCE_COST_INDEX = 0,
	SINK_COST_INDEX,
	OPIN_COST_INDEX,
	IPIN_COST_INDEX,
	CHANX_COST_INDEX_START
};

/* Xifan Tang: Move this struct from rr_graph.c to here
 * This is a general representation on clb_to_clb_directs
 */
typedef struct s_clb_to_clb_directs {
	t_type_descriptor *from_clb_type;
	int from_clb_pin_start_index;
	int from_clb_pin_end_index;
	t_type_descriptor *to_clb_type;
	int to_clb_pin_start_index;
	int to_clb_pin_end_index;
    /* Xifan Tang: add useful addition info to this struct */
	int x_offset;
	int y_offset;
	int z_offset;
    t_spice_model* spice_model;
    char* name;
} t_clb_to_clb_directs;


/* Gives the index of the SOURCE, SINK, OPIN, IPIN, etc. member of           *
 * rr_indexed_data.                                                          */

/* Xifan TANG: For better modeling of global routing architecture */
/* Information for each switch block */
typedef struct s_sb t_sb;
struct s_sb {
  /* Coordinators */
  int x;
  int y;
  /* Directionality */ 
  enum e_directionality directionality; /* UDSD by AY */
  /* Connectivity parameter */
  int fs;
  int fc_out;
  /* chan_width at each side */
  int num_sides; /* Should be fixed to 4 */
  /* Input/output rr_nodes at each side, according to chan_width
   * Each element is a pointer to a rr_node   
   */ 
  /* A list of all the rr_nodes at each side, whatever their directionality */
  int* chan_width;
  enum PORTS** chan_rr_node_direction;
  t_rr_node*** chan_rr_node;
  /* LB inputs/outputs */
  int* num_ipin_rr_nodes; /* Switch block has some inputs that are CLB IPIN*/
  t_rr_node*** ipin_rr_node;
  int** ipin_rr_node_grid_side; /* We need to record the side of a IPIN, because a IPIN may locate on more than one sides */
  int* num_opin_rr_nodes; /* Connection block has some outputs that are CLB OPIN */
  t_rr_node*** opin_rr_node;
  int** opin_rr_node_grid_side; /* We need to record the side of a OPIN, because a OPIN may locate on more than one sides */
  int num_reserved_conf_bits; /* number of reserved configuration bits */
  int conf_bits_lsb; /* LSB of configuration bits */
  int conf_bits_msb; /* MSB of configuration bits */

  /* For identical SBs */
  t_sb* mirror; /* an exact mirror of this switch block, with same connection & switches */
  /* an rotatable mirror of this switch block, 
   * the two switch blocks will be same in terms of connection & switches 
   * by applying an offset to the connection & switches 
   */
  t_sb* rotatable; 
  /* Offset to be applied for each side of nodes */
  int* offset_ipin; /* [0, ..., num_sides-1]*/
  int* offset_opin; /* [0, ..., num_sides-1]*/
  int* offset_chan; /* [0, ..., num_sides-1]*/
};

/* Information for each conneciton block */
typedef struct s_cb t_cb;
struct s_cb {
  /* Type of Connection block, can only be either CHANX or CHANY,
   * Corresponding to CB connected CHANX/CHANY to a CLB 
   */
  t_rr_type type;
  /* Coordinators */
  int x;
  int y;
  /* Directionality */ 
  enum e_directionality directionality; /* UDSD by AY */
  /* Connectivity parameter */
  int fc_in;
  /* chan_width at each side */
  int num_sides; /* Should be fixed to 4 */
  /* Input/output rr_nodes at each side, according to chan_width
   * Each element is a pointer to a rr_node   
   */ 
  /* A list of all the rr_nodes at each side, whatever their directionality */
  int* chan_width;
  enum PORTS** chan_rr_node_direction;
  t_rr_node*** chan_rr_node;
  /* LB inputs/outputs */
  int* num_ipin_rr_nodes; /* Switch block has some inputs that are CLB IPIN*/
  t_rr_node*** ipin_rr_node;
  int** ipin_rr_node_grid_side; /* We need to record the side of a IPIN, because a IPIN may locate on more than one sides */
  int* num_opin_rr_nodes; /* Connection block has some outputs that are CLB OPIN */
  t_rr_node*** opin_rr_node;
  int** opin_rr_node_grid_side; /* We need to record the side of a OPIN, because a OPIN may locate on more than one sides */
  int num_reserved_conf_bits; /* number of reserved configuration bits */
  int conf_bits_lsb; /* LSB of configuration bits */
  int conf_bits_msb; /* MSB of configuration bits */

  /* For identical SBs */
  t_cb* mirror; /* an exact mirror of this connection block, with same connection & switches */
  /* an rotatable mirror of this connection block, 
   * the two connection blocks will be same in terms of connection & switches 
   * by applying an offset to the connection & switches 
   */
  t_cb* rotatable; 
  /* Offset to be applied for each side of nodes */
  int* offset_ipin; /* [0, ..., num_sides-1]*/
  int* offset_opin; /* [0, ..., num_sides-1]*/
  int* offset_chan; /* [0, ..., num_sides-1]*/
};

/* Xifan TANG: SPICE Support*/
typedef struct s_spice_opts t_spice_opts;
struct s_spice_opts {
  boolean do_spice;
  boolean fpga_spice_print_top_testbench; 
  boolean fpga_spice_print_grid_testbench; 
  boolean fpga_spice_print_cb_testbench; 
  boolean fpga_spice_print_sb_testbench; 
  boolean fpga_spice_print_pb_mux_testbench; 
  boolean fpga_spice_print_cb_mux_testbench; 
  boolean fpga_spice_print_sb_mux_testbench; 
  boolean fpga_spice_print_lut_testbench; 
  boolean fpga_spice_print_hardlogic_testbench; 
  boolean fpga_spice_print_io_testbench; 
  boolean fpga_spice_leakage_only;
  boolean fpga_spice_parasitic_net_estimation;
  boolean fpga_spice_testbench_load_extraction;
 
  /*Xifan TANG: FPGA SPICE Model Support*/
  char* spice_dir;
  char* include_dir;
  char* subckt_dir;

  int fpga_spice_sim_multi_thread_num;
  char* simulator_path;
};

/* Xifan TANG: synthesizable verilog dumping */
typedef struct s_syn_verilog_opts t_syn_verilog_opts;
struct s_syn_verilog_opts {
  boolean dump_syn_verilog;
  boolean dump_explicit_verilog;
  char* syn_verilog_dump_dir;
  boolean print_top_testbench;
  boolean print_input_blif_testbench;
  boolean print_formal_verification_top_netlist;
  boolean include_timing;
  boolean include_signal_init;
  boolean include_icarus_simulator;
  boolean print_modelsim_autodeck;
  char* modelsim_ini_path;
  char* report_timing_path;
  boolean print_user_defined_template;
  boolean print_autocheck_top_testbench;
  char* reference_verilog_benchmark_file;
  boolean print_report_timing_tcl;
  boolean print_sdc_pnr;
  boolean print_sdc_analysis;
};

/* Xifan TANG: bitstream generator */
typedef struct s_bitstream_gen_opts t_bitstream_gen_opts;
struct s_bitstream_gen_opts {
  boolean gen_bitstream;
  char* bitstream_output_file;
};

typedef struct s_fpga_spice_opts t_fpga_spice_opts;
struct s_fpga_spice_opts {
  boolean do_fpga_spice;
  boolean read_act_file;
  boolean rename_illegal_port; /* Rename illegal port names that is not compatible with verilog/SPICE syntax */
  t_spice_opts SpiceOpts; /* Xifan TANG: SPICE Support*/
  t_syn_verilog_opts SynVerilogOpts; /* Xifan TANG: Synthesizable verilog dumping*/
  t_bitstream_gen_opts BitstreamGenOpts; /* Xifan Bitsteam Generator */

  boolean compact_routing_hierarchy; /* use compact routing hierarchy */

  /* Signal Density */
  float signal_density_weight;
  float sim_window_size;
  /* SB XML file prefix */
  boolean output_sb_xml;
  char* sb_xml_dir;
};

/* Power estimation options */
struct s_power_opts {
	boolean do_power; /* Perform power estimation? */
};

/* Type to store our list of token to enum pairings */
struct s_TokenPair {
	const char *Str;
	int Enum;
};

/* Store settings for VPR */
typedef struct s_vpr_setup {
	boolean TimingEnabled; /* Is VPR timing enabled */
	struct s_file_name_opts FileNameOpts; /* File names */
	enum e_operation Operation; /* run VPR or do analysis only */
	t_model * user_models; /* blif models defined by the user */
	t_model * library_models; /* blif models in VPR */
	struct s_packer_opts PackerOpts; /* Options for packer */
	struct s_placer_opts PlacerOpts; /* Options for placer */
	struct s_annealing_sched AnnealSched; /* Placement option annealing schedule */
	struct s_router_opts RouterOpts; /* router options */
	struct s_det_routing_arch RoutingArch; /* routing architecture */
	t_segment_inf * Segments; /* wires in routing architecture */
    t_swseg_pattern_inf* swseg_patterns; /* Xifan TANG: Switch Segment Pattern Support */
	t_timing_inf Timing; /* timing information */
	float constant_net_delay; /* timing information when place and route not run */
	boolean ShowGraphics; /* option to show graphics */
	int GraphPause; /* user interactiveness graphics option */
	t_power_opts PowerOpts;
    t_fpga_spice_opts FPGA_SPICE_Opts; /* Xifan TANG: FPGA-SPICE support */
} t_vpr_setup;

#endif

