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
This is the ARM equivalent to `simple.py` (which is designed to run using the
X86 ISA). More detailed documentation can be found in `simple.py`.
"""

import m5
import os
from m5.objects import *
from caches import * 

# Instantiate the system object
system = System()

system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "1GHz"
system.clk_domain.voltage_domain = VoltageDomain()

system.mem_mode = "timing"
system.mem_ranges = [AddrRange("512MB")]
system.cpu = RiscvTimingSimpleCPU()

system.membus = SystemXBar()

system.cpu.icache = L1ICache()
system.cpu.dcache = L1DCache()

system.cpu.dcache.prefetcher = IndirectMemoryPrefetcher()
system.cpu.icache.prefetcher = IndirectMemoryPrefetcher()

#system.cpu.icache.prefetch_on_access = True
#system.cpu.dcache.prefetch_on_access = True
#70742281000
system.cpu.icache.connectCPU(system.cpu)
system.cpu.dcache.connectCPU(system.cpu)

system.cpu.icache.connectBus(system.membus)
system.cpu.dcache.connectBus(system.membus)

# Create interrupt controller
system.cpu.createInterruptController()

#system.physmem = SimpleMemory()
#system.memdelay_inst = SimpleMemDelay()
#system.memdelay_data = SimpleMemDelay()

#system.memdelay_inst.read_req = "20ns"
#system.memdelay_inst.write_req = "20ns"

#system.memdelay_data.read_req = "20ns"
#system.memdelay_data.write_req = "20ns"

#system.memdelay_inst.mem_side_port = system.membus.cpu_side_ports
#system.cpu.icache_port = system.memdelay_inst.cpu_side_port

#system.memdelay_data.mem_side_port = system.membus.cpu_side_ports
#system.cpu.dcache_port = system.memdelay_data.cpu_side_port

#system.mem_ctrl.dram = SRAM()
#system.mem_ctrl.dram = MemInterface()

#system.mem_ctrl.dram.range = system.mem_ranges[0]

#system.cpu.icache_port = system.membus.cpu_side_ports
#system.cpu.dcache_port = system.membus.cpu_side_ports


system.physmem = SimpleMemory()
#system.physmem.refill_period = 5000000000
#system.physmem.demand_burstiness =40
system.physmem.port = system.membus.mem_side_ports
system.system_port = system.membus.cpu_side_ports
 






# Define the binary to be executed
thispath = os.path.dirname(os.path.realpath(__file__))
binary = os.path.join(
    thispath,
    "../../../../",
    "tacle-bench-master/bench/kernel/sha/sha.riscv",
)

# Set workload for the CPU
system.workload = SEWorkload.init_compatible(binary)

# Create a process object and set its command to the binary
process = Process()
process.cmd = [binary]
system.cpu.workload = process

# Create threads for the CPU
system.cpu.createThreads()

# Create root node
root = Root(full_system=False, system=system)

# Instantiate the system
m5.instantiate()

# Run simulation
print("Beginning simulation!")
exit_event = m5.simulate()
print("Exiting @ tick %i because %s" % (m5.curTick(), exit_event.getCause()))
