/*
 * Copyright (c) 2010-2020 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2013 Amin Farmahini-Farahani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <vector>
#include <queue>
#include <unordered_set>

#include <cmath>

#include "mem/mem_ctrl.hh"

#include "base/trace.hh"
#include "debug/DRAM.hh"
#include "debug/Drain.hh"
#include "debug/MemCtrl.hh"
#include "debug/NVM.hh"
#include "debug/QOS.hh"
#include "mem/packet.hh"
#include "mem/dram_interface.hh"
#include "mem/mem_interface.hh"
#include "mem/nvm_interface.hh"
#include "mem/request.hh"
#include "sim/system.hh"
#include "base/debug.hh"
#include "base/types.hh"



const int N = 1; 

const int baseCounter = 0x10000;

const int baseNodes = 0x200;

const int Leaves_size = 100;
int Number_of_nodes_need_to_read = std::ceil(std::log2(Leaves_size));



const uint64_t L2Delay = 3500;



const uint64_t L3Delay = 25000;



class AddressCache {
public:
    AddressCache(int size) : maxSize(size) {}

    bool isInCache(gem5::Addr address) {
        cacheAccesses++;
        auto it = cacheSet.find(address);
        if (it != cacheSet.end()) {
            // Cache hit
            return true;
        } else {
            // Cache miss
            cacheMisses++;
            if (cacheQueue.size() >= maxSize) {
                gem5::Addr oldAddress = cacheQueue.front();
                cacheQueue.pop();
                cacheSet.erase(oldAddress);
            }
            cacheQueue.push(address);
            cacheSet.insert(address);
            return false;
        }
    }

    double getMissRate() const {
        if (cacheAccesses == 0) return 0.0;
        return static_cast<double>(cacheMisses) / static_cast<double>(cacheAccesses);
    }
    void printCacheContents() const {
        for (const auto& addr : cacheSet) {  // Assuming cacheSet is your cache data structure
            DPRINTF(MemCtrl, "Cache Content: 0x%x\n", addr);
        }
    }


private:
    int maxSize;
    std::queue<gem5::Addr> cacheQueue;
    std::unordered_set<gem5::Addr> cacheSet;
    int cacheMisses = 0;
    int cacheAccesses = 0;
};



class DataCache {
public:
    DataCache(int size) : maxSize(size) {}
    bool isInCache(gem5::Addr address) {
        cacheAccesses++;
        // Convert the address to a block address by ignoring the lower bits
        gem5::Addr blockAddress = address >> 5;  // For 64-byte blocks, shift right by 6 bits

        auto it = cacheSet.find(blockAddress);
        if (it != cacheSet.end()) {
            // Cache hit
            // Move the block to the back of the queue to mark it as recently used
            updateLRU(blockAddress);
            return true;
        } else {
            // Cache miss
            cacheMisses++;
            if (cacheQueue.size() >= maxSize) {
                // Evict the least recently used block
                gem5::Addr oldBlockAddress = cacheQueue.front();
                cacheQueue.pop();
                cacheSet.erase(oldBlockAddress);
            }
            cacheQueue.push(blockAddress);
            cacheSet.insert(blockAddress);
            return false;
        }
    }

    void updateLRU(gem5::Addr blockAddress) {
        // Remove the block from its current position in the queue
        std::queue<gem5::Addr> tempQueue;
        while (!cacheQueue.empty()) {
            gem5::Addr currentAddress = cacheQueue.front();
            cacheQueue.pop();
            if (currentAddress != blockAddress) {
                tempQueue.push(currentAddress);
            }
        }

        // Add the block to the back of the queue
        tempQueue.push(blockAddress);
        std::swap(cacheQueue, tempQueue);
    }


    double getMissRate() const {
        if (cacheAccesses == 0) return 0.0;
        return static_cast<double>(cacheMisses) / static_cast<double>(cacheAccesses);
    }

    void printCacheContents() const {
        for (const auto& blockAddr : cacheSet) {
            DPRINTF(MemCtrl, "Cache Block Content: 0x%x\n", blockAddr << 6);
        }
    }

private:
    int maxSize;
    std::queue<gem5::Addr> cacheQueue;
    std::unordered_set<gem5::Addr> cacheSet;
    unsigned long long cacheAccesses = 0;
    unsigned long long cacheMisses = 0;
};




class CounterCache {
public:
    CounterCache(int size) : maxSize(size) {}
    bool isInCache(gem5::Addr address) {
        cacheAccesses++;
        // Convert the address to a block address by ignoring the lower bits
        gem5::Addr blockAddress = address >> 6;  // For 64-byte blocks, shift right by 6 bits

        auto it = cacheSet.find(blockAddress);
        if (it != cacheSet.end()) {
            // Cache hit
            // Move the block to the back of the queue to mark it as recently used
            updateLRU(blockAddress);
            return true;
        } else {
            // Cache miss
            cacheMisses++;
            if (cacheQueue.size() >= maxSize) {
                // Evict the least recently used block
                gem5::Addr oldBlockAddress = cacheQueue.front();
                cacheQueue.pop();
                cacheSet.erase(oldBlockAddress);
            }
            cacheQueue.push(blockAddress);
            cacheSet.insert(blockAddress);
            return false;
        }
    }

    void updateLRU(gem5::Addr blockAddress) {
        // Remove the block from its current position in the queue
        std::queue<gem5::Addr> tempQueue;
        while (!cacheQueue.empty()) {
            gem5::Addr currentAddress = cacheQueue.front();
            cacheQueue.pop();
            if (currentAddress != blockAddress) {
                tempQueue.push(currentAddress);
            }
        }

        // Add the block to the back of the queue
        tempQueue.push(blockAddress);
        std::swap(cacheQueue, tempQueue);
    }


    double getMissRate() const {
        if (cacheAccesses == 0) return 0.0;
        return static_cast<double>(cacheMisses) / static_cast<double>(cacheAccesses);
    }

    void printCacheContents() const {
        for (const auto& blockAddr : cacheSet) {
            DPRINTF(MemCtrl, "Cache Block Content: 0x%x\n", blockAddr << 6);
        }
    }

private:
    int maxSize;
    std::queue<gem5::Addr> cacheQueue;
    std::unordered_set<gem5::Addr> cacheSet;
    unsigned long long cacheAccesses = 0;
    unsigned long long cacheMisses = 0;
};




// Global cache object
const int CACHE_SIZE = 131072/8; 

const int L2_CACHE_SIZE = 4096; 

const int L3_CACHE_SIZE = 131072; 
CounterCache globalCache(CACHE_SIZE);

DataCache L2Cache(L2_CACHE_SIZE);

DataCache L3Cache(L3_CACHE_SIZE);



namespace gem5
{

namespace memory
{

MemCtrl::MemCtrl(const MemCtrlParams &p) :
    qos::MemCtrl(p),
    port(name() + ".port", *this), isTimingMode(false),
    retryRdReq(false), retryWrReq(false),
    nextReqEvent([this] {processNextReqEvent(dram, respQueue,
                         respondEvent, nextReqEvent, retryWrReq);}, name()),
    respondEvent([this] {processRespondEvent(dram, respQueue,
                         respondEvent, retryRdReq); }, name()),
    dram(p.dram),
    readBufferSize(dram->readBufferSize),
    writeBufferSize(dram->writeBufferSize),
    writeHighThreshold(writeBufferSize * p.write_high_thresh_perc / 100.0),
    writeLowThreshold(writeBufferSize * p.write_low_thresh_perc / 100.0),
    minWritesPerSwitch(p.min_writes_per_switch),
    minReadsPerSwitch(p.min_reads_per_switch),
    memSchedPolicy(p.mem_sched_policy),
    frontendLatency(p.static_frontend_latency),
    backendLatency(p.static_backend_latency),
    commandWindow(p.command_window),
    prevArrival(0),
    stats(*this)
{
    DPRINTF(MemCtrl, "Setting up controller\n");

    readQueue.resize(p.qos_priorities);
    writeQueue.resize(p.qos_priorities);

    dram->setCtrl(this, commandWindow);

    // perform a basic check of the write thresholds
    if (p.write_low_thresh_perc >= p.write_high_thresh_perc)
        fatal("Write buffer low threshold %d must be smaller than the "
              "high threshold %d\n", p.write_low_thresh_perc,
              p.write_high_thresh_perc);
    if (p.disable_sanity_check) {
        port.disableSanityCheck();
    }
}

void
MemCtrl::init()
{
   if (!port.isConnected()) {
        fatal("MemCtrl %s is unconnected!\n", name());
    } else {
        port.sendRangeChange();
    }
}


std::vector<Addr> addressMapping(Addr address) {
    std::vector<Addr> addresses;
    for (int i = 0; i < N; i++) {
        addresses.push_back(address ^ (i + 1));
    }
    return addresses;
}

std::vector<Addr> addressMapping_MT(Addr address) {
    std::vector<Addr> addresses;
    for (int i = 0; i < Number_of_nodes_need_to_read; i++) {
        addresses.push_back(address + (baseNodes + i));
         // addresses.push_back(address ^ (i + 1));
    }
    return addresses;
}


// std::pair<std::vector<Addr>, std::vector<bool>> addressMapping_read(Addr address) {
//     std::vector<Addr> addresses;
//     std::vector<bool> cacheMisses;

//     for (int i = 0; i < N; i++) {
//         Addr counterAddr = address + (baseCounter + i);

//         // Addr counterAddr = address ^ (i + 1);
//         addresses.push_back(counterAddr);
//         bool isMiss = !globalCache.isInCache(counterAddr);
//         cacheMisses.push_back(isMiss);

//         if (!isMiss) {
//             DPRINTF(MemCtrl, "Cache Hit: 0x%x\n", counterAddr);
//         } else {
//             DPRINTF(MemCtrl, "Cache Miss: 0x%x\n", counterAddr);
//         }
//     }

//     // Print the current cache miss rate
//     DPRINTF(MemCtrl, "Current Cache Miss Rate: %.2f%%\n", globalCache.getMissRate() * 100);
//     // globalCache.printCacheContents();

//     return {addresses, cacheMisses};
// }


bool Counter_cache(Addr address){

     bool isMiss = !globalCache.isInCache(address + baseCounter);
     if (!isMiss) {
        DPRINTF(MemCtrl, "Counter Cache Hit: 0x%x\n", address);
    } else {
        DPRINTF(MemCtrl, "Counter Cache Miss: 0x%x\n", address);
    }
    DPRINTF(MemCtrl, "Current Cache Miss Rate: %.2f%%\n", globalCache.getMissRate() * 100);

    return isMiss;


}

bool L2_cache(Addr address){

     bool isMiss = !L2Cache.isInCache(address);
     if (!isMiss) {
        DPRINTF(MemCtrl, "L2 Cache Hit: 0x%x\n", address);
    } else {
        DPRINTF(MemCtrl, "L2 Cache Miss: 0x%x\n", address);
    }
    DPRINTF(MemCtrl, "Current Cache Miss Rate: %.2f%%\n", L2Cache.getMissRate() * 100);

    return isMiss;


}

bool L3_cache(Addr address){

     bool isMiss = !L3Cache.isInCache(address);
     if (!isMiss) {
        DPRINTF(MemCtrl, "L3 Cache Hit: 0x%x\n", address);
    } else {
        DPRINTF(MemCtrl, "L3 Cache Miss: 0x%x\n", address);
    }

    DPRINTF(MemCtrl, "Current Cache Miss Rate: %.2f%%\n", L3Cache.getMissRate() * 100);

    return isMiss;


}




std::vector<uint8_t> splitData(uint8_t data) {
    std::vector<uint8_t> shares(N);

    // Random number generator
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, data / N);  // Ensure we don't generate values that would exceed the original data

    // Generate N-1 random shares
    uint8_t sum_shares = 0;
    for (int i = 0; i < N - 1; i++) {
        shares[i] = static_cast<uint8_t>(dis(gen));
        sum_shares += shares[i];
    }

    // Calculate the Nth share such that the sum of all shares equals the original data
    shares[N-1] = data - sum_shares;

    return shares;
}

std::uint64_t reconstructData(const std::vector<uint8_t>& shares) {
    uint8_t sum = 0;
    for(const auto& share : shares) {
        sum += share;
    }
    return sum;
}


void
MemCtrl::startup()
{
    // remember the memory system mode of operation
    isTimingMode = system()->isTimingMode();

    if (isTimingMode) {
        // shift the bus busy time sufficiently far ahead that we never
        // have to worry about negative values when computing the time for
        // the next request, this will add an insignificant bubble at the
        // start of simulation
        dram->nextBurstAt = curTick() + dram->commandOffset();
    }
}

Tick
MemCtrl::recvAtomic(PacketPtr pkt)
{
    if (!dram->getAddrRange().contains(pkt->getAddr())) {
        panic("Can't handle address range for packet %s\n", pkt->print());
    }

    return recvAtomicLogic(pkt, dram);
}


Tick
MemCtrl::recvAtomicLogic(PacketPtr pkt, MemInterface* mem_intr)
{
    DPRINTF(MemCtrl, "recvAtomic: %s 0x%x\n",
                     pkt->cmdString(), pkt->getAddr());

    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    // do the actual memory access and turn the packet into a response
    mem_intr->access(pkt);

    if (pkt->hasData()) {
        // this value is not supposed to be accurate, just enough to
        // keep things going, mimic a closed page
        // also this latency can't be 0
        return mem_intr->accessLatency();
    }

    return 0;
}





Tick
MemCtrl::recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &backdoor)
{
    Tick latency = recvAtomic(pkt);
    dram->getBackdoor(backdoor);
    return latency;
}

bool
MemCtrl::readQueueFull(unsigned int neededEntries) const
{
    DPRINTF(MemCtrl,
            "Read queue limit %d, current size %d, entries needed %d\n",
            readBufferSize, totalReadQueueSize + respQueue.size(),
            neededEntries);

    auto rdsize_new = totalReadQueueSize + respQueue.size() + neededEntries;
    return rdsize_new > readBufferSize;
}

bool
MemCtrl::writeQueueFull(unsigned int neededEntries) const
{
    DPRINTF(MemCtrl,
            "Write queue limit %d, current size %d, entries needed %d\n",
            writeBufferSize, totalWriteQueueSize, neededEntries);

    auto wrsize_new = (totalWriteQueueSize + neededEntries);
    return  wrsize_new > writeBufferSize;
}

bool
MemCtrl::addToReadQueue(PacketPtr pkt,
                unsigned int pkt_count, MemInterface* mem_intr)
{
    // only add to the read queue here. whenever the request is
    // eventually done, set the readyTime, and call schedule()
    assert(!pkt->isWrite());

    assert(pkt_count != 0);

    DPRINTF(MemCtrl, "Received a read request for addr %#x of size %d\n",
            pkt->getAddr(), pkt->getSize());

    // if the request size is larger than burst size, the pkt is split into
    // multiple packets
    // Note if the pkt starting address is not aligened to burst size, the
    // address of first packet is kept unaliged. Subsequent packets
    // are aligned to burst size boundaries. This is to ensure we accurately
    // check read packets against packets in write queue.
    const Addr base_addr = pkt->getAddr();
    Addr addr = base_addr;
    unsigned pktsServicedByWrQ = 0;
    BurstHelper* burst_helper = NULL;

    uint32_t burst_size = mem_intr->bytesPerBurst();

    for (int cnt = 0; cnt < pkt_count; ++cnt) {
        unsigned size = std::min((addr | (burst_size - 1)) + 1,
                        base_addr + pkt->getSize()) - addr;
        stats.readPktSize[ceilLog2(size)]++;
        stats.readBursts++;
        stats.requestorReadAccesses[pkt->requestorId()]++;

        // First check write buffer to see if the data is already at
        // the controller
        bool foundInWrQ = false;
        Addr burst_addr = burstAlign(addr, mem_intr);
        // if the burst address is not present then there is no need
        // looking any further
        if (isInWriteQueue.find(burst_addr) != isInWriteQueue.end()) {
            for (const auto& vec : writeQueue) {
                for (const auto& p : vec) {
                    // check if the read is subsumed in the write queue
                    // packet we are looking at
                    if (p->addr <= addr &&
                       ((addr + size) <= (p->addr + p->size))) {

                        foundInWrQ = true;
                        stats.servicedByWrQ++;
                        pktsServicedByWrQ++;
                        DPRINTF(MemCtrl,
                                "Read to addr %#x with size %d serviced by "
                                "write queue\n",
                                addr, size);
                        stats.bytesReadWrQ += burst_size;
                        break;
                    }
                }
            }
        }

        // If not found in the write q, make a memory packet and
        // push it onto the read queue
        if (!foundInWrQ) {
             DPRINTF(MemCtrl, "Read request for addr %#x not found in WriteQ, adding to ReadQ\n", addr);


            // Make the burst helper for split packets
            if (pkt_count > 1 && burst_helper == NULL) {
                DPRINTF(MemCtrl, "Read to addr %#x translates to %d "
                        "memory requests\n", pkt->getAddr(), pkt_count);
                burst_helper = new BurstHelper(pkt_count);
            }

            MemPacket* mem_pkt;
            mem_pkt = mem_intr->decodePacket(pkt, addr, size, true,
                                                    mem_intr->pseudoChannel);

            // Increment read entries of the rank (dram)
            // Increment count to trigger issue of non-deterministic read (nvm)
            mem_intr->setupRank(mem_pkt->rank, true);
            // Default readyTime to Max; will be reset once read is issued
            mem_pkt->readyTime = MaxTick;
            mem_pkt->burstHelper = burst_helper;

            assert(!readQueueFull(1));
            stats.rdQLenPdf[totalReadQueueSize + respQueue.size()]++;

            DPRINTF(MemCtrl, "Adding to read queue\n");


            readQueue[mem_pkt->qosValue()].push_back(mem_pkt);

            // log packet
            logRequest(MemCtrl::READ, pkt->requestorId(),
                       pkt->qosValue(), mem_pkt->addr, 1);

            mem_intr->readQueueSize++;

            // Update stats
            stats.avgRdQLen = totalReadQueueSize + respQueue.size();
        }else{
            DPRINTF(MemCtrl, "Read request for addr %#x serviced by WriteQ\n", addr);
        }

        // Starting address of next memory pkt (aligned to burst boundary)
        addr = (addr | (burst_size - 1)) + 1;
    }

    // If all packets are serviced by write queue, we send the repsonse back
    if (pktsServicedByWrQ == pkt_count) {
        accessAndRespond(pkt, frontendLatency, mem_intr);
        return true;
    }

    // Update how many split packets are serviced by write queue
    if (burst_helper != NULL)
        burst_helper->burstsServiced = pktsServicedByWrQ;

    // not all/any packets serviced by the write queue
    return false;
}

void
MemCtrl::addToWriteQueue(PacketPtr pkt, unsigned int pkt_count,
                                MemInterface* mem_intr)
{
    // only add to the write queue here. whenever the request is
    // eventually done, set the readyTime, and call schedule()
    assert(pkt->isWrite());

    DPRINTF(MemCtrl, "Received a write request for addr %#x of size %d\n",
            pkt->getAddr(), pkt->getSize());

    // if the request size is larger than burst size, the pkt is split into
    // multiple packets
    const Addr base_addr = pkt->getAddr();
    Addr addr = base_addr;
    uint32_t burst_size = mem_intr->bytesPerBurst();

    for (int cnt = 0; cnt < pkt_count; ++cnt) {
        unsigned size = std::min((addr | (burst_size - 1)) + 1,
                        base_addr + pkt->getSize()) - addr;
        stats.writePktSize[ceilLog2(size)]++;
        stats.writeBursts++;
        stats.requestorWriteAccesses[pkt->requestorId()]++;

        // see if we can merge with an existing item in the write
        // queue and keep track of whether we have merged or not
        bool merged = isInWriteQueue.find(burstAlign(addr, mem_intr)) !=
            isInWriteQueue.end();

        // if the item was not merged we need to create a new write
        // and enqueue it
        if (!merged) {
            DPRINTF(MemCtrl, "Write request for addr %#x not merged, adding to WriteQ\n", addr);
            MemPacket* mem_pkt;
            mem_pkt = mem_intr->decodePacket(pkt, addr, size, false,
                                                    mem_intr->pseudoChannel);
            // Default readyTime to Max if nvm interface;
            //will be reset once read is issued
            mem_pkt->readyTime = MaxTick;

            mem_intr->setupRank(mem_pkt->rank, false);

            assert(totalWriteQueueSize < writeBufferSize);
            stats.wrQLenPdf[totalWriteQueueSize]++;

            DPRINTF(MemCtrl, "Adding to write queue\n");

            writeQueue[mem_pkt->qosValue()].push_back(mem_pkt);
            isInWriteQueue.insert(burstAlign(addr, mem_intr));

            // log packet
            logRequest(MemCtrl::WRITE, pkt->requestorId(),
                       pkt->qosValue(), mem_pkt->addr, 1);

            mem_intr->writeQueueSize++;

            assert(totalWriteQueueSize == isInWriteQueue.size());

            // Update stats
            stats.avgWrQLen = totalWriteQueueSize;

        } else {
            DPRINTF(MemCtrl,
                    "Merging write burst with existing queue entry\n");

            // keep track of the fact that this burst effectively
            // disappeared as it was merged with an existing one
            stats.mergedWrBursts++;
        }

        // Starting address of next memory pkt (aligned to burst_size boundary)
        addr = (addr | (burst_size - 1)) + 1;
    }

    // we do not wait for the writes to be send to the actual memory,
    // but instead take responsibility for the consistency here and
    // snoop the write queue for any upcoming reads
    // @todo, if a pkt size is larger than burst size, we might need a
    // different front end latency
    accessAndRespond(pkt, frontendLatency, mem_intr);
    DPRINTF(MemCtrl, "Finished processing write request for addr %#x\n", pkt->getAddr());
}

void
MemCtrl::printQs() const
{
#if TRACING_ON
    DPRINTF(MemCtrl, "===READ QUEUE===\n\n");
    for (const auto& queue : readQueue) {
        for (const auto& packet : queue) {
            DPRINTF(MemCtrl, "Read %#x\n", packet->addr);
        }
    }

    DPRINTF(MemCtrl, "\n===RESP QUEUE===\n\n");
    for (const auto& packet : respQueue) {
        DPRINTF(MemCtrl, "Response %#x\n", packet->addr);
    }

    DPRINTF(MemCtrl, "\n===WRITE QUEUE===\n\n");
    for (const auto& queue : writeQueue) {
        for (const auto& packet : queue) {
            DPRINTF(MemCtrl, "Write %#x\n", packet->addr);
        }
    }
#endif // TRACING_ON
}

bool
MemCtrl::recvTimingReq(PacketPtr pkt)
{
    // This is where we enter from the outside world
    DPRINTF(MemCtrl, "recvTimingReq: request %s addr %#x size %d\n",
            pkt->cmdString(), pkt->getAddr(), pkt->getSize());

    panic_if(pkt->cacheResponding(), "Should not see packets where cache "
             "is responding");

    panic_if(!(pkt->isRead() || pkt->isWrite()),
             "Should only see read and writes at memory controller\n");

    // Calc avg gap between requests
    if (prevArrival != 0) {
        stats.totGap += curTick() - prevArrival;
    }
    prevArrival = curTick();

    panic_if(!(dram->getAddrRange().contains(pkt->getAddr())),
             "Can't handle address range for packet %s\n", pkt->print());

    // Find out how many memory packets a pkt translates to
    // If the burst size is equal or larger than the pkt size, then a pkt
    // translates to only one memory packet. Otherwise, a pkt translates to
    // multiple memory packets
    unsigned size = pkt->getSize();
    uint32_t burst_size = dram->bytesPerBurst();

    unsigned offset = pkt->getAddr() & (burst_size - 1);
    unsigned int pkt_count = divCeil(offset + size, burst_size);

    // run the QoS scheduler and assign a QoS priority value to the packet
    qosSchedule( { &readQueue, &writeQueue }, burst_size, pkt);

    // check local buffers and do not accept if full
    if (pkt->isWrite()) {
        assert(size != 0);
        if (writeQueueFull(pkt_count*N)) {
            DPRINTF(MemCtrl, "Write queue full, not accepting\n");
            // remember that we have to retry this port
            retryWrReq = true;
            stats.numWrRetry++;
            return false;
        } else {
            addToWriteQueue(pkt, pkt_count, dram);
            // If we are not already scheduled to get a request out of the
            // queue, do so now
            if (!nextReqEvent.scheduled()) {
                DPRINTF(MemCtrl, "Request scheduled immediately\n");
                schedule(nextReqEvent, curTick());
            }
            stats.writeReqs++;
            stats.bytesWrittenSys += size;
	    

        //     uint32_t context_id = pkt->req->requestorId();

    	//     std::vector<Addr> addresses = addressMapping(pkt->getAddr());
    	//     uint8_t originalValue = *pkt->getPtr<uint8_t>();
    	//     std::vector<uint8_t> shares = splitData(originalValue);
    	//     for (size_t i = 0; i < N; i++) {
        // 		uint64_t addr = addresses[i];
        //     	uint8_t share = shares[i];
		//         DPRINTF(MemCtrl, "recvTimingReq:  processing addr at %s\n",addr);
        // 		// Create a new packet for the share
        //     	// Using the Packet constructor with the appropriate parameters
        // 		Request::Flags flags;
                        
        // 		//flags.set(0x00002000|0x00001000);
		// 	flags.set(0x00001000);
        // 		RequestPtr req = std::make_shared<Request>(addr, 1, flags, context_id);
        // 		PacketPtr new_pkt = new Packet(req, pkt->cmd);
		// 	new_pkt->bespoke = 1;

        // 		// Allocate data for the new packet and copy the share data
        // 		new_pkt->allocate();
        // 		memcpy(new_pkt->getPtr<uint8_t>(), &share, sizeof(share));

        // 		addToWriteQueue(new_pkt, pkt_count, dram);

        		
        // 		// If we are not already scheduled to get a request out of the
        //         	// queue, do so now
        //     	if (!nextReqEvent.scheduled()) {
        //         	DPRINTF(MemCtrl, "Request scheduled immediately\n");
        //         	schedule(nextReqEvent, curTick());
        //     	}
        //     	stats.writeReqs++;
        //     	stats.bytesWrittenSys += size;
    	//     }
	    // addToWriteQueue(pkt, pkt_count, dram);
	    // stats.writeReqs++;
        //     stats.bytesWrittenSys += size;

            
        }
    } else {
        assert(pkt->isRead());
        assert(size != 0);
        if (readQueueFull(pkt_count*N)) {
            DPRINTF(MemCtrl, "Read queue full, not accepting\n");
            // remember that we have to retry this port
            retryRdReq = true;
            stats.numRdRetry++;
            return false;
        } else {

            bool L2Miss = L2_cache(pkt->getAddr());

            uint64_t  totalDelay = L2Delay;

            if (L2Miss) {

                bool L3Miss = L3_cache(pkt->getAddr());
                if (L3Miss){
                    totalDelay += L3Delay;

                    bool CounterMiss = Counter_cache(pkt->getAddr());

                    // if(CounterMiss){

                    //     std::vector<uint64_t> MT_addresses = addressMapping_MT(pkt->getAddr());
                    //     uint32_t context_id = pkt->req->requestorId();

                    //     for (uint64_t addr : MT_addresses) {
                    //         // Create a new packet for each read
                    //         Request::Flags flags;
                    //         //flags.set(0x00002000|0x00001000);
                    //         flags.set(0x00001000);
                    //         //flags.set(Request::DYNAMIC_DATA);
                    //         RequestPtr req = std::make_shared<Request>(addr, 1, flags, context_id);
                    //         PacketPtr new_pkt = new Packet(req, pkt->cmd);
                    //         // Allocate data for the new packet and copy the share data
                    //         new_pkt->allocate();
                    //         new_pkt->bespoke = 1;
                    //         //memcpy(new_pkt->getPtr<uint8_t>(), &share[0], share.size());

                    //         addToReadQueue(new_pkt, pkt_count, dram);
                    //         stats.readReqs++;
                    //         stats.bytesReadSys += size;


                    //     }


                    // }
                }

            }

            if (nextReqEvent.scheduled()) {
                deschedule(nextReqEvent);
            }
            schedule(nextReqEvent, curTick() + totalDelay);

        if (!addToReadQueue(pkt, pkt_count, dram)) {
                // If we are not already scheduled to get a request out of the
                // queue, do so now
                if (!nextReqEvent.scheduled()) {
                    DPRINTF(MemCtrl, "Request scheduled immediately\n");
                    schedule(nextReqEvent, curTick());
                }
            }
	        stats.readReqs++;
            stats.bytesReadSys += size;
            // uint8_t total = reconstructData(dataArray);
	   }
    }

    return true;
}

void
MemCtrl::processRespondEvent(MemInterface* mem_intr,
                        MemPacketQueue& queue,
                        EventFunctionWrapper& resp_event,
                        bool& retry_rd_req)
{

    DPRINTF(MemCtrl,
            "processRespondEvent(): Some req has reached its readyTime\n");

    MemPacket* mem_pkt = queue.front();

    // media specific checks and functions when read response is complete
    // DRAM only
    mem_intr->respondEvent(mem_pkt->rank);

    if (mem_pkt->burstHelper) {
        // it is a split packet
        mem_pkt->burstHelper->burstsServiced++;
        if (mem_pkt->burstHelper->burstsServiced ==
            mem_pkt->burstHelper->burstCount) {
            // we have now serviced all children packets of a system packet
            // so we can now respond to the requestor
            // @todo we probably want to have a different front end and back
            // end latency for split packets
            accessAndRespond(mem_pkt->pkt, frontendLatency + backendLatency,
                             mem_intr);
            delete mem_pkt->burstHelper;
            mem_pkt->burstHelper = NULL;
        }
    } else {
        // it is not a split packet
        accessAndRespond(mem_pkt->pkt, frontendLatency + backendLatency,
                         mem_intr);
    }

    queue.pop_front();

    if (!queue.empty()) {
        assert(queue.front()->readyTime >= curTick());
        assert(!resp_event.scheduled());
        schedule(resp_event, queue.front()->readyTime);
    } else {
        // if there is nothing left in any queue, signal a drain
        if (drainState() == DrainState::Draining &&
            !totalWriteQueueSize && !totalReadQueueSize &&
            allIntfDrained()) {

            DPRINTF(Drain, "Controller done draining\n");
            signalDrainDone();
        } else {
            // check the refresh state and kick the refresh event loop
            // into action again if banks already closed and just waiting
            // for read to complete
            // DRAM only
            mem_intr->checkRefreshState(mem_pkt->rank);
        }
    }

    delete mem_pkt;

    // We have made a location in the queue available at this point,
    // so if there is a read that was forced to wait, retry now
    if (retry_rd_req) {
        retry_rd_req = false;
        port.sendRetryReq();
    }
}

MemPacketQueue::iterator
MemCtrl::chooseNext(MemPacketQueue& queue, Tick extra_col_delay,
                                                MemInterface* mem_intr)
{
    // This method does the arbitration between requests.

    MemPacketQueue::iterator ret = queue.end();

    if (!queue.empty()) {
        if (queue.size() == 1) {
            // available rank corresponds to state refresh idle
            MemPacket* mem_pkt = *(queue.begin());
            if (mem_pkt->pseudoChannel != mem_intr->pseudoChannel) {
                return ret;
            }
            if (packetReady(mem_pkt, mem_intr)) {
                ret = queue.begin();
                DPRINTF(MemCtrl, "Single request, going to a free rank\n");
            } else {
                DPRINTF(MemCtrl, "Single request, going to a busy rank\n");
            }
        } else if (memSchedPolicy == enums::fcfs) {
            // check if there is a packet going to a free rank
            for (auto i = queue.begin(); i != queue.end(); ++i) {
                MemPacket* mem_pkt = *i;
                if (mem_pkt->pseudoChannel != mem_intr->pseudoChannel) {
                    continue;
                }
                if (packetReady(mem_pkt, mem_intr)) {
                    ret = i;
                    break;
                }
            }
        } else if (memSchedPolicy == enums::frfcfs) {
            Tick col_allowed_at;
            std::tie(ret, col_allowed_at)
                    = chooseNextFRFCFS(queue, extra_col_delay, mem_intr);
        } else {
            panic("No scheduling policy chosen\n");
        }
    }
    return ret;
}

std::pair<MemPacketQueue::iterator, Tick>
MemCtrl::chooseNextFRFCFS(MemPacketQueue& queue, Tick extra_col_delay,
                                MemInterface* mem_intr)
{
    auto selected_pkt_it = queue.end();
    Tick col_allowed_at = MaxTick;

    // time we need to issue a column command to be seamless
    const Tick min_col_at = std::max(mem_intr->nextBurstAt + extra_col_delay,
                                    curTick());

    std::tie(selected_pkt_it, col_allowed_at) =
                 mem_intr->chooseNextFRFCFS(queue, min_col_at);

    if (selected_pkt_it == queue.end()) {
        DPRINTF(MemCtrl, "%s no available packets found\n", __func__);
    }

    return std::make_pair(selected_pkt_it, col_allowed_at);
}

void
MemCtrl::accessAndRespond(PacketPtr pkt, Tick static_latency,
                                                MemInterface* mem_intr)
{
    if(pkt->bespoke) 
    {
    	DPRINTF(MemCtrl, "BESPOKE, NOT Responding to Address %#x.. \n", pkt->getAddr());
    }
    else 
    {
    	DPRINTF(MemCtrl, "Responding to Address %#x.. \n", pkt->getAddr());
    }
	   

    bool needsResponse = pkt->needsResponse();
    // do the actual memory access which also turns the packet into a
    // response
    panic_if(!mem_intr->getAddrRange().contains(pkt->getAddr()),
             "Can't handle address range for packet %s\n", pkt->print());
    mem_intr->access(pkt);

    // turn packet around to go back to requestor if response expected
    if(!pkt->bespoke) {
	    if (needsResponse) {
		// access already turned the packet into a response
		assert(pkt->isResponse());
		// response_time consumes the static latency and is charged also
		// with headerDelay that takes into account the delay provided by
		// the xbar and also the payloadDelay that takes into account the
		// number of data beats.
		Tick response_time = curTick() + static_latency + pkt->headerDelay +
				     pkt->payloadDelay;
		// Here we reset the timing of the packet before sending it out.
		pkt->headerDelay = pkt->payloadDelay = 0;

		// queue the packet in the response queue to be sent out after
		// the static latency has passed
		port.schedTimingResp(pkt, response_time);
	    } else {
		// @todo the packet is going to be deleted, and the MemPacket
		// is still having a pointer to it
		pendingDelete.reset(pkt);
	    }
    }else{
    	//drainState() = DrainState::Draining;
	delete pkt;
    }

    DPRINTF(MemCtrl, "Done\n");

    return;
}

void
MemCtrl::pruneBurstTick()
{
    auto it = burstTicks.begin();
    while (it != burstTicks.end()) {
        auto current_it = it++;
        if (curTick() > *current_it) {
            DPRINTF(MemCtrl, "Removing burstTick for %d\n", *current_it);
            burstTicks.erase(current_it);
        }
    }
}

Tick
MemCtrl::getBurstWindow(Tick cmd_tick)
{
    // get tick aligned to burst window
    Tick burst_offset = cmd_tick % commandWindow;
    return (cmd_tick - burst_offset);
}

Tick
MemCtrl::verifySingleCmd(Tick cmd_tick, Tick max_cmds_per_burst, bool row_cmd)
{
    // start with assumption that there is no contention on command bus
    Tick cmd_at = cmd_tick;

    // get tick aligned to burst window
    Tick burst_tick = getBurstWindow(cmd_tick);

    // verify that we have command bandwidth to issue the command
    // if not, iterate over next window(s) until slot found
    while (burstTicks.count(burst_tick) >= max_cmds_per_burst) {
        DPRINTF(MemCtrl, "Contention found on command bus at %d\n",
                burst_tick);
        burst_tick += commandWindow;
        cmd_at = burst_tick;
    }

    // add command into burst window and return corresponding Tick
    burstTicks.insert(burst_tick);
    return cmd_at;
}

Tick
MemCtrl::verifyMultiCmd(Tick cmd_tick, Tick max_cmds_per_burst,
                         Tick max_multi_cmd_split)
{
    // start with assumption that there is no contention on command bus
    Tick cmd_at = cmd_tick;

    // get tick aligned to burst window
    Tick burst_tick = getBurstWindow(cmd_tick);

    // Command timing requirements are from 2nd command
    // Start with assumption that 2nd command will issue at cmd_at and
    // find prior slot for 1st command to issue
    // Given a maximum latency of max_multi_cmd_split between the commands,
    // find the burst at the maximum latency prior to cmd_at
    Tick burst_offset = 0;
    Tick first_cmd_offset = cmd_tick % commandWindow;
    while (max_multi_cmd_split > (first_cmd_offset + burst_offset)) {
        burst_offset += commandWindow;
    }
    // get the earliest burst aligned address for first command
    // ensure that the time does not go negative
    Tick first_cmd_tick = burst_tick - std::min(burst_offset, burst_tick);

    // Can required commands issue?
    bool first_can_issue = false;
    bool second_can_issue = false;
    // verify that we have command bandwidth to issue the command(s)
    while (!first_can_issue || !second_can_issue) {
        bool same_burst = (burst_tick == first_cmd_tick);
        auto first_cmd_count = burstTicks.count(first_cmd_tick);
        auto second_cmd_count = same_burst ? first_cmd_count + 1 :
                                   burstTicks.count(burst_tick);

        first_can_issue = first_cmd_count < max_cmds_per_burst;
        second_can_issue = second_cmd_count < max_cmds_per_burst;

        if (!second_can_issue) {
            DPRINTF(MemCtrl, "Contention (cmd2) found on command bus at %d\n",
                    burst_tick);
            burst_tick += commandWindow;
            cmd_at = burst_tick;
        }

        // Verify max_multi_cmd_split isn't violated when command 2 is shifted
        // If commands initially were issued in same burst, they are
        // now in consecutive bursts and can still issue B2B
        bool gap_violated = !same_burst &&
             ((burst_tick - first_cmd_tick) > max_multi_cmd_split);

        if (!first_can_issue || (!second_can_issue && gap_violated)) {
            DPRINTF(MemCtrl, "Contention (cmd1) found on command bus at %d\n",
                    first_cmd_tick);
            first_cmd_tick += commandWindow;
        }
    }

    // Add command to burstTicks
    burstTicks.insert(burst_tick);
    burstTicks.insert(first_cmd_tick);

    return cmd_at;
}

bool
MemCtrl::inReadBusState(bool next_state, const MemInterface* mem_intr) const
{
    // check the bus state
    if (next_state) {
        // use busStateNext to get the state that will be used
        // for the next burst
        return (mem_intr->busStateNext == MemCtrl::READ);
    } else {
        return (mem_intr->busState == MemCtrl::READ);
    }
}

bool
MemCtrl::inWriteBusState(bool next_state, const MemInterface* mem_intr) const
{
    // check the bus state
    if (next_state) {
        // use busStateNext to get the state that will be used
        // for the next burst
        return (mem_intr->busStateNext == MemCtrl::WRITE);
    } else {
        return (mem_intr->busState == MemCtrl::WRITE);
    }
}

Tick
MemCtrl::doBurstAccess(MemPacket* mem_pkt, MemInterface* mem_intr)
{
    // first clean up the burstTick set, removing old entries
    // before adding new entries for next burst
    pruneBurstTick();

    // When was command issued?
    Tick cmd_at;

    // Issue the next burst and update bus state to reflect
    // when previous command was issued
    std::vector<MemPacketQueue>& queue = selQueue(mem_pkt->isRead());
    std::tie(cmd_at, mem_intr->nextBurstAt) =
            mem_intr->doBurstAccess(mem_pkt, mem_intr->nextBurstAt, queue);

    DPRINTF(MemCtrl, "Access to %#x, ready at %lld next burst at %lld.\n",
            mem_pkt->addr, mem_pkt->readyTime, mem_intr->nextBurstAt);

    // Update the minimum timing between the requests, this is a
    // conservative estimate of when we have to schedule the next
    // request to not introduce any unecessary bubbles. In most cases
    // we will wake up sooner than we have to.
    mem_intr->nextReqTime = mem_intr->nextBurstAt - mem_intr->commandOffset();

    // Update the common bus stats
    if (mem_pkt->isRead()) {
        ++(mem_intr->readsThisTime);
        // Update latency stats
        stats.requestorReadTotalLat[mem_pkt->requestorId()] +=
            mem_pkt->readyTime - mem_pkt->entryTime;
        stats.requestorReadBytes[mem_pkt->requestorId()] += mem_pkt->size;
    } else {
        ++(mem_intr->writesThisTime);
        stats.requestorWriteBytes[mem_pkt->requestorId()] += mem_pkt->size;
        stats.requestorWriteTotalLat[mem_pkt->requestorId()] +=
            mem_pkt->readyTime - mem_pkt->entryTime;
    }

    return cmd_at;
}

bool
MemCtrl::memBusy(MemInterface* mem_intr) {

    // check ranks for refresh/wakeup - uses busStateNext, so done after
    // turnaround decisions
    // Default to busy status and update based on interface specifics
    // Default state of unused interface is 'true'
    bool mem_busy = true;
    bool all_writes_nvm = mem_intr->numWritesQueued == mem_intr->writeQueueSize;
    bool read_queue_empty = mem_intr->readQueueSize == 0;
    mem_busy = mem_intr->isBusy(read_queue_empty, all_writes_nvm);
    if (mem_busy) {
        // if all ranks are refreshing wait for them to finish
        // and stall this state machine without taking any further
        // action, and do not schedule a new nextReqEvent
        return true;
    } else {
        return false;
    }
}

bool
MemCtrl::nvmWriteBlock(MemInterface* mem_intr) {

    bool all_writes_nvm = mem_intr->numWritesQueued == totalWriteQueueSize;
    return (mem_intr->writeRespQueueFull() && all_writes_nvm);
}

void
MemCtrl::nonDetermReads(MemInterface* mem_intr) {

    for (auto queue = readQueue.rbegin();
            queue != readQueue.rend(); ++queue) {
            // select non-deterministic NVM read to issue
            // assume that we have the command bandwidth to issue this along
            // with additional RD/WR burst with needed bank operations
            if (mem_intr->readsWaitingToIssue()) {
                // select non-deterministic NVM read to issue
                mem_intr->chooseRead(*queue);
            }
    }
}

void
MemCtrl::processNextReqEvent(MemInterface* mem_intr,
                        MemPacketQueue& resp_queue,
                        EventFunctionWrapper& resp_event,
                        EventFunctionWrapper& next_req_event,
                        bool& retry_wr_req) {
    // transition is handled by QoS algorithm if enabled
    if (turnPolicy) {
        // select bus state - only done if QoS algorithms are in use
        busStateNext = selectNextBusState();
    }

    // detect bus state change
    bool switched_cmd_type = (mem_intr->busState != mem_intr->busStateNext);
    // record stats
    recordTurnaroundStats(mem_intr->busState, mem_intr->busStateNext);

    DPRINTF(MemCtrl, "QoS Turnarounds selected state %s %s\n",
            (mem_intr->busState==MemCtrl::READ)?"READ":"WRITE",
            switched_cmd_type?"[turnaround triggered]":"");

    if (switched_cmd_type) {
        if (mem_intr->busState == MemCtrl::READ) {
            DPRINTF(MemCtrl,
            "Switching to writes after %d reads with %d reads "
            "waiting\n", mem_intr->readsThisTime, mem_intr->readQueueSize);
            stats.rdPerTurnAround.sample(mem_intr->readsThisTime);
            mem_intr->readsThisTime = 0;
        } else {
            DPRINTF(MemCtrl,
            "Switching to reads after %d writes with %d writes "
            "waiting\n", mem_intr->writesThisTime, mem_intr->writeQueueSize);
            stats.wrPerTurnAround.sample(mem_intr->writesThisTime);
            mem_intr->writesThisTime = 0;
        }
    }

    if (drainState() == DrainState::Draining && !totalWriteQueueSize &&
        !totalReadQueueSize && respQEmpty() && allIntfDrained()) {

        DPRINTF(Drain, "MemCtrl controller done draining\n");
        signalDrainDone();
    }

    // updates current state
    mem_intr->busState = mem_intr->busStateNext;

    nonDetermReads(mem_intr);

    if (memBusy(mem_intr)) {
        return;
    }

    // when we get here it is either a read or a write
    if (mem_intr->busState == READ) {
        // track if we should switch or not
        bool switch_to_writes = false;
	DPRINTF(MemCtrl,"ReadQueueSize = : %s\n",mem_intr->readQueueSize);
        if (mem_intr->readQueueSize == 0) {
            // In the case there is no read request to go next,
            // trigger writes if we have passed the low threshold (or
            // if we are draining)
            if (!(mem_intr->writeQueueSize == 0) &&
                (drainState() == DrainState::Draining ||
                 mem_intr->writeQueueSize > writeLowThreshold)) {

                DPRINTF(MemCtrl,
                        "Switching to writes due to read queue empty\n");
                switch_to_writes = true;
            } else {
		//DPRINTF(MemCtrl,"Drain state: %s\n",drainState() == DrainState::Running);
               // DPRINTF(MemCtrl,"respQEmpty: %s\n",respQEmpty());
               // DPRINTF(MemCtrl,"allIntfDrained: %s\n",allIntfDrained());

                // check if we are drained
                // not done draining until in PWR_IDLE state
                // ensuring all banks are closed and
                // have exited low power states
                if (drainState() == DrainState::Draining &&
                    respQEmpty() && allIntfDrained()) {

                    DPRINTF(Drain, "MemCtrl controller done draining\n");
                    signalDrainDone();
                }

                // nothing to do, not even any point in scheduling an
                // event for the next request
                return;
            }
        } else {

            bool read_found = false;
            MemPacketQueue::iterator to_read;
            uint8_t prio = numPriorities();

            for (auto queue = readQueue.rbegin();
                 queue != readQueue.rend(); ++queue) {

                prio--;

                DPRINTF(QOS,
                        "Checking READ queue [%d] priority [%d elements]\n",
                        prio, queue->size());

                // Figure out which read request goes next
                // If we are changing command type, incorporate the minimum
                // bus turnaround delay which will be rank to rank delay
                to_read = chooseNext((*queue), switched_cmd_type ?
                                     minWriteToReadDataGap() : 0, mem_intr);

                if (to_read != queue->end()) {
                    // candidate read found
                    read_found = true;
                    break;
                }
            }

            // if no read to an available rank is found then return
            // at this point. There could be writes to the available ranks
            // which are above the required threshold. However, to
            // avoid adding more complexity to the code, return and wait
            // for a refresh event to kick things into action again.
            if (!read_found) {
                DPRINTF(MemCtrl, "No Reads Found - exiting\n");
                return;
            }

            auto mem_pkt = *to_read;

            Tick cmd_at = doBurstAccess(mem_pkt, mem_intr);

            DPRINTF(MemCtrl,
            "Command for %#x, issued at %lld.\n", mem_pkt->addr, cmd_at);

            // sanity check
            assert(pktSizeCheck(mem_pkt, mem_intr));
            assert(mem_pkt->readyTime >= curTick());

            // log the response
            logResponse(MemCtrl::READ, (*to_read)->requestorId(),
                        mem_pkt->qosValue(), mem_pkt->getAddr(), 1,
                        mem_pkt->readyTime - mem_pkt->entryTime);

            mem_intr->readQueueSize--;

            // Insert into response queue. It will be sent back to the
            // requestor at its readyTime
            if (resp_queue.empty()) {
                assert(!resp_event.scheduled());
                schedule(resp_event, mem_pkt->readyTime);
            } else {
                assert(resp_queue.back()->readyTime <= mem_pkt->readyTime);
                assert(resp_event.scheduled());
            }

            resp_queue.push_back(mem_pkt);

            // we have so many writes that we have to transition
            // don't transition if the writeRespQueue is full and
            // there are no other writes that can issue
            // Also ensure that we've issued a minimum defined number
            // of reads before switching, or have emptied the readQ
            if ((mem_intr->writeQueueSize > writeHighThreshold) &&
               (mem_intr->readsThisTime >= minReadsPerSwitch ||
               mem_intr->readQueueSize == 0)
               && !(nvmWriteBlock(mem_intr))) {
                switch_to_writes = true;
            }

            // remove the request from the queue
            // the iterator is no longer valid .
            readQueue[mem_pkt->qosValue()].erase(to_read);
        }

        // switching to writes, either because the read queue is empty
        // and the writes have passed the low threshold (or we are
        // draining), or because the writes hit the hight threshold
        if (switch_to_writes) {
            // transition to writing
            mem_intr->busStateNext = WRITE;
        }
    } else {

        bool write_found = false;
        MemPacketQueue::iterator to_write;
        uint8_t prio = numPriorities();

        for (auto queue = writeQueue.rbegin();
             queue != writeQueue.rend(); ++queue) {

            prio--;

            DPRINTF(QOS,
                    "Checking WRITE queue [%d] priority [%d elements]\n",
                    prio, queue->size());

            // If we are changing command type, incorporate the minimum
            // bus turnaround delay
            to_write = chooseNext((*queue),
                    switched_cmd_type ? minReadToWriteDataGap() : 0, mem_intr);

            if (to_write != queue->end()) {
                write_found = true;
                break;
            }
        }

        // if there are no writes to a rank that is available to service
        // requests (i.e. rank is in refresh idle state) are found then
        // return. There could be reads to the available ranks. However, to
        // avoid adding more complexity to the code, return at this point and
        // wait for a refresh event to kick things into action again.
        if (!write_found) {
            DPRINTF(MemCtrl, "No Writes Found - exiting\n");
            return;
        }

        auto mem_pkt = *to_write;

        // sanity check
        assert(pktSizeCheck(mem_pkt, mem_intr));

        Tick cmd_at = doBurstAccess(mem_pkt, mem_intr);
        DPRINTF(MemCtrl,
        "Command for %#x, issued at %lld.\n", mem_pkt->addr, cmd_at);

        isInWriteQueue.erase(burstAlign(mem_pkt->addr, mem_intr));

        // log the response
        logResponse(MemCtrl::WRITE, mem_pkt->requestorId(),
                    mem_pkt->qosValue(), mem_pkt->getAddr(), 1,
                    mem_pkt->readyTime - mem_pkt->entryTime);

        mem_intr->writeQueueSize--;

        // remove the request from the queue - the iterator is no longer valid
        writeQueue[mem_pkt->qosValue()].erase(to_write);

        delete mem_pkt;

        // If we emptied the write queue, or got sufficiently below the
        // threshold (using the minWritesPerSwitch as the hysteresis) and
        // are not draining, or we have reads waiting and have done enough
        // writes, then switch to reads.
        // If we are interfacing to NVM and have filled the writeRespQueue,
        // with only NVM writes in Q, then switch to reads
        bool below_threshold =
            mem_intr->writeQueueSize + minWritesPerSwitch < writeLowThreshold;

        if (mem_intr->writeQueueSize == 0 ||
            (below_threshold && drainState() != DrainState::Draining) ||
            (mem_intr->readQueueSize && mem_intr->writesThisTime >= minWritesPerSwitch) ||
            (mem_intr->readQueueSize && (nvmWriteBlock(mem_intr)))) {

            // turn the bus back around for reads again
            mem_intr->busStateNext = MemCtrl::READ;

            // note that the we switch back to reads also in the idle
            // case, which eventually will check for any draining and
            // also pause any further scheduling if there is really
            // nothing to do
        }
    }
    // It is possible that a refresh to another rank kicks things back into
    // action before reaching this point.
    if (!next_req_event.scheduled())
        schedule(next_req_event, std::max(mem_intr->nextReqTime, curTick()));

    if (retry_wr_req && mem_intr->writeQueueSize < writeBufferSize) {
        retry_wr_req = false;
        port.sendRetryReq();
    }
}

bool
MemCtrl::packetReady(MemPacket* pkt, MemInterface* mem_intr)
{
    return mem_intr->burstReady(pkt);
}

Tick
MemCtrl::minReadToWriteDataGap()
{
    return dram->minReadToWriteDataGap();
}

Tick
MemCtrl::minWriteToReadDataGap()
{
    return dram->minWriteToReadDataGap();
}

Addr
MemCtrl::burstAlign(Addr addr, MemInterface* mem_intr) const
{
    return (addr & ~(Addr(mem_intr->bytesPerBurst() - 1)));
}

bool
MemCtrl::pktSizeCheck(MemPacket* mem_pkt, MemInterface* mem_intr) const
{
    return (mem_pkt->size <= mem_intr->bytesPerBurst());
}

MemCtrl::CtrlStats::CtrlStats(MemCtrl &_ctrl)
    : statistics::Group(&_ctrl),
    ctrl(_ctrl),

    ADD_STAT(readReqs, statistics::units::Count::get(),
             "Number of read requests accepted"),
    ADD_STAT(writeReqs, statistics::units::Count::get(),
             "Number of write requests accepted"),

    ADD_STAT(readBursts, statistics::units::Count::get(),
             "Number of controller read bursts, including those serviced by "
             "the write queue"),
    ADD_STAT(writeBursts, statistics::units::Count::get(),
             "Number of controller write bursts, including those merged in "
             "the write queue"),
    ADD_STAT(servicedByWrQ, statistics::units::Count::get(),
             "Number of controller read bursts serviced by the write queue"),
    ADD_STAT(mergedWrBursts, statistics::units::Count::get(),
             "Number of controller write bursts merged with an existing one"),

    ADD_STAT(neitherReadNorWriteReqs, statistics::units::Count::get(),
             "Number of requests that are neither read nor write"),

    ADD_STAT(avgRdQLen, statistics::units::Rate<
                statistics::units::Count, statistics::units::Tick>::get(),
             "Average read queue length when enqueuing"),
    ADD_STAT(avgWrQLen, statistics::units::Rate<
                statistics::units::Count, statistics::units::Tick>::get(),
             "Average write queue length when enqueuing"),

    ADD_STAT(numRdRetry, statistics::units::Count::get(),
             "Number of times read queue was full causing retry"),
    ADD_STAT(numWrRetry, statistics::units::Count::get(),
             "Number of times write queue was full causing retry"),

    ADD_STAT(readPktSize, statistics::units::Count::get(),
             "Read request sizes (log2)"),
    ADD_STAT(writePktSize, statistics::units::Count::get(),
             "Write request sizes (log2)"),

    ADD_STAT(rdQLenPdf, statistics::units::Count::get(),
             "What read queue length does an incoming req see"),
    ADD_STAT(wrQLenPdf, statistics::units::Count::get(),
             "What write queue length does an incoming req see"),

    ADD_STAT(rdPerTurnAround, statistics::units::Count::get(),
             "Reads before turning the bus around for writes"),
    ADD_STAT(wrPerTurnAround, statistics::units::Count::get(),
             "Writes before turning the bus around for reads"),

    ADD_STAT(bytesReadWrQ, statistics::units::Byte::get(),
             "Total number of bytes read from write queue"),
    ADD_STAT(bytesReadSys, statistics::units::Byte::get(),
             "Total read bytes from the system interface side"),
    ADD_STAT(bytesWrittenSys, statistics::units::Byte::get(),
             "Total written bytes from the system interface side"),

    ADD_STAT(avgRdBWSys, statistics::units::Rate<
                statistics::units::Byte, statistics::units::Second>::get(),
             "Average system read bandwidth in Byte/s"),
    ADD_STAT(avgWrBWSys, statistics::units::Rate<
                statistics::units::Byte, statistics::units::Second>::get(),
             "Average system write bandwidth in Byte/s"),

    ADD_STAT(totGap, statistics::units::Tick::get(),
             "Total gap between requests"),
    ADD_STAT(avgGap, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "Average gap between requests"),

    ADD_STAT(requestorReadBytes, statistics::units::Byte::get(),
             "Per-requestor bytes read from memory"),
    ADD_STAT(requestorWriteBytes, statistics::units::Byte::get(),
             "Per-requestor bytes write to memory"),
    ADD_STAT(requestorReadRate, statistics::units::Rate<
                statistics::units::Byte, statistics::units::Second>::get(),
             "Per-requestor bytes read from memory rate"),
    ADD_STAT(requestorWriteRate, statistics::units::Rate<
                statistics::units::Byte, statistics::units::Second>::get(),
             "Per-requestor bytes write to memory rate"),
    ADD_STAT(requestorReadAccesses, statistics::units::Count::get(),
             "Per-requestor read serviced memory accesses"),
    ADD_STAT(requestorWriteAccesses, statistics::units::Count::get(),
             "Per-requestor write serviced memory accesses"),
    ADD_STAT(requestorReadTotalLat, statistics::units::Tick::get(),
             "Per-requestor read total memory access latency"),
    ADD_STAT(requestorWriteTotalLat, statistics::units::Tick::get(),
             "Per-requestor write total memory access latency"),
    ADD_STAT(requestorReadAvgLat, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "Per-requestor read average memory access latency"),
    ADD_STAT(requestorWriteAvgLat, statistics::units::Rate<
                statistics::units::Tick, statistics::units::Count>::get(),
             "Per-requestor write average memory access latency")
{
}

void
MemCtrl::CtrlStats::regStats()
{
    using namespace statistics;

    assert(ctrl.system());
    const auto max_requestors = ctrl.system()->maxRequestors();

    avgRdQLen.precision(2);
    avgWrQLen.precision(2);

    readPktSize.init(ceilLog2(ctrl.system()->cacheLineSize()) + 1);
    writePktSize.init(ceilLog2(ctrl.system()->cacheLineSize()) + 1);

    rdQLenPdf.init(ctrl.readBufferSize);
    wrQLenPdf.init(ctrl.writeBufferSize);

    rdPerTurnAround
        .init(ctrl.readBufferSize)
        .flags(nozero);
    wrPerTurnAround
        .init(ctrl.writeBufferSize)
        .flags(nozero);

    avgRdBWSys.precision(8);
    avgWrBWSys.precision(8);
    avgGap.precision(2);

    // per-requestor bytes read and written to memory
    requestorReadBytes
        .init(max_requestors)
        .flags(nozero | nonan);

    requestorWriteBytes
        .init(max_requestors)
        .flags(nozero | nonan);

    // per-requestor bytes read and written to memory rate
    requestorReadRate
        .flags(nozero | nonan)
        .precision(12);

    requestorReadAccesses
        .init(max_requestors)
        .flags(nozero);

    requestorWriteAccesses
        .init(max_requestors)
        .flags(nozero);

    requestorReadTotalLat
        .init(max_requestors)
        .flags(nozero | nonan);

    requestorReadAvgLat
        .flags(nonan)
        .precision(2);

    requestorWriteRate
        .flags(nozero | nonan)
        .precision(12);

    requestorWriteTotalLat
        .init(max_requestors)
        .flags(nozero | nonan);

    requestorWriteAvgLat
        .flags(nonan)
        .precision(2);

    for (int i = 0; i < max_requestors; i++) {
        const std::string requestor = ctrl.system()->getRequestorName(i);
        requestorReadBytes.subname(i, requestor);
        requestorReadRate.subname(i, requestor);
        requestorWriteBytes.subname(i, requestor);
        requestorWriteRate.subname(i, requestor);
        requestorReadAccesses.subname(i, requestor);
        requestorWriteAccesses.subname(i, requestor);
        requestorReadTotalLat.subname(i, requestor);
        requestorReadAvgLat.subname(i, requestor);
        requestorWriteTotalLat.subname(i, requestor);
        requestorWriteAvgLat.subname(i, requestor);
    }

    // Formula stats
    avgRdBWSys = (bytesReadSys) / simSeconds;
    avgWrBWSys = (bytesWrittenSys) / simSeconds;

    avgGap = totGap / (readReqs + writeReqs);

    requestorReadRate = requestorReadBytes / simSeconds;
    requestorWriteRate = requestorWriteBytes / simSeconds;
    requestorReadAvgLat = requestorReadTotalLat / requestorReadAccesses;
    requestorWriteAvgLat = requestorWriteTotalLat / requestorWriteAccesses;
}

void
MemCtrl::recvFunctional(PacketPtr pkt)
{
    bool found = recvFunctionalLogic(pkt, dram);

    panic_if(!found, "Can't handle address range for packet %s\n",
             pkt->print());
}

void
MemCtrl::recvMemBackdoorReq(const MemBackdoorReq &req,
        MemBackdoorPtr &backdoor)
{
    panic_if(!dram->getAddrRange().contains(req.range().start()),
            "Can't handle address range for backdoor %s.",
            req.range().to_string());

    dram->getBackdoor(backdoor);
}

bool
MemCtrl::recvFunctionalLogic(PacketPtr pkt, MemInterface* mem_intr)
{
    if (mem_intr->getAddrRange().contains(pkt->getAddr())) {
        // rely on the abstract memory
        mem_intr->functionalAccess(pkt);
        return true;
    } else {
        return false;
    }
}

Port &
MemCtrl::getPort(const std::string &if_name, PortID idx)
{
    if (if_name != "port") {
        return qos::MemCtrl::getPort(if_name, idx);
    } else {
        return port;
    }
}

bool
MemCtrl::allIntfDrained() const
{
   // DRAM: ensure dram is in power down and refresh IDLE states
   // NVM: No outstanding NVM writes
   // NVM: All other queues verified as needed with calling logic
   return dram->allRanksDrained();
}

DrainState
MemCtrl::drain()
{
    // if there is anything in any of our internal queues, keep track
    // of that as well
    if (totalWriteQueueSize || totalReadQueueSize || !respQEmpty() ||
          !allIntfDrained()) {
        DPRINTF(Drain, "Memory controller not drained, write: %d, read: %d,"
                " resp: %d\n", totalWriteQueueSize, totalReadQueueSize,
                respQueue.size());

        // the only queue that is not drained automatically over time
        // is the write queue, thus kick things into action if needed
        if (totalWriteQueueSize && !nextReqEvent.scheduled()) {
            DPRINTF(Drain,"Scheduling nextReqEvent from drain\n");
            schedule(nextReqEvent, curTick());
        }
	 DPRINTF(Drain,"Draining right now\n");

        dram->drainRanks();

        return DrainState::Draining;
    } else {
        return DrainState::Drained;
    }
}

void
MemCtrl::drainResume()
{
    if (!isTimingMode && system()->isTimingMode()) {
        // if we switched to timing mode, kick things into action,
        // and behave as if we restored from a checkpoint
        startup();
        dram->startup();
    } else if (isTimingMode && !system()->isTimingMode()) {
        // if we switch from timing mode, stop the refresh events to
        // not cause issues with KVM
        dram->suspend();
    }

    // update the mode
    isTimingMode = system()->isTimingMode();
}

AddrRangeList
MemCtrl::getAddrRanges()
{
    AddrRangeList range;
    range.push_back(dram->getAddrRange());
    return range;
}

MemCtrl::MemoryPort::
MemoryPort(const std::string& name, MemCtrl& _ctrl)
    : QueuedResponsePort(name, queue), queue(_ctrl, *this, true),
      ctrl(_ctrl)
{ }

AddrRangeList
MemCtrl::MemoryPort::getAddrRanges() const
{
    return ctrl.getAddrRanges();
}

void
MemCtrl::MemoryPort::recvFunctional(PacketPtr pkt)
{
    pkt->pushLabel(ctrl.name());

    if (!queue.trySatisfyFunctional(pkt)) {
        // Default implementation of SimpleTimingPort::recvFunctional()
        // calls recvAtomic() and throws away the latency; we can save a
        // little here by just not calculating the latency.
        ctrl.recvFunctional(pkt);
    }

    pkt->popLabel();
}

void
MemCtrl::MemoryPort::recvMemBackdoorReq(const MemBackdoorReq &req,
        MemBackdoorPtr &backdoor)
{
    ctrl.recvMemBackdoorReq(req, backdoor);
}

Tick
MemCtrl::MemoryPort::recvAtomic(PacketPtr pkt)
{
    return ctrl.recvAtomic(pkt);
}

Tick
MemCtrl::MemoryPort::recvAtomicBackdoor(
        PacketPtr pkt, MemBackdoorPtr &backdoor)
{
    return ctrl.recvAtomicBackdoor(pkt, backdoor);
}

bool
MemCtrl::MemoryPort::recvTimingReq(PacketPtr pkt)
{
    // pass it to the memory controller
    return ctrl.recvTimingReq(pkt);
}

void
MemCtrl::MemoryPort::disableSanityCheck()
{
    queue.disableSanityCheck();
}

} // namespace memory
} // namespace gem5
