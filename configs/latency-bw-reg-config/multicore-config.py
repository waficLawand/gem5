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
from m5.objects import *
m5.util.addToPath("../")
from common.FileSystemConfig import config_filesystem
from common import Options

parser = argparse.ArgumentParser()
Options.addCommonOptions(parser)
Options.addSEOptions(parser)
# Adding new argument for prefetcher type
parser.add_argument("--prefetcher_type", type=str, default="None",
                    help="Type of prefetcher: None, Stride, BOP, AMPMPrefetcher, SignaturePathPrefetcher, DCPTPrefetcher")
parser.add_argument("--demand_burstiness", type=int, default=10)
parser.add_argument("--prefetch_burstiness", type=int, default=100)
parser.add_argument("--latency_in_ticks", type=int, default=30000)
parser.add_argument("--is_fcfs", type=int, default=1)
parser.add_argument("--is_duetto", type=int, default=0)
parser.add_argument("--latency_slack", type=int, default=60)
parser.add_argument("--num_requestors", type=int, default=8)
parser.add_argument("--benchmark", type=str)
parser.add_argument("--with_prefetchers", type=int, default=0)
sys_args = parser.parse_args()

system = System()

system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "2GHz"
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = "timing"
system.mem_ranges = [AddrRange("512MB")]


# Instantiating CPUs
system.cpu = [RiscvO3CPU() for i in range(sys_args.num_requestors)]

if sys_args.is_duetto == 1:
    print("HERE!!!~\n")
    system.mem_ctrl = DuettoSimpleMem()
    system.mem_ctrl.is_fcfs = True
    system.mem_ctrl.latency_slack = sys_args.latency_slack
    system.mem_ctrl.num_requestors = sys_args.num_requestors
    system.mem_ctrl.latency_in_ticks = sys_args.latency_in_ticks
    system.mem_ctrl.request_delta = 480
    #system.mem_ctrl.bandwidth = "8.5GiB/s"

else:
    print("HELOOOOOOO!!!~")
    #system.mem_ctrl = SimpleMemory()
    system.mem_ctrl = LatencyRegulatedSimpleMem()
    system.mem_ctrl.is_fcfs = bool(sys_args.is_fcfs)
    system.mem_ctrl.requestors = sys_args.num_requestors
    system.mem_ctrl.ticks_per_cycle = sys_args.latency_in_ticks
    #system.mem_ctrl.bandwidth = "8.5GiB/s"


system.l2bus = L2XBar()

# create the interrupt controller for the CPU and connect to the membus
i=0
for cpu in system.cpu:
    #cpu.createInterruptController()
    cpu.icache = L1ICache()
    cpu.dcache = L1DCache()
    
    cpu.icache.connectCPU(cpu)
    cpu.dcache.connectCPU(cpu)

    cpu.icache.connectBus(system.l2bus)
    cpu.dcache.connectBus(system.l2bus)
    
    if sys_args.with_prefetchers == 1:
        print("HEREE!!!!!!!!!")
        # Conditional statement to select the prefetcher type based on the command-line argument
        if sys_args.prefetcher_type == "StridePrefetcher":
            cpu.dcache.prefetcher = StridePrefetcher()
        elif sys_args.prefetcher_type == "BOPPrefetcher":
            cpu.dcache.prefetcher = BOPPrefetcher()
        elif sys_args.prefetcher_type == "AMPMPrefetcher":
            cpu.dcache.prefetcher = AMPMPrefetcher()
        elif sys_args.prefetcher_type == "SignaturePathPrefetcher":
            cpu.dcache.prefetcher = SignaturePathPrefetcher()
        elif sys_args.prefetcher_type == "DCPTPrefetcher":
            cpu.dcache.prefetcher = DCPTPrefetcher()
        else:
            print("No valid prefetcher selected. Continuing without prefetcher.")

    i = i+1



system.l2cache = L2Cache()
#system.l2cache.prefetcher = StridePrefetcher()
system.l2cache.connectCPUSideBus(system.l2bus)



system.membus = SystemXBar()
system.l2cache.connectMemSideBus(system.membus)

system.mem_ctrl.port = system.membus.mem_side_ports

for i in range(sys_args.num_requestors):
    system.cpu[i].createInterruptController()

        
# Run application and use the compiled ISA to find the binary
# grab the specific path to the binary
thispath = os.path.dirname(os.path.realpath(__file__))

"""for cpu in system.cpu:
    cpu.interrupts[0].pio = system.membus.mem_side_ports
    cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
    cpu.interrupts[0].int_responder = system.membus.mem_side_ports"""

binary0 = os.path.join(
    thispath,
    "../../../../",
    "san-diego-vision-benchmark/cortexsuite/vision/benchmarks/"+sys_args.benchmark+"/data/sim/"+sys_args.benchmark
    #"san-diego-vision-benchmark/cortexsuite/vision/benchmarks/isolbench/bench/bandwidth"
)

#binary0 = os.path.join(
#    thispath,
#    "../../../../", "PolyBenchC-4.2.1-master/atax_time"
#)


binary1 = os.path.join(
    thispath,
    "../../../../",
    "san-diego-vision-benchmark/cortexsuite/vision/benchmarks/isolbench/bench/bandwidth"
)

binary2 = os.path.join(
    thispath,
    "../../../../",
    "san-diego-vision-benchmark/cortexsuite/vision/benchmarks/isolbench/bench/bandwidth"
)

binary3 = os.path.join(
    thispath,
    "../../../../",
    "san-diego-vision-benchmark/cortexsuite/vision/benchmarks/isolbench/bench/bandwidth"

    
)


binary4 = os.path.join(
    thispath,
    "../../../../",
    "san-diego-vision-benchmark/cortexsuite/vision/benchmarks/isolbench/bench/bandwidth"
)

binary5 = os.path.join(
    thispath,
    "../../../../",
    "san-diego-vision-benchmark/cortexsuite/vision/benchmarks/isolbench/bench/bandwidth"
)

binary6 = os.path.join(
    thispath,
    "../../../../",
    "san-diego-vision-benchmark/cortexsuite/vision/benchmarks/isolbench/bench/bandwidth"
)

binary7 = os.path.join(
    thispath,
    "../../../../",
    "san-diego-vision-benchmark/cortexsuite/vision/benchmarks/isolbench/bench/bandwidth"
)


processes = []


process = Process()
process.cmd = [binary0, "../../san-diego-vision-benchmark/cortexsuite/vision/benchmarks/"+sys_args.benchmark+"/data/sim"]
#process.cmd = [binary0, ""]
process.pid = 100 + 1  # Ensure unique PID for each process
processes.append(process)

process = Process()
process.cmd = [binary1, ""]
process.pid = 100 + 2  # Ensure unique PID for each process
processes.append(process)

process = Process()
process.cmd = [binary2, ""]
process.pid = 100 + 3  # Ensure unique PID for each process
processes.append(process)

process = Process()
process.cmd = [binary3, ""]
process.pid = 100 + 4  # Ensure unique PID for each process
processes.append(process)

process = Process()
process.cmd = [binary4, ""]
process.pid = 100 + 5  # Ensure unique PID for each process
processes.append(process)

process = Process()
process.cmd = [binary5, ""]
process.pid = 100 + 6  # Ensure unique PID for each process
processes.append(process)

process = Process()
process.cmd = [binary6, ""]
process.pid = 100 + 7  # Ensure unique PID for each process
processes.append(process)

process = Process()
process.cmd = [binary7, ""]
process.pid = 100 + 8  # Ensure unique PID for each process
processes.append(process)


"""for i in range(1,4):
    process = Process()
    process.cmd = [binary, "../../san-diego-vision-benchmark/cortexsuite/vision/benchmarks/" + sys_args.benchmark + "/data/sim"]
    process.pid = 100 + i  # Ensure unique PID for each process
    processes.append(process)"""

# Create a process for a simple "multi-threaded" application
#process = Process()
# Set the command
# cmd is a list which begins with the executable (like argv)

#process.cmd = [binary]
#process.cmd = [binary,"../../san-diego-vision-benchmark/cortexsuite/vision/benchmarks/"+sys_args.benchmark+"/data/sim"]
# Set the cpu to use the process as its workload and create thread contexts
x = 0
for cpu in system.cpu:
    print(cpu)
    cpu.workload = processes[x]
    #cpu.max_insts_all_threads = 10000000

    cpu.createThreads()
    x = x+1

system.workload = SEWorkload.init_compatible(binary1)


# Set up the pseudo file system for the threads function above
config_filesystem(system)

# set up the root SimObject and start the simulation
root = Root(full_system=False, system=system)
# instantiate all of the objects we've created above



m5.instantiate()




print("Beginning simulation!")
exit_event = m5.simulate(15000000000)
print(
    "Exiting @ tick {} because {}".format(m5.curTick(), exit_event.getCause())
)