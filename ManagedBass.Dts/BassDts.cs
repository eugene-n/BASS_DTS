using System;
using System.Runtime.InteropServices;

namespace ManagedBass.Dts
{
    public static class BassDts
    {
        const string DllName = "bass_dts";

        [DllImport(DllName)]
        static extern int BASS_DTS_StreamCreateFile(bool Memory, string File, long Offset, long Length, BassFlags Flags);

        public static int CreateStream(string File, long Offset = 0, long Length = 0, BassFlags Flags = BassFlags.Default)
        {
            return BASS_DTS_StreamCreateFile(false, File, Offset, Length, Flags);
        }
    }
}
