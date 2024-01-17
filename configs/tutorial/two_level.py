# Copyright (c) 2015 Jason Power
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met: redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer;
# redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution;
# neither the name of the copyright holders nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

"""
This is the RISCV equivalent to `simple.py` (which is designed to run using the
X86 ISA). More detailed documentation can be found in `simple.py`.
"""

import m5

from m5.objects import *
from caches import * 
import argparse

import argparse
import sys
import os

import m5
from m5.defines import buildEnv
from m5.objects import *
from m5.params import NULL
from m5.util import addToPath, fatal, warn
from gem5.isas import ISA
from gem5.runtime import get_runtime_isa

addToPath("../")

from ruby import Ruby

from common import Options
from common import Simulation
from common import CacheConfig
from common import CpuConfig
from common import ObjectList
from common import MemConfig
from common.FileSystemConfig import config_filesystem
from common.Caches import *
from common.cpu2000 import *


def run_simulation(options):
    system = System()

    system.clk_domain = SrcClockDomain()
    system.clk_domain.clock = "1GHz"
    system.clk_domain.voltage_domain = VoltageDomain()

    system.mem_mode = "timing"
    system.mem_ranges = [AddrRange("512MB")]
    system.cpu = RiscvTimingSimpleCPU()

    system.membus = SystemXBar()
    
    l1_data_cache_size = options.l1_data_cache_size
    l1_data_assoc = options.l1_data_assoc
    l1_data_locking = options.l1_data_locking
    full_data_context_locking = options.full_data_context_locking

    l1_insn_cache_size = options.l1_insn_cache_size
    l1_insn_assoc = options.l1_insn_assoc
    l1_insn_locking = options.l1_insn_locking
    full_insn_context_locking = options.full_insn_context_locking
    print_addresses = options.print_addresses

    i_cache = L1ICache(size=l1_insn_cache_size, assoc=l1_insn_assoc, is_l1_cache_locking=l1_insn_locking,is_l1_cache_locking_full_context=full_insn_context_locking,print_addresses=print_addresses)
    d_cache = L1DCache(size=l1_data_cache_size, assoc=l1_data_assoc, is_l1_cache_locking=l1_data_locking,is_l1_cache_locking_full_context=full_data_context_locking,print_addresses=False)
    #d_cache = L1DCache()

    system.cpu.icache = i_cache
    system.cpu.dcache = d_cache

    system.cpu.icache.connectCPU(system.cpu)
    system.cpu.dcache.connectCPU(system.cpu)

    system.cpu.icache.connectBus(system.membus)
    system.cpu.dcache.connectBus(system.membus)

    system.cpu.createInterruptController()

    system.mem_ctrl = MemCtrl()
    system.mem_ctrl.dram = DDR3_1600_8x8()
    system.mem_ctrl.dram.range = system.mem_ranges[0]
    system.mem_ctrl.port = system.membus.mem_side_ports

    system.system_port = system.membus.cpu_side_ports
#    print(system.cpu_type)

    #thispath = os.path.dirname(os.path.realpath(__file__))
    #binary = os.path.join(
    #    thispath,
    #    "../../",
    #    "tests/test-progs/hello/bin/riscv/linux/coremark.riscv",
    #)
    #thispath = os.path.dirname(os.path.realpath(__file__))
    #binary = os.path.join(
   #     thispath,
  #      "../../../../",
 #       "coremark-pro-main/builds/riscv64/riscv-gcc64/bin/cjpeg-rose7-preset.riscv",
#)

    #path1= "san-diego-vision-benchmark/cortexsuite/cortex/clustering/kmeans/kmeans-small"
    path1 = "tacle-bench-master/bench/kernel/sha/sha.riscv"
    path2= "san-diego-vision-benchmark/cortexsuite/vision/benchmarks/disparity/data/sim/disparity"
    path3 = "san-diego-vision-benchmark/cortexsuite/cortex/cnn/main"
    path4 = "san-diego-vision-benchmark/cortexsuite/cortex/clustering/kmeans/kmeans-small"
    path5 = "bubble_sort"
    path6 = "spec2017/benchspec/CPU/505.mcf_r/build/build_base_mytest.0000/mcf_r"
    path7 = "spec2017/benchspec/CPU/619.lbm_s/build/build_base_mytest.0000/lbm_s"
    path8 = "mibench/telecomm/CRC32/crc"
    path9 = "mibench/network/dijkstra/dijkstra_small"
    path10 = "mibench/telecomm/FFT/fft"
    path11 = "bubble_sort"
    path12 = "spec2017/benchspec/CPU/541.leela_r/build/build_base_mytest.0000/leela_r"
    path13 = "san-diego-vision-benchmark/cortexsuite/cortex/clustering/spectral/spc-small"
    path14 = "mibench/security/sha/sha"
    path15 = "mibench/automotive/qsort/qsort_small"
    path16 = "mibench/automotive/bitcount/bitcnts"
    path17 = "mibench/office/stringsearch/search_large"
    path18 = "mibench/automotive/basicmath/basicmath_small"
    path19 = "mibench/network/patricia/patricia"
    path20 = "mibench/automotive/susan/susan"
    path21 = "mibench/security/blowfish/bf"
    path22 = "mibench/telecomm/adpcm/bin/rawcaudio"
    path23 = "tacle-bench-master/bench/sequential/susan/susan"
    path24 = "tacle-bench-master/bench/sequential/rijndael_enc/rijndael_enc"
    path25 = "tacle-bench-master/bench/sequential/cjpeg_wrbmp/cjpeg"
    path26 = "tacle-bench-master/bench/sequential/adpcm_enc/adcpm-enc"
    path27 = "tacle-bench-master/bench/sequential/adpcm_dec/adcpm-dec"
    path28 = "mibench/consumer/jpeg/jpeg-6a/cjpeg"
    path29 = "san-diego-vision-benchmark/cortexsuite/cortex/svd3/svd3"
    path30 = "san-diego-vision-benchmark/cortexsuite/cortex/srr/srr-small"
    path31 = "tacle-bench-master/bench/sequential/gsm_dec/gsm-dec"
    path32 = "san-diego-vision-benchmark/cortexsuite/cortex/pca/pca-small"
    path33 = "san-diego-vision-benchmark/cortexsuite/cortex/rbm/rbm-small"
    path34 = "san-diego-vision-benchmark/cortexsuite/cortex/motion-estimation/me-small"
    path35 = "tacle-bench-master/bench/kernel/bitcount/bitcount"
    path36 = "tacle-bench-master/bench/sequential/ndes/ndes"
    path37 = "mibench/consumer/typeset/lout-3.24/lout"
    path38 = "tacle-bench-master/bench/kernel/fft/fft.riscv"
    path39= "san-diego-vision-benchmark/cortexsuite/vision/benchmarks/mser/data/sim/mser"
    thispath = os.path.dirname(os.path.realpath(__file__))
    binary = os.path.join(
        thispath,
        "../../../../",
        path39,
)


    (CPUClass, sim_mem_mode, FutureClass) = Simulation.setCPUClass(options)
    system.workload = SEWorkload.init_compatible(binary)

    process = Process()
    process.cmd = [binary,"../../san-diego-vision-benchmark/cortexsuite/vision/benchmarks/mser/data/sim"]
    #process.cmd = [binary,"../../san-diego-vision-benchmark/cortexsuite/vision/benchmarks/multi_ncut/data/test"]
    #process.cmd = [binary,"../../san-diego-vision-benchmark/cortexsuite/cortex/liblinear/data/100M/crime_scale"]
    #process.cmd = [binary,"-dct","int","-progressive","-opt","-outfile","testout.jpg","../../mibench/consumer/jpeg/jpeg-6a/testimg.ppm"]
    #process.cmd = [binary,"-outfile","testout.jpg","../../mibench/consumer/jpeg/jpeg-6a/testprog.jpg"]
    #process.cmd = [binary,"../../san-diego-vision-benchmark/cortexsuite/cortex/clustering/datasets/yeast","1484","8","10"]
    #process.cmd = [binary,"../../san-diego-vision-benchmark/cortexsuite/cortex/svd3/small.txt"]
    #process.cmd = [binary,"../../san-diego-vision-benchmark/cortexsuite/cortex/clustering/datasets/R15","600","2","15","0.707","1"]
    #process.cmd=[binary,"../../spec2017/benchspec/CPU/505.mcf_r/run/run_base_refrate_mytest.0000/inp.in"]
    #process.cmd=[binary,"2000","../../spec2017/benchspec/CPU/619.lbm_s/run/run_base_refspeed_mytest.0000/reference.dat", "0", "0", "../../spec2017/benchspec/CPU/619.lbm_s/run/run_base_refspeed_mytest.0000/200_200_260_ldc.of"]
    #process.cmd=[binary,"../../mibench/network/dijkstra/input.dat"]
    #process.cmd=[binary,"../../mibench/automotive/qsort/input_small.dat"]
    #process.cmd=[binary,"../../mibench//telecomm/adpcm/data/small.pcm"]
    #process.cmd=[binary,"../../mibench/network/patricia/small.udp"]
    #process.cmd=[binary,"1125000"]
    #process.cmd=[binary]
    #process.cmd=[binary,"../../san-diego-vision-benchmark/cortexsuite/cortex/pca/small.data","100","75","R"]
    #process.cmd=[binary,"../../san-diego-vision-benchmark/cortexsuite/cortex/svd3/small.txt"]
    #process.cmd=[binary,"2>&1","../../san-diego-vision-benchmark/cortexsuite/cortex/results/srr.small"]
    #process.cmd=[binary,"4", "4096"]
    #process.cmd=[binary,"-I","../../mibench/consumer/typeset/lout-3.24/include","-D","../../mibench/consumer/typeset/lout-3.24/data","-F","../../mibench/consumer/typeset/lout-3.24/font","-C","../../mibench/consumer/typeset/lout-3.24/maps","-H","../../mibench/consumer/typeset/lout-3.24/hyph","../../mibench/consumer/typeset/small.lout"]
    #process.cmd=[binary,"../../san-diego-vision-benchmark/cortexsuite/cortex/rbm/rbm-small"]
    #process.cmd=[binary, "spec2017/benchspec/CPU/541.leela_r/run/run_base_refrate_mytest.0000/ref.sgf"]
    #process.cmd=[binary,"../../mibench/automotive/susan/input_large.pgm","output_small.smoothing.pgm","-s"]
    #process.cmd=[binary,"-","../../mibench/telecomm/adpcm/data/small.pcm","output_small.txt"]
    #process.cmd=[binary,"e","../../mibench/security/blowfish/input_small.asc","../../mibench/security/blowfish/output_small.enc","1234567890abcdeffedcba0987654321"]
    system.cpu.workload = process
    system.cpu.createThreads()

    root = Root(full_system=False, system=system)

    #m5.instantiate()
    

    print("Beginning simulation!")
    #exit_event = m5.simulate()
    Simulation.run(options,root,system,FutureClass)
    #print("Exiting @ tick %i because %s" % (m5.curTick(), exit_event.getCause()))




#parser = argparse.ArgumentParser()
parser = argparse.ArgumentParser()
Options.addCommonOptions(parser)
Options.addSEOptions(parser)
#parser.add_argument("--maxinsts", type=int, default=10, help="Maximum number of instructions to simulate")
parser.add_argument("--l1_data_cache_size", type=str, default="8kB", help="L1 cache size")
parser.add_argument("--l1_data_assoc", type=int, default=4, help="L1 cache associativity")
parser.add_argument("--l1_data_locking",type=bool,default=False, help="Enable L1 cache locking")
parser.add_argument("--full_data_context_locking",type=bool,default=False, help="Enable L1 cache locking")
#parser.add_argument("--maxinsts", type="int", default=1)
parser.add_argument("--l1_insn_cache_size", type=str, default="8kB", help="L1 cache size")
parser.add_argument("--l1_insn_assoc", type=int, default=16, help="L1 cache associativity")
parser.add_argument("--l1_insn_locking",type=bool,default=False, help="Enable L1 cache locking")
parser.add_argument("--full_insn_context_locking",type=bool,default=False, help="Enable L1 cache locking")
parser.add_argument("--print_addresses",type=bool,default=False, help="Enable L1 cache locking")
options = parser.parse_args()

#parser = argparse.ArgumentParser()


#options=""
run_simulation(options)
#run_simulation("32kB", 16,False,"")