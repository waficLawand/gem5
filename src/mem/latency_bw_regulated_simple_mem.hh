/*
 * Copyright (c) 2012-2013 ARM Limited
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
 * Copyright (c) 2001-2005 The Regents of The University of Michigan
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

/**
 * @file
 * LatencyBwRegulatedSimpleMem declaration
 */

#ifndef __MEM_SIMPLE_MEMORY_HH__
#define __MEM_SIMPLE_MEMORY_HH__

#include <list>
#include <queue>
#include "mem/abstract_mem.hh"
#include "mem/port.hh"
#include "params/LatencyBwRegulatedSimpleMem.hh"


namespace gem5
{

namespace memory
{

/**
 * The simple memory is a basic single-ported memory controller with
 * a configurable throughput and latency.
 *
 * @sa  \ref gem5MemorySystem "gem5 Memory System"
 */
class LatencyBwRegulatedSimpleMem : public AbstractMemory
{

  private:

    /**
     * A deferred packet stores a packet along with its scheduled
     * transmission time
     */
    class DeferredPacket
    {

      public:

        const Tick tick;
        const PacketPtr pkt;

        DeferredPacket(PacketPtr _pkt, Tick _tick) : tick(_tick), pkt(_pkt)
        { }
    };

    class MemoryPort : public ResponsePort
    {
      private:
        LatencyBwRegulatedSimpleMem& mem;

      public:
        MemoryPort(const std::string& _name, LatencyBwRegulatedSimpleMem& _memory);

      protected:
        Tick recvAtomic(PacketPtr pkt) override;
        Tick recvAtomicBackdoor(
                PacketPtr pkt, MemBackdoorPtr &_backdoor) override;
        void recvFunctional(PacketPtr pkt) override;
        void recvMemBackdoorReq(const MemBackdoorReq &req,
                MemBackdoorPtr &backdoor) override;
        bool recvTimingReq(PacketPtr pkt) override;
        void recvRespRetry() override;
        AddrRangeList getAddrRanges() const override;
    };

    MemoryPort port;

    /**
     * Latency from that a request is accepted until the response is
     * ready to be sent.
     */
    const Tick latency;

    /**
     * Fudge factor added to the latency.
     */
    const Tick latency_var;

    /**
     * Internal (unbounded) storage to mimic the delay caused by the
     * actual memory access. Note that this is where the packet spends
     * the memory latency.
     */
    std::list<DeferredPacket> packetQueue;

    /**
     * Bandwidth in ticks per byte. The regulation affects the
     * acceptance rate of requests and the queueing takes place after
     * the regulation.
     */
    const double bandwidth;

    /**
     * Track the state of the memory as either idle or busy, no need
     * for an enum with only two states.
     */
    bool isBusy;

    /**
     * Remember if we have to retry an outstanding request that
     * arrived while we were busy.
     */
    bool retryReq;

    /**
     * Remember if we failed to send a response and are awaiting a
     * retry. This is only used as a check.
     */
    bool retryResp;

    /**
     * Release the memory after being busy and send a retry if a
     * request was rejected in the meanwhile.
     */
    void release();

    EventFunctionWrapper releaseEvent;

    /**
     * Dequeue a packet from our internal packet queue and move it to
     * the port where it will be sent as soon as possible.
     */
    void dequeue();

    EventFunctionWrapper dequeueEvent;

    /**
     * Detemine the latency.
     *
     * @return the latency seen by the current packet
     */
    Tick getLatency() const;

    /**
     * Upstream caches need this packet until true is returned, so
     * hold it for deletion until a subsequent call
     */
    std::unique_ptr<Packet> pendingDelete;

  public:
    
    void startup() override;

    LatencyBwRegulatedSimpleMem(const LatencyBwRegulatedSimpleMemParams &p);

    DrainState drain() override;

    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;
    void init() override;

    // #################################### Bandwidth regulation elements ########################################################
    struct packet_queue_element
    {
      PacketPtr pkt;
      Tick pkt_tick;
      int requestor_id;
      int relative_deadline;
    };
    
    int64_t requestors; // Total number of requestors in the system
    int64_t demand_burstiness; // Demand bucket size
    int64_t prefetch_burstiness; // Prefetch bucket size

    int64_t refill_tokens; // Number of tokens that will be filled every refill period
    Tick refill_period; // How often the demand bucket will be filled (per Tick)
    Tick last_empty; // Last tick when the bucket got empty

    int64_t * demand_tokens; // Tokens in the demand bucket
    int64_t * prefetch_tokens; // Tokens in the prefetch bucket

    int64_t * latency_counters; // Array of counters that keep track of the suffered latency by each requestor

    int64_t initial_slack; // Initial slack that all the counters are initialized with

    int64_t ticks_per_cycle; // Number of ticks per cycle

    bool hpa_mode; // High performance arbiter mode, when this is true we are in HPA mode when this is false we are in RTA mode

    std::queue<packet_queue_element> * demand_queues_pre_bucket; // Demand Queue for every requestor before the bucket
    std::queue<packet_queue_element> * demand_queues_post_bucket; // Demand Queue for every requestor after the bucket

    std::queue<packet_queue_element> * prefetch_queues_pre_bucket; // Prefetch Queue for every requestor before the bucket
    std::queue<packet_queue_element> * prefetch_queues_post_bucket; // Prefetch Queue for every requestor after the bucket

    std::queue<packet_queue_element> * requestor_queues; // Operate in an FCFS manner based on the arrival tick
    
    std::queue<packet_queue_element> * global_fcfs_queue; // Global FCFS queue
    std::queue<packet_queue_element> * global_rr_queue; // Global Round Robin queue

    // Round Robin Queue that keeps track of which requestor can push packets to the memory
    std::queue<int> round_robin_sched_queue;

    // List for finished requests, this list is used to add to the counters the deadline of memory requests when they finish
    std::list<packet_queue_element> finished_list;


    int64_t demand_queue_size;
    int64_t prefetch_queue_size;
    int64_t requestor_queue_size;

    void consume_demand_token(int requestor);
    EventFunctionWrapper consume_demand_token_event();

    void bucket_refill();
    EventFunctionWrapper monitoring_event;

    void handle_pkts_in_queues();
    EventFunctionWrapper handle_pkts;

    bool fulfillReq(PacketPtr pkt);

    // Functions that fill the global queues based of FCFS and RR
    void fill_global_fcfs_queue();
    EventFunctionWrapper fill_global_fcfs;

    void fill_global_rr_queue();
    EventFunctionWrapper rr_arbitration;

    // Event that will switch from HPA to RTA or vice versa
    //EventFunctionWrapper arbiter_mode_event;
    void switch_arbiter_mode();

    // Event to update resource counters, add on finish and subtract when the request is still being processed
    //EventFunctionWrapper update_resource_counters;
    void add_sub_counters();

    EventFunctionWrapper handle_fcfs;
    void handle_global_fcfs();

    bool start_with_full_buckets; // Start with full buckets

    bool enable_bw_regulation;
    
    // TODO Requires a cleaner solution this is just for testing
    bool is_fcfs;

    //

    // #################################### Bandwidth regulation elements ########################################################

  protected:
    Tick recvAtomic(PacketPtr pkt);
    Tick recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor);
    void recvFunctional(PacketPtr pkt);
    void recvMemBackdoorReq(const MemBackdoorReq &req,
            MemBackdoorPtr &backdoor);
    bool recvTimingReq(PacketPtr pkt);
    void recvRespRetry();
};

} // namespace memory
} // namespace gem5

#endif //__MEM_SIMPLE_MEMORY_HH__