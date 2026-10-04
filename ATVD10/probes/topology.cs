using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;

public static class ArchitectureTopology {
    [DllImport("kernel32.dll", SetLastError=true)]
    static extern bool GetLogicalProcessorInformationEx(int relationship, IntPtr buffer, ref uint length);
    [DllImport("kernel32.dll")] static extern ushort GetActiveProcessorGroupCount();
    [DllImport("kernel32.dll")] static extern uint GetActiveProcessorCount(ushort group);
    static ulong Mask(byte[] b, int p) { return IntPtr.Size == 8 ? BitConverter.ToUInt64(b,p) : BitConverter.ToUInt32(b,p); }
    static int[] Bits(ulong mask) {
        var a = new List<int>(); for(int i=0;i<IntPtr.Size*8;i++) if((mask & (1UL<<i))!=0) a.Add(i); return a.ToArray();
    }
    public static object Read() {
        uint len=0; GetLogicalProcessorInformationEx(0xffff,IntPtr.Zero,ref len);
        if(len==0) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
        IntPtr ptr=Marshal.AllocHGlobal((int)len);
        var rows = new List<object>();
        try {
            if(!GetLogicalProcessorInformationEx(0xffff,ptr,ref len)) throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
            byte[] b=new byte[len]; Marshal.Copy(ptr,b,0,(int)len);
            for(int p=0;p<b.Length;) {
                int rel=BitConverter.ToInt32(b,p), size=BitConverter.ToInt32(b,p+4);
                if(size<8 || p+size>b.Length) throw new Exception("Invalid topology record");
                var row=new Dictionary<string,object>(); row["relationship"]=rel;
                if(rel==0 || rel==3) {
                    row["kind"]=rel==0?"core":"socket";
                    row["smt_flag"]=(b[p+8]&1)!=0; row["efficiency_class"]=b[p+9];
                    int n=BitConverter.ToUInt16(b,p+30); var masks=new List<object>();
                    for(int j=0;j<n;j++) { int m=p+32+j*(IntPtr.Size+8); masks.Add(new { group=BitConverter.ToUInt16(b,m+IntPtr.Size), logical_processors=Bits(Mask(b,m)) }); }
                    row["affinity"]=masks;
                } else if(rel==2) {
                    row["kind"]="cache"; row["level"]=b[p+8]; row["line_bytes"]=BitConverter.ToUInt16(b,p+10);
                    row["size_bytes"]=BitConverter.ToUInt32(b,p+12); row["cache_type"]=BitConverter.ToInt32(b,p+16);
                    row["group"]=BitConverter.ToUInt16(b,p+40+IntPtr.Size); row["logical_processors"]=Bits(Mask(b,p+40));
                } else if(rel==1) {
                    row["kind"]="numa_node"; row["node"]=BitConverter.ToUInt32(b,p+8);
                    row["group"]=BitConverter.ToUInt16(b,p+32+IntPtr.Size); row["logical_processors"]=Bits(Mask(b,p+32));
                } else { row["kind"]="other"; }
                rows.Add(row); p+=size;
            }
        } finally { Marshal.FreeHGlobal(ptr); }
        var groups=new List<object>();
        for(ushort g=0;g<GetActiveProcessorGroupCount();g++) groups.Add(new { group=g, active_logical_processors=GetActiveProcessorCount(g) });
        return new { source="GetLogicalProcessorInformationEx(RelationAll)", pointer_size=IntPtr.Size, groups=groups, records=rows };
    }
}
