#! /bin/bash
# Exit if error occurs
set -e
# Make sure a clear start
pwd_path="$PWD"
task_name="tuto"
config_file="$pwd_path/configs/tutorial/${task_name}.conf" 
bench_txt="$pwd_path/benchmarks/List/tuto_benchmark.txt"
rpt_file="$pwd_path/csv_rpts/fpga_spice/${task_name}.csv"
verilog_path="${pwd_path}/${task_name}_Verilog"
architecture_generated="${pwd_path}/arch/generated/k6_N10_sram_chain_HC.xml"
architecture_template="${pwd_path}/arch/template/k6_N10_sram_chain_HC_template.xml"

ff_keyword="FFPATHKEYWORD"
ff_template_path="${pwd_path}/../vpr7_x2p/vpr/VerilogNetlists/ff.v"
ff_path="${pwd_path}/../vpr7_x2p/vpr/VerilogNetlists/ff_${task_name}.v"
dir_keyword="GENERATED_DIR_KEYWORD"

rm -rf ${pwd_path}/results_OpenPithon

cd ${pwd_path}/arch
mkdir -p generated	# create folder to save rewritten architecture
cd ${pwd_path}/scripts

# Replace keyword in config and architecture files
perl rewrite_path_in_file.pl -i $config_file	# Replace OPENFPGAPATHKEYWORD in the config file
perl rewrite_path_in_file.pl -i $architecture_template -o $architecture_generated	# Replace OPENFPGAPATHKEYWORD in the architecture file
perl rewrite_path_in_file.pl -i $architecture_generated -k $ff_keyword $ff_path	# Set the ff path in the architecture file
echo "perl rewrite_path_in_file.pl -i $ff_path -k $dir_keyword $verilog_path"
perl rewrite_path_in_file.pl -i $ff_template_path -o $ff_path -k $dir_keyword $verilog_path	# Set the define path in the ff.v file


# SRAM FPGA
# TT case 
perl fpga_flow.pl -conf ${config_file} -benchmark ${bench_txt} -rpt ${rpt_file} -N 10 -K 6 -ace_d 0.5 -multi_thread 1 -vpr_fpga_x2p_rename_illegal_port -vpr_fpga_verilog -vpr_fpga_verilog_dir $verilog_path -vpr_fpga_bitstream_generator -vpr_fpga_verilog_print_autocheck_top_testbench -vpr_fpga_verilog_include_timing -vpr_fpga_verilog_include_signal_init -vpr_fpga_verilog_formal_verification_top_netlist -fix_route_chan_width -vpr_fpga_verilog_include_icarus_simulator -power -vpr_fpga_verilog_print_user_defined_template -vpr_fpga_verilog_print_report_timing_tcl -vpr_fpga_verilog_print_sdc_pnr -vpr_fpga_verilog_print_sdc_analysis -vpr_fpga_x2p_compact_routing_hierarchy -end_flow_with_test -vpr_fpga_verilog_explicit_mapping

echo "Netlists successfully generated and tested"
