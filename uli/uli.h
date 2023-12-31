#ifndef ULI_H
#define ULI_H

#include <stdint.h>
#include <stdio.h>

constexpr uint64_t ULI_FX_ADDR      = 0x82E; // CSR_VALUE14
constexpr uint64_t ULI_DATA_ADDR      = 0x82F; // CSR_VALUE15
constexpr uint64_t ULI_SEND_NO_RESP   = 0x830; // CSR_VALUE16
constexpr uint64_t ULI_SEND_REQ       = 0x832; // CSR_VALUE18
constexpr uint64_t ULI_SEND_RESP      = 0x833; // CSR_VALUE19
constexpr uint64_t ULI_RECV_RESP      = 0x834; // CSR_VALUE20
constexpr uint64_t ULI_RECV_VAL       = 0x835; // CSR_VALUE21
constexpr uint64_t ULI_RECV_SENDER_ID = 0x836; // CSR_VALUE22

// Based on
// The RISC-V Instruction Set Manual
// Volume II: Privileged Architecture
// Document Version 20190608-Priv-MSU-Ratified

constexpr uint64_t CSR_MHARTID  = 0xF14;
constexpr uint64_t CSR_MSTATUS  = 0x300;
constexpr uint64_t CSR_MIDELEG  = 0x303;
constexpr uint64_t CSR_SIDELEG  = 0x103;
constexpr uint64_t CSR_USTATUS  = 0x000;
constexpr uint64_t CSR_UIE      = 0x004;
constexpr uint64_t CSR_UTVEC    = 0x005;
constexpr uint64_t CSR_USCRATCH = 0x040;
constexpr uint64_t CSR_UEPC     = 0x041;
constexpr uint64_t CSR_UCAUSE   = 0x042;
constexpr uint64_t CSR_UTVAL    = 0x043;
constexpr uint64_t CSR_UIP      = 0x044;

constexpr uint64_t CSR_MIDELEG_BIT_USI = 0;
constexpr uint64_t CSR_SIDELEG_BIT_USI = 0;
constexpr uint64_t CSR_MSTATUS_BIT_UIE = 0;

// Get core ID

inline uint64_t core_id()
{
  volatile uint64_t id;
  __asm__ volatile ("csrr %0, %1;" : "=r"(id) : "i"(CSR_MHARTID):);
  return id;
}

inline uint64_t sender_id()
{
  volatile uint64_t id;
  __asm__ volatile ("csrr %0, %1;" : "=r"(id) : "i"(ULI_RECV_SENDER_ID):);
  return id;
}

inline uint64_t read_addr()
{
  volatile uint64_t addr;
  __asm__ volatile ("csrr %0, %1;" : "=r"(addr) : "i"(ULI_DATA_ADDR):);
  return addr;
}

inline uint64_t read_fx_addr()
{
  volatile uint64_t addr;
  __asm__ volatile ("csrr %0, %1;" : "=r"(addr) : "i"(ULI_FX_ADDR):);
  return addr;
}

// Initialize ULI for the current hart

inline void uli_init()
{
    // Enable global UIE bit in the mstatus
    uint64_t status, deleg;
    __asm__ volatile ("csrr %0, %1;" : "=r"(status) : "i"(CSR_MSTATUS) :);
    status |= (1 << CSR_MSTATUS_BIT_UIE);
    __asm__ volatile ("csrw %0, %1;" :: "i"(CSR_MSTATUS), "r"(status) :);
    // Set USI bit in sideleg and mideleg
    // to delegate ULI handling to the user mode.
    __asm__ volatile ("csrr %0, %1;" : "=r"(deleg) : "i"(CSR_SIDELEG) :);
    deleg |= (1 << CSR_MIDELEG_BIT_USI);
    __asm__ volatile ("csrw %0, %1;" :: "i"(CSR_SIDELEG), "r"(deleg) :);
    __asm__ volatile ("csrr %0, %1;" : "=r"(deleg) : "i"(CSR_MIDELEG) :);
    deleg |= (1 << CSR_SIDELEG_BIT_USI);
    __asm__ volatile ("csrw %0, %1;" :: "i"(CSR_MIDELEG), "r"(deleg) :);
}


// Set ULI handler

inline void uli_set_handler(void* addr)
{
  __asm__ volatile ("csrw %0, %1;"
                    :
                    : "i"(CSR_UTVEC), "r" ((uint64_t)addr)
                    :);
}

// Enable/disable ULI

inline void uli_enable()
{
  __asm__ volatile ("csrw %0, %1;"
                    :
                    : "i"(CSR_UIE), "i" (1)
                    :);
}

inline void uli_disable()
{
  __asm__ volatile ("csrw %0, %1;"
                    :
                    : "i"(CSR_UIE), "i" (0)
                    :);
}

// ULI sender: send a ULI to target core,
// blocked until a resp (or NACK) is received
inline uint64_t uli_send_req(uint64_t target)
{
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_DATA_ADDR),  "r" (0) :); //send no as the payload
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_FX_ADDR),  "r" (0) :); //send no as the payload
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_RECV_RESP),  "i" (0) :); // reset ULI_RECV_RESP to zero
  // send
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_SEND_REQ),  "r" (target) :);
  volatile uint64_t resp = 0, val = 0;
  // blocked, waiting for resp
  while (resp == 0) {
    __asm__ volatile ("csrr %0, %1;" : "=r"(resp) : "i"(ULI_RECV_RESP) :);
  }
  __asm__ volatile ("csrr %0, %1;" : "=r"(val) : "i"(ULI_RECV_VAL) :);
  return val;
}

// ULI sender: send a ULI to target core,
// blocked until a resp (or NACK) is received
// 
// passes address for a data item with the callback
inline uint64_t uli_send_req_addr(uint64_t target, void* addr)
{
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_DATA_ADDR),  "r" ((uint64_t)addr) :);//write pointer to address
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_FX_ADDR),  "r" (0) :); //send no as the payload
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_RECV_RESP),  "i" (0) :);// reset ULI_RECV_RESP to zero
  // send
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_SEND_REQ),  "r" (target) :);
  volatile uint64_t resp = 0, val = 0;
  // blocked, waiting for resp
  while (resp == 0) {
    __asm__ volatile ("csrr %0, %1;" : "=r"(resp) : "i"(ULI_RECV_RESP) :);
  }
  __asm__ volatile ("csrr %0, %1;" : "=r"(val) : "i"(ULI_RECV_VAL) :);
  return val;
}

// ULI sender: send a ULI to target core,
// not blocked until a resp (or NACK) is received
inline uint64_t uli_send_req_no_resp(uint64_t target)
{
  // reset ULI_RECV_RESP to zero
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_RECV_RESP),  "i" (0) :);
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_SEND_REQ),  "r" (target) :);
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_DATA_ADDR),  "r" (0) :); //send no as the payload
  return 0;
}

// ULI sender: send a ULI to target core,
// not blocked until a resp (or NACK) is received
inline uint64_t uli_send_req_no_resp_addr(uint64_t target, void* addr)
{
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_DATA_ADDR),  "r" ((uint64_t)addr) :);//write pointer to address
  // reset ULI_RECV_RESP to zero
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_RECV_RESP),  "i" (0) :);
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_SEND_REQ),  "r" (target) :);
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_DATA_ADDR),  "r" (0) :); //send no as the payload
  return 0;
}

// ULI receiver: send a ULI response to the interrupter
inline void uli_send_resp(uint64_t val = 0)
{
  // send
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_SEND_RESP),  "r" (val) :);
}

/* uli_send_no_resp

  Purpose: for workshedding - when work is shed to the requesting core,
           after completing the interrupt, call this in the handler func.
           This is for when the requestor is not waiting for an ack.

*/
inline void uli_send_no_resp(uint64_t val = 0)
{
  // send
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_SEND_NO_RESP),  "r" (val) :);
}

/* uli_local_send_resp

  Purpose: for workshedding - when work is shed to the requesting core,
           after completing the interrupt, call this in the handler func
           to "return" the value to itself (if the caller is waiting for
           a return value).

*/
inline void uli_local_send_resp(uint64_t val = 0)
{
  // send
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_SEND_NO_RESP),  "r" (val) :);
  // __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_RECV_RESP),  "r" (1) :);
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_RECV_VAL),  "r" (val) :);
}


// ULI sender: send a ULI to target core,
// blocked until a resp (or NACK) is received
inline uint64_t uli_send_req_fx_addr(uint64_t target, void* addr)
{
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_FX_ADDR),  "r" ((uint64_t)addr) :);//write pointer to address
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_DATA_ADDR),  "r" (0) :); //send no as the payload
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_RECV_RESP),  "i" (0) :);// reset ULI_RECV_RESP to zero
  // send
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_SEND_REQ),  "r" (target) :);
  volatile uint64_t resp = 0, val = 0;
  // blocked, waiting for resp
  while (resp == 0) {
    __asm__ volatile ("csrr %0, %1;" : "=r"(resp) : "i"(ULI_RECV_RESP) :);
  }
  __asm__ volatile ("csrr %0, %1;" : "=r"(val) : "i"(ULI_RECV_VAL) :);
  return val;
}

inline uint64_t uli_send_req_fx_addr_data(uint64_t target, void* addr, void* dataAddr)
{
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_FX_ADDR),  "r" ((uint64_t)addr) :);//write pointer to fx address
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_DATA_ADDR),  "r" ((uint64_t)dataAddr) :); //write pointer to data address
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_RECV_RESP),  "i" (0) :);// reset ULI_RECV_RESP to zero
  // send
  __asm__ volatile ("csrw %0, %1;" :: "i"(ULI_SEND_REQ),  "r" (target) :);
  volatile uint64_t resp = 0, val = 0;
  // blocked, waiting for resp
  while (resp == 0) {
    __asm__ volatile ("csrr %0, %1;" : "=r"(resp) : "i"(ULI_RECV_RESP) :);
  }
  __asm__ volatile ("csrr %0, %1;" : "=r"(val) : "i"(ULI_RECV_VAL) :);
  return val;
}


#endif
