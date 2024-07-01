import m5
import argparse

# import all of the SimObjects
from m5.objects import *
from gem5.runtime import get_runtime_isa

# Add the common scripts to our path
m5.util.addToPath("../")

# import the caches which we made
from caches import *

# Create an ArgumentParser object and define the --cmd argument
parser = argparse.ArgumentParser()
parser.add_argument('--cmd', help='The binary to run on the simulated system')

# Parse the command-line arguments and store the result in args
args = parser.parse_args()

system = System()

# Set the clock frequency of the system (and all of its children)
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = "3GHz"
system.clk_domain.voltage_domain = VoltageDomain()

# Set up the system
system.mem_mode = "timing"  # Use timing accesses
system.mem_ranges = [AddrRange("32GB")]  # Create an address range

# Create a simple CPU
# system.cpu = X86TimingSimpleCPU()
system.cpu = DerivO3CPU()
# Create an L1 instruction and data cache



system.cpu.icache = L1ICache()
system.cpu.dcache = L1DCache()

# Connect the instruction and data caches to the CPU
system.cpu.icache.connectCPU(system.cpu)
system.cpu.dcache.connectCPU(system.cpu)

# Create a memory bus, a coherent crossbar, in this case
# system.l2bus = L2XBar()

# Hook the CPU ports up to the l2bus
# system.cpu.icache.connectBus(system.l2bus)
# system.cpu.dcache.connectBus(system.l2bus)

system.membus = SystemXBar()

system.cpu.icache.connectBus(system.membus)
system.cpu.dcache.connectBus(system.membus)

# # Create an L2 cache and connect it to the l2bus
# system.l2cache = L2Cache()
# system.l2cache.connectCPUSideBus(system.l2bus)


# system.l3cache = L3Cache()
# system.l3bus = L2XBar()
# # Connect the L2 cache to the l3bus
# system.l2cache.connectMemSideBus(system.l3bus)

# # Connect the L3 cache to the l3bus
# system.l3cache.connectCPUSideBus(system.l3bus)


# # Create a memory bus
# system.membus = SystemXBar()

# # Connect the L2 cache to the membus
# #system.l2cache.connectMemSideBus(system.membus)
# system.l3cache.connectMemSideBus(system.membus)
# # create the interrupt controller for the CPU
system.cpu.createInterruptController()
system.cpu.interrupts[0].pio = system.membus.mem_side_ports
system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

# # Connect the system up to the membus
system.system_port = system.membus.cpu_side_ports

# Create a DDR3 memory controller
system.mem_ctrl = MemCtrl()
#system.mem_ctrl.dram = DDR3_1600_8x8()
#system.mem_ctrl.dram = NVM_2400_1x64()
system.mem_ctrl.dram = DDR4_2400_16x4()

system.mem_ctrl.dram.read_buffer_size = 512  # Number of entries in read queue
system.mem_ctrl.dram.write_buffer_size =512 # Number of entries in write queue

system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports

system.workload = SEWorkload.init_compatible(args.cmd)

# Create a process for a simple "Hello World" application
process = Process()
# Set the command
# cmd is a list which begins with the executable (like argv)
process.cmd = [args.cmd]
# Set the cpu to use the process as its workload and create thread contexts
system.cpu.workload = process
system.cpu.createThreads()

# set up the root SimObject and start the simulation
root = Root(full_system=False, system=system)
# instantiate all of the objects we've created above
m5.instantiate()

print("Beginning simulation!")
exit_event = m5.simulate()
print("Exiting @ tick %i because %s" % (m5.curTick(), exit_event.getCause()))

