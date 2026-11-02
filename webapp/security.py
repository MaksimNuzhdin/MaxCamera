"""Linux syscall restrictions applied after inheriting fixed network sockets."""
import ctypes
import errno
import socket

class ArgCmp(ctypes.Structure):
    _fields_ = [('arg',ctypes.c_uint),('op',ctypes.c_uint),('a',ctypes.c_uint64),('b',ctypes.c_uint64)]

def lock_network():
    lib=ctypes.CDLL('libseccomp.so.2',use_errno=True)
    lib.seccomp_init.argtypes=[ctypes.c_uint32];lib.seccomp_init.restype=ctypes.c_void_p
    lib.seccomp_syscall_resolve_name.argtypes=[ctypes.c_char_p];lib.seccomp_syscall_resolve_name.restype=ctypes.c_int
    lib.seccomp_rule_add_array.argtypes=[ctypes.c_void_p,ctypes.c_uint32,ctypes.c_int,ctypes.c_uint,ctypes.POINTER(ArgCmp)]
    lib.seccomp_load.argtypes=[ctypes.c_void_p];lib.seccomp_release.argtypes=[ctypes.c_void_p]
    lib.seccomp_attr_set.argtypes=[ctypes.c_void_p,ctypes.c_uint,ctypes.c_uint32]
    ctx=lib.seccomp_init(0x7fff0000)
    if not ctx:raise RuntimeError('seccomp initialization failed')
    try:
        # TSYNC applies the policy to every existing thread and all descendants.
        if lib.seccomp_attr_set(ctx,4,1):raise RuntimeError('seccomp TSYNC unavailable')
        def deny(name,cmp=None):
            syscall=lib.seccomp_syscall_resolve_name(name.encode())
            if syscall<0:return
            arr=(ArgCmp*len(cmp))(*cmp) if cmp else None
            if lib.seccomp_rule_add_array(ctx,0x50000|errno.EPERM,syscall,len(cmp or []),arr):raise RuntimeError('seccomp rule failed')
        for name in ('connect','bind','sendmsg','sendmmsg','ptrace','process_vm_writev','bpf','mount','setns','unshare'):
            deny(name)
        for family in (socket.AF_INET,socket.AF_INET6,socket.AF_PACKET,socket.AF_NETLINK):
            deny('socket',[ArgCmp(0,4,family,0)])
        # send()/sendto(NULL) can use only the preconnected camera socket.
        deny('sendto',[ArgCmp(4,1,0,0)])
        if lib.seccomp_load(ctx):raise RuntimeError('seccomp loading failed')
    finally:lib.seccomp_release(ctx)
