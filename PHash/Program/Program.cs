namespace Program
{
    using System;
    using System.Runtime.InteropServices;
    using System.Text;

    using PHashLong = System.UInt64;

    public class Program
    {
        [DllImport("PHash.dll", CallingConvention = CallingConvention.Cdecl)]
        private static extern int ph_dct_imagehash(
            [MarshalAs(UnmanagedType.LPStr)]
            string file,
            [MarshalAs(UnmanagedType.U8)]
            ref PHashLong hash,
            [MarshalAs(UnmanagedType.LPStr)]
            StringBuilder error,
            [MarshalAs(UnmanagedType.LPStr)]
            StringBuilder buffer);

        public static void Main(string[] args)
        {
            ulong hash1 = 0, hash2 = 0;
            System.Text.StringBuilder error1 = new System.Text.StringBuilder(10000), error2 = new System.Text.StringBuilder(10000);
            System.Text.StringBuilder buffer1 = new System.Text.StringBuilder(10000), buffer2 = new System.Text.StringBuilder(10000);
            
            int result1 = ph_dct_imagehash(@"25022o.png", ref hash1, error1, buffer1);
            int result2 = ph_dct_imagehash(@"25022b.png", ref hash2, error2, buffer2);

            Console.WriteLine("Done");
        }
    }
}
