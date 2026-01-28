using System;
using System.Runtime.InteropServices;
using System.Threading;

namespace Pengu.Loader.Utils
{
    /// C# version of Pengu hook.h
    unsafe class Hook<T> : IDisposable where T : Delegate
    {
        private void* func_;
        private void* code_;
        private Lock lock_;

        [StructLayout(LayoutKind.Sequential, Size = Shellcode.SizeOf, Pack = 1)]
        unsafe struct Shellcode
        {
            public byte movabs = 0x48;
            public byte mov_eax = 0xB8;
            public IntPtr addr;
            //public byte push_eax = 0x50;
            //public byte ret = 0xC3;
            public byte jmp_ = 0xFF;
            public byte _rax = 0xE0;

            public Shellcode() { }
            public const int SizeOf = 12;
        }

        public Hook()
        {
            func_ = null;
            lock_ = new Lock();
        }

        public void Install(string lib, string name, T hook)
        {
            if (func_ != null)
                return;

            var mod = NativeLibrary.Load(lib);
            var proc = NativeLibrary.GetExport(mod, name);

            Install(proc, hook);
        }

        public void Install(IntPtr orig, T hook)
        {
            if (orig == IntPtr.Zero || func_ != null)
                return;

            func_ = (void*)orig;
            code_ = NativeMemory.AllocZeroed(Shellcode.SizeOf);
            NativeMemory.Copy((void*)orig, code_, Shellcode.SizeOf);

            var code = new Shellcode();
            code.addr = Marshal.GetFunctionPointerForDelegate<T>(hook);

            Native.ProtectedMemcpy((void*)orig, &code, Shellcode.SizeOf);
        }

        public void Dispose()
        {
            if (func_ != null)
            {
                lock (lock_)
                {
                    Native.ProtectedMemcpy(func_, code_, Shellcode.SizeOf);
                    NativeMemory.Free(code_);
                }
            }
        }

        public CallGuard GetCall()
        {
            return new CallGuard(func_, code_, lock_);
        }

        public class CallGuard : IDisposable
        {
            private void* func_;
            private void* backup_;
            private Lock lock_;

            public IntPtr FuncPtr => (IntPtr)func_;
            public T Func => Marshal.GetDelegateForFunctionPointer<T>((nint)func_);

            public CallGuard(void *func, void* code, Lock @lock)
            {
                lock_ = @lock;
                @lock.Enter();

                func_ = func;
                backup_ = NativeMemory.AllocZeroed(Shellcode.SizeOf);

                NativeMemory.Copy(func, backup_, Shellcode.SizeOf);
                Native.ProtectedMemcpy(func, code, Shellcode.SizeOf);
            }

            public void Dispose()
            {
                Native.ProtectedMemcpy(func_, backup_, Shellcode.SizeOf);
                NativeMemory.Free(backup_);
                
                lock_.Exit();
            }
        }
    }

    partial class Native
    {
        [LibraryImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static unsafe partial bool VirtualProtect(void* addr, int size, int newProt, out int oldProt);

        public static unsafe void ProtectedMemcpy(void* dst, void* src, int size)
        {
            int op;
            VirtualProtect(dst, size, /*PAGE_EXECUTE_READWRITE*/0x40, out op);
            NativeMemory.Copy(src, dst, (nuint)size);
            VirtualProtect(dst, size, op, out op);
        }
    }
}