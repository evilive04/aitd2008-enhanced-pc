using System;
using System.IO;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.Cryptography.X509Certificates;

namespace EnhancedSetup {
    internal sealed class PythonEnvironment {
        public bool Interpreter, Packages;
        public static PythonEnvironment Parse(int exit,string transcript) {
            string line=transcript.Split(new[]{'\r','\n'},StringSplitOptions.RemoveEmptyEntries).LastOrDefault(x=>x.StartsWith("AITD_SETUP_ENV\t",StringComparison.Ordinal));
            string[] fields=line==null?new string[0]:line.Split('\t');int major,minor,bits;
            bool interpreter=exit==0&&fields.Length==6&&Int32.TryParse(fields[1],out major)&&major==3&&Int32.TryParse(fields[2],out minor)&&minor>=12&&Int32.TryParse(fields[3],out bits)&&bits==64;
            return new PythonEnvironment { Interpreter=interpreter,Packages=interpreter&&fields[4]=="13.0.0"&&fields[5]=="2.5.4" };
        }
        public const string Probe="import sys,struct,importlib.metadata as m\n"+
            "def version(name):\n try: return m.version(name)\n except m.PackageNotFoundError: return 'missing'\n"+
            "print('AITD_SETUP_ENV\\t%d\\t%d\\t%d\\t%s\\t%s' % (sys.version_info[0],sys.version_info[1],struct.calcsize('P')*8,version('Pillow'),version('numpy')))";
    }
    internal static class PythonDownload {
        public const string Url="https://www.python.org/ftp/python/3.14.10/python-3.14.10-amd64.exe";
        [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]
        struct TrustFile { public uint Size;public string Path;public IntPtr File,KnownSubject; }
        [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]
        struct TrustData { public uint Size;public IntPtr Callback,Sip;public uint UI,Revocation,Choice;public IntPtr File;public uint StateAction;public IntPtr State;public string Reference;public uint Flags,Context; }
        [DllImport("wintrust.dll",ExactSpelling=true,PreserveSig=true)]
        static extern int WinVerifyTrust(IntPtr window,ref Guid action,ref TrustData data);
        internal static void VerifyPublisher(string path) {
            var file=new TrustFile { Size=(uint)Marshal.SizeOf(typeof(TrustFile)),Path=path };
            IntPtr memory=Marshal.AllocHGlobal(Marshal.SizeOf(typeof(TrustFile)));
            try {
                Marshal.StructureToPtr(file,memory,false);
                var data=new TrustData { Size=(uint)Marshal.SizeOf(typeof(TrustData)),UI=2,Choice=1,File=memory,StateAction=1,Flags=0x80 };
                Guid action=new Guid("00AAC56B-CD44-11d0-8CC2-00C04FC295EE");
                int result;
                try { result=WinVerifyTrust(new IntPtr(-1),ref action,ref data); }
                finally { data.StateAction=2;WinVerifyTrust(new IntPtr(-1),ref action,ref data); }
                if(result!=0)throw new IOException("Python's digital signature could not be verified. No installer was run. Use the official download page.");
                using(var certificate=new X509Certificate2(X509Certificate.CreateFromSignedFile(path))) {
                    if(!String.Equals(certificate.GetNameInfo(X509NameType.SimpleName,false),"Python Software Foundation",StringComparison.Ordinal))throw new IOException("Unexpected Python installer publisher. No installer was run.");
                }
            }finally { Marshal.DestroyStructure(memory,typeof(TrustFile));Marshal.FreeHGlobal(memory); }
        }
        public static string Download(string folder,Action<string> log) {
            string target=Path.Combine(folder,"python-3.14.10-amd64.exe");
            ServicePointManager.SecurityProtocol|=SecurityProtocolType.Tls12;
            log("Downloading 64-bit Python from python.org...");
            var request=(HttpWebRequest)WebRequest.Create(Url);
            request.UserAgent="AITD-Enhanced-Setup";request.AllowAutoRedirect=false;
            request.Timeout=30000;request.ReadWriteTimeout=30000;
            using(var response=(HttpWebResponse)request.GetResponse()) {
                if(response.StatusCode!=HttpStatusCode.OK||response.ContentLength>100L*1024*1024)throw new IOException("Unexpected Python download response. Use Manual downloads.");
                using(var input=response.GetResponseStream())
                using(var output=new FileStream(target,FileMode.CreateNew,FileAccess.Write,FileShare.None)) {
                    var buffer=new byte[65536];long total=0;int count;
                    while((count=input.Read(buffer,0,buffer.Length))>0) {
                        total+=count;if(total>100L*1024*1024)throw new IOException("Python download exceeded its size limit.");
                        output.Write(buffer,0,count);
                    }
                }
            }
            long size=new FileInfo(target).Length;
            if(size<1024*1024||size>100L*1024*1024)throw new IOException("Unexpected Python download size. No installer was run.");
            VerifyPublisher(target);log("Verified Python Software Foundation digital signature.");return target;
        }
    }
}
