/***********************************/
/*  Synthesizable Verilog Dumping  */
/*       Xifan TANG, EPFL/LSI      */
/***********************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>

/* Include vpr structs*/
#include "util.h"
#include "physical_types.h"
#include "vpr_types.h"
#include "globals.h"
#include "rr_graph.h"
#include "vpr_utils.h"
#include "path_delay.h"
#include "stats.h"
#include "route_common.h"

/* Include FPGA-SPICE utils */
#include "read_xml_spice_util.h"
#include "linkedlist.h"
#include "fpga_x2p_types.h"
#include "fpga_x2p_utils.h"
#include "fpga_x2p_pbtypes_utils.h"
#include "fpga_x2p_backannotate_utils.h"
#include "fpga_x2p_globals.h"
#include "fpga_bitstream.h"

/* Include SynVerilog headers */
#include "verilog_global.h"
#include "verilog_utils.h"
#include "verilog_submodules.h"
#include "verilog_pbtypes.h"
#include "verilog_routing.h"
#include "verilog_compact_netlist.h"
#include "verilog_top_testbench.h"
#include "verilog_autocheck_top_testbench.h"
#include "verilog_formal_random_top_testbench.h"
#include "verilog_verification_top_netlist.h"
#include "verilog_modelsim_autodeck.h"
#include "verilog_report_timing.h"
#include "verilog_sdc.h"
#include "verilog_formality_autodeck.h"
#include "verilog_sdc_pb_types.h"
#include "verilog_include_netlists.h"

#include "verilog_api.h"

/***** Subroutines *****/
/* Alloc array that records Configuration bits for :
 * (1) Switch blocks
 * (2) Connection boxes
 * TODO: Can be improved in alloc strategy to be more memory efficient!
 */
static 
void alloc_global_routing_conf_bits() {
  int i;
  
  /* Alloc array for Switch blocks */
  num_conf_bits_sb = (int**)my_malloc((nx+1)*sizeof(int*));
  for (i = 0; i < (nx + 1); i++) {
    num_conf_bits_sb[i] = (int*)my_calloc((ny+1), sizeof(int));
  }

  /* Alloc array for Connection blocks */
  num_conf_bits_cbx = (int**)my_malloc((nx+1)*sizeof(int*));
  for (i = 0; i < (nx + 1); i++) {
    num_conf_bits_cbx[i] = (int*)my_calloc((ny+1), sizeof(int));
  }

  num_conf_bits_cby = (int**)my_malloc((nx+1)*sizeof(int*));
  for (i = 0; i < (nx + 1); i++) {
    num_conf_bits_cby[i] = (int*)my_calloc((ny+1), sizeof(int));
  }

  return;
}

static 
void free_global_routing_conf_bits() {
  int i;

  /* Free array for Switch blocks */
  for (i = 0; i < (nx + 1); i++) {
    my_free(num_conf_bits_sb[i]);
  }
  my_free(num_conf_bits_sb);
  
  /* Free array for Connection box */
  for (i = 0; i < (nx + 1); i++) {
    my_free(num_conf_bits_cbx[i]);
  }
  my_free(num_conf_bits_cbx);

  for (i = 0; i < (nx + 1); i++) {
    my_free(num_conf_bits_cby[i]);
  }
  my_free(num_conf_bits_cby);

  return;
}
 
/* Top-level function*/
void vpr_fpga_verilog(t_vpr_setup vpr_setup,
                      t_arch Arch,
                      char* circuit_name) {
  /* Timer */
  clock_t t_start;
  clock_t t_end;
  float run_time_sec;

  int num_clocks = Arch.spice->spice_params.stimulate_params.num_clocks;
  /* int vpr_crit_path_delay = Arch.spice->spice_params.stimulate_params.vpr_crit_path_delay; */
  
  /* Directory paths */
  char* verilog_dir_formatted = NULL;
  char* src_dir_path = NULL;
  char* submodule_dir_path= NULL;
  char* lb_dir_path = NULL;
  char* rr_dir_path = NULL;
  char* tcl_dir_path = NULL;
  char* sdc_dir_path = NULL;
  char* msim_dir_path = NULL;
  char* fm_dir_path = NULL;
  char* top_netlist_file = NULL;
  char* top_netlist_path = NULL;
  char* top_testbench_file_name = NULL;
  char* top_testbench_file_path = NULL;
  char* blif_testbench_file_name = NULL;
  char* blif_testbench_file_path = NULL;
  char* bitstream_file_name = NULL;
  char* bitstream_file_path = NULL;
  char* formal_verification_top_netlist_file_name = NULL;
  char* formal_verification_top_netlist_file_path = NULL;
  char* autocheck_top_testbench_file_name = NULL;
  char* autocheck_top_testbench_file_path = NULL;
  char* random_top_testbench_file_name = NULL;
  char* random_top_testbench_file_path = NULL;

  char* chomped_parent_dir = NULL;
  char* chomped_circuit_name = NULL;
 
  t_sram_orgz_info* sram_verilog_orgz_info = NULL;

  /* Check if the routing architecture we support*/
  if (UNI_DIRECTIONAL != vpr_setup.RoutingArch.directionality) {
    vpr_printf(TIO_MESSAGE_ERROR, "FPGA synthesizable Verilog dumping only support uni-directional routing architecture!\n");
    exit(1);
  }
  
  /* We don't support mrFPGA */
#ifdef MRFPGA_H
  if (is_mrFPGA) {
    vpr_printf(TIO_MESSAGE_ERROR, "FPGA synthesizable verilog dumping do not support mrFPGA!\n");
    exit(1);
  }
#endif
  
  assert ( TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.dump_syn_verilog);

  /* VerilogGenerator formally starts*/
  vpr_printf(TIO_MESSAGE_INFO, "\nFPGA synthesizable verilog generator starts...\n");
 
  /* Start time count */
  t_start = clock();

  /* Format the directory paths */
  split_path_prog_name(circuit_name, '/', &chomped_parent_dir, &chomped_circuit_name);

  if (NULL != vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.syn_verilog_dump_dir) {
    verilog_dir_formatted = format_dir_path(vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.syn_verilog_dump_dir);
  } else { 
    verilog_dir_formatted = format_dir_path(my_strcat(format_dir_path(chomped_parent_dir), default_verilog_dir_name));
  }

  /* SRC directory */
  src_dir_path = format_dir_path(my_strcat(verilog_dir_formatted, default_src_dir_name)); 
  /* lb directory */
  lb_dir_path = my_strcat(src_dir_path, default_lb_dir_name);
  /* routing resources directory */
  rr_dir_path = my_strcat(src_dir_path, default_rr_dir_name);
  /* submodule_dir_path */
  submodule_dir_path = my_strcat(src_dir_path, default_submodule_dir_name);
  /* SDC_dir_path */
  sdc_dir_path = my_strcat(verilog_dir_formatted, default_sdc_dir_name);
  /* tcl_dir_path */
  tcl_dir_path = my_strcat(verilog_dir_formatted, default_tcl_dir_name);
  /* msim_dir_path */
  msim_dir_path = my_strcat(verilog_dir_formatted, default_msim_dir_name);
  /* fm_dir_path */
  fm_dir_path = my_strcat(verilog_dir_formatted, default_snpsfm_dir_name);
  /* Top netlists dir_path */
  top_netlist_file = my_strcat(chomped_circuit_name, verilog_top_postfix);
  top_netlist_path = my_strcat(src_dir_path, top_netlist_file);
  /* Report timing directory */
  if (NULL == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.report_timing_path) {
    vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.report_timing_path = my_strcat(verilog_dir_formatted, default_report_timing_rpt_dir_name);
  }
  
  /* Create directories */
  create_dir_path(verilog_dir_formatted);
  create_dir_path(src_dir_path);
  create_dir_path(lb_dir_path);
  create_dir_path(rr_dir_path);
  create_dir_path(sdc_dir_path);
  create_dir_path(tcl_dir_path);
  create_dir_path(fm_dir_path);
  create_dir_path(msim_dir_path);
  create_dir_path(submodule_dir_path);

  /* assign the global variable of SRAM model */
  assert(NULL != Arch.sram_inf.verilog_sram_inf_orgz); /* Check !*/
  sram_verilog_model = Arch.sram_inf.verilog_sram_inf_orgz->spice_model;
  /* initialize the SRAM organization information struct */
  sram_verilog_orgz_info = alloc_one_sram_orgz_info();
  init_sram_orgz_info(sram_verilog_orgz_info, Arch.sram_inf.verilog_sram_inf_orgz->type, sram_verilog_model, nx + 2, ny + 2);

  /* Check all the SRAM port is using the correct SRAM SPICE MODEL */
  config_spice_models_sram_port_spice_model(Arch.spice->num_spice_model, 
                                            Arch.spice->spice_models,
                                            Arch.sram_inf.verilog_sram_inf_orgz->spice_model);

  /* Assign global variables of input and output pads */
  iopad_verilog_model = find_iopad_spice_model(Arch.spice->num_spice_model, Arch.spice->spice_models);
  assert(NULL != iopad_verilog_model);

  /* zero the counter of each spice_model */
  zero_spice_models_cnt(Arch.spice->num_spice_model, Arch.spice->spice_models);

  /* Initialize the user-defined verilog netlists to be included */
  init_list_include_verilog_netlists(Arch.spice);
 
  /* Initial global variables about configuration bits */
  alloc_global_routing_conf_bits();

  /* Initialize the number of configuration bits of all the grids */
  vpr_printf(TIO_MESSAGE_INFO, "Count the number of configuration bits, IO pads in each logic block...\n");
  /* init_grids_num_conf_bits(sram_verilog_orgz_type); */
  init_grids_num_conf_bits(sram_verilog_orgz_info);
  init_grids_num_iopads();
  /* init_grids_num_mode_bits(); */

  dump_verilog_defines_preproc(src_dir_path,
                               vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts);

  dump_verilog_simulation_preproc(src_dir_path,
                               vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts);

  /* Dump routing resources: switch blocks, connection blocks and channel tracks */
  dump_verilog_routing_resources(sram_verilog_orgz_info, src_dir_path, rr_dir_path, Arch, &vpr_setup.RoutingArch,
                                 num_rr_nodes, rr_node, rr_node_indices, rr_indexed_data,
                                 vpr_setup.FPGA_SPICE_Opts);

  /* Dump logic blocks 
   * Branches to go: 
   * 1. a compact output
   * 2. a full-size output
   */
  dump_compact_verilog_logic_blocks(sram_verilog_orgz_info, src_dir_path, 
                                    lb_dir_path, &Arch,
                                    vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.dump_explicit_verilog);

  /* Dump internal structures of submodules */
  dump_verilog_submodules(sram_verilog_orgz_info, src_dir_path, submodule_dir_path, 
                          Arch, &vpr_setup.RoutingArch, 
                          vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts);

  /* Dump top-level verilog */
  dump_compact_verilog_top_netlist(sram_verilog_orgz_info, chomped_circuit_name, 
                                   top_netlist_path, src_dir_path, submodule_dir_path, lb_dir_path, rr_dir_path, 
                                   num_rr_nodes, rr_node, rr_node_indices, 
                                   num_clocks,
                                   vpr_setup.FPGA_SPICE_Opts.compact_routing_hierarchy,
								   *(Arch.spice), vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.dump_explicit_verilog);
   
  /* Dump SDC constraints */
  /* Output SDC to contrain the P&R flow
   */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_sdc_pnr) {
    verilog_generate_sdc_pnr(sram_verilog_orgz_info, sdc_dir_path,
                             Arch, &vpr_setup.RoutingArch,
                             num_rr_nodes, rr_node, rr_node_indices, rr_indexed_data,
                             nx, ny, device_rr_gsb,
                             vpr_setup.FPGA_SPICE_Opts.compact_routing_hierarchy);
  }

  /* dump verilog testbench only for input blif */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_input_blif_testbench) {
    blif_testbench_file_name = my_strcat(chomped_circuit_name, blif_testbench_verilog_file_postfix);
    blif_testbench_file_path = my_strcat(src_dir_path, blif_testbench_file_name);
    dump_verilog_input_blif_testbench(chomped_circuit_name, blif_testbench_file_path, src_dir_path,
                                      *(Arch.spice));
    /* Free */
    my_free(blif_testbench_file_name);
    my_free(blif_testbench_file_path);
  }

  /* Free sram_orgz_info:
   * Free the allocated sram_orgz_info before, we start bitstream generation !
   */
  free_sram_orgz_info(sram_verilog_orgz_info,
                      sram_verilog_orgz_info->type);

  /* Force enable bitstream generator when we need to output Verilog top testbench*/  
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_top_testbench) {
    vpr_setup.FPGA_SPICE_Opts.BitstreamGenOpts.gen_bitstream = TRUE;
  }

  /* Generate bitstream if required, and also Dump bitstream file */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.BitstreamGenOpts.gen_bitstream) {
    bitstream_file_name = my_strcat(chomped_circuit_name, fpga_spice_bitstream_output_file_postfix);
    bitstream_file_path = my_strcat(src_dir_path, bitstream_file_name);
    /* Run bitstream generation */
    vpr_fpga_generate_bitstream(vpr_setup, Arch, circuit_name, bitstream_file_path, &sram_verilog_orgz_info);
    my_free(bitstream_file_name);
    my_free(bitstream_file_path);
  }

  /* dump verilog testbench only for top-level: ONLY valid when bitstream is generated! */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_top_testbench) {
    top_testbench_file_name = my_strcat(chomped_circuit_name, top_testbench_verilog_file_postfix);
    top_testbench_file_path = my_strcat(src_dir_path, top_testbench_file_name);
    dump_verilog_top_testbench(sram_verilog_orgz_info, chomped_circuit_name, top_testbench_file_path,
                               src_dir_path, *(Arch.spice));
    /* Free */
    my_free(top_testbench_file_name);
    my_free(top_testbench_file_path);
  }

  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_formal_verification_top_netlist) {
    formal_verification_top_netlist_file_name = my_strcat(chomped_circuit_name, formal_verification_verilog_file_postfix);
    formal_verification_top_netlist_file_path = my_strcat(src_dir_path, formal_verification_top_netlist_file_name);
    dump_verilog_formal_verification_top_netlist(sram_verilog_orgz_info, chomped_circuit_name, 
                                                 formal_verification_top_netlist_file_path, src_dir_path);
    /* Output script for formality */
    write_formality_script(vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts,
                           fm_dir_path,
                           src_dir_path,
                           chomped_circuit_name,
                           *(Arch.spice));
    random_top_testbench_file_name = my_strcat(chomped_circuit_name, random_top_testbench_verilog_file_postfix);
    random_top_testbench_file_path = my_strcat(src_dir_path, random_top_testbench_file_name);
	dump_verilog_random_top_testbench(sram_verilog_orgz_info, chomped_circuit_name, 
                                      random_top_testbench_file_path, src_dir_path,
                                      vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts, *(Arch.spice));
    /* Free */
    my_free(formal_verification_top_netlist_file_name);
    my_free(formal_verification_top_netlist_file_path);
  }

  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_autocheck_top_testbench) {
    autocheck_top_testbench_file_name = my_strcat(chomped_circuit_name, autocheck_top_testbench_verilog_file_postfix);
    autocheck_top_testbench_file_path = my_strcat(src_dir_path, autocheck_top_testbench_file_name);
    dump_verilog_autocheck_top_testbench(sram_verilog_orgz_info, chomped_circuit_name, 
                                         autocheck_top_testbench_file_path, src_dir_path, 
                                         vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts, *(Arch.spice));
    /* Free */
    my_free(autocheck_top_testbench_file_name);
    my_free(autocheck_top_testbench_file_path);
  }

  /* Output Modelsim Autodeck scripts */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_modelsim_autodeck) {
    dump_verilog_modelsim_autodeck(sram_verilog_orgz_info, 
                                   vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts,
                                   *(Arch.spice),
                                   Arch.spice->spice_params.meas_params.sim_num_clock_cycle,
                                   msim_dir_path, 
								   chomped_circuit_name,
								   src_dir_path);
  }

  /* Output SDC to contrain the mapped FPGA in timing-analysis purpose
   */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_sdc_analysis) {
    verilog_generate_sdc_analysis(sram_verilog_orgz_info, sdc_dir_path,
                                  Arch, 
                                  num_rr_nodes, rr_node, rr_node_indices, 
                                  nx, ny, grid, block, device_rr_gsb,
                                  vpr_setup.FPGA_SPICE_Opts.compact_routing_hierarchy);
  }
  /* Output routing report_timing script :
   */
  if (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_report_timing_tcl) {
    verilog_generate_report_timing(sram_verilog_orgz_info, tcl_dir_path,
                                   Arch, &vpr_setup.RoutingArch,
                                   num_rr_nodes, rr_node, rr_node_indices,
                                   vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts,
                                   vpr_setup.FPGA_SPICE_Opts.compact_routing_hierarchy);
  }

  if ((TRUE == vpr_setup.FPGA_SPICE_Opts.BitstreamGenOpts.gen_bitstream)
    || (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_top_testbench)
    || (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_autocheck_top_testbench)
    || (TRUE == vpr_setup.FPGA_SPICE_Opts.SynVerilogOpts.print_formal_verification_top_netlist)) {
    /* Free sram_orgz_info:
     * Free the allocated sram_orgz_info before, we start bitstream generation !
     */
    free_sram_orgz_info(sram_verilog_orgz_info,
                        sram_verilog_orgz_info->type);
  }

  write_include_netlists(src_dir_path,
                         chomped_circuit_name,
                         *(Arch.spice) );

  /* End time count */
  t_end = clock();
 
  run_time_sec = (float)(t_end - t_start) / CLOCKS_PER_SEC;
  vpr_printf(TIO_MESSAGE_INFO, "Synthesizable verilog dumping took %g seconds\n", run_time_sec);  

  /* Free global array */
  free_global_routing_conf_bits();

  /* Free */
  my_free(verilog_dir_formatted);
  my_free(src_dir_path);
  my_free(lb_dir_path);
  my_free(rr_dir_path);
  my_free(msim_dir_path);
  my_free(fm_dir_path);
  my_free(sdc_dir_path);
  my_free(tcl_dir_path);
  my_free(top_netlist_file);
  my_free(top_netlist_path);
  my_free(submodule_dir_path);

  return;
}
