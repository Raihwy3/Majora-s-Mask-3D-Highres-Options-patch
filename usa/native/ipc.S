.syntax unified
.arm
.text
.global mm_ipc_buffer
.type mm_ipc_buffer,%function
mm_ipc_buffer:
    mrc p15,0,r0,c13,c0,3
    add r0,r0,#0x80
    bx lr
.global mm_ipc_send
.type mm_ipc_send,%function
mm_ipc_send:
    svc 0x32
    bx lr
.global mm_ipc_close
.type mm_ipc_close,%function
mm_ipc_close:
    svc 0x23
    bx lr
