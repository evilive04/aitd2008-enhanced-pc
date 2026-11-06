using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32.SafeHandles;

namespace EnhancedSetup {
    internal sealed class RemovalFile {
        public string Name, Hash;
        public long Bytes;
    }
    internal sealed class RemovalPlan {
        internal const string Original="d8a7370a1f4f2418cfeb2f22c0d2e9dafa59f606ff83fbb6fc8311ba84df4818";
        internal const string Self="uninstall mod.exe", Receipt=".enhanced-install.json";
        public string Root;
        public readonly List<RemovalFile> Files=new List<RemovalFile>();
        public readonly List<string> Directories=new List<string>();
        static readonly HashSet<string> Roots=new HashSet<string>(new[]{"d3d9.dll","dinput8.dll","AITD Enhanced Launcher.exe","aitd-enhanced.ini",Self},StringComparer.OrdinalIgnoreCase);
        static readonly HashSet<string> Folders=new HashSet<string>(new[]{"assets","enhanced-ui",".enhanced","mods"},StringComparer.OrdinalIgnoreCase);
        static readonly HashSet<string> ForbiddenExtensions=new HashSet<string>(new[]{".big",".fat",".bik",".aitd",".dll",".exe",".dmp"},StringComparer.OrdinalIgnoreCase);
        internal static bool HashValid(string hash) { return hash!=null&&Regex.IsMatch(hash,"\\A[0-9a-f]{64}\\z"); }
        internal static bool Allowed(string name,bool directory) {
            if(String.IsNullOrEmpty(name)||name.IndexOf('\\')>=0||Path.IsPathRooted(name))return false;
            var parts=name.Split('/');
            if(parts.Any(p=>p.Length==0||p=="."||p==".."||p.EndsWith(".",StringComparison.Ordinal)||p.EndsWith(" ",StringComparison.Ordinal)||p.IndexOfAny(Path.GetInvalidFileNameChars())>=0||Regex.IsMatch(p,@"\A(?:CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|\z)",RegexOptions.IgnoreCase)))return false;
            if(!directory&&parts.Length==1)return Roots.Contains(name);
            if(!Folders.Contains(parts[0])||(!directory&&parts.Length<2))return false;
            return directory||!ForbiddenExtensions.Contains(Path.GetExtension(name));
        }
        public static string SafePath(string root,string relative) {
            root=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar);
            string target=Path.GetFullPath(Path.Combine(root,relative.Replace('/',Path.DirectorySeparatorChar)));
            if(!target.StartsWith(root+Path.DirectorySeparatorChar,StringComparison.OrdinalIgnoreCase))throw new IOException("Removal target escapes the game folder.");
            for(string path=target;!String.IsNullOrEmpty(path);path=Path.GetDirectoryName(path)) {
                try { if((File.GetAttributes(path)&FileAttributes.ReparsePoint)!=0)throw new IOException("Links and junctions are not removal targets: "+path); }
                catch(FileNotFoundException) { }catch(DirectoryNotFoundException) { }
            }
            return target;
        }
        public static string HashFile(string path) {
            using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.Read))
            using(var hash=SHA256.Create())return BitConverter.ToString(hash.ComputeHash(file)).Replace("-","").ToLowerInvariant();
        }
        internal static Dictionary<string,object> Json(string path,int limit) {
            if(new FileInfo(path).Length>limit)throw new IOException("Removal metadata exceeds its size limit.");
            var serializer=new JavaScriptSerializer { MaxJsonLength=limit,RecursionLimit=32 };
            var result=serializer.DeserializeObject(File.ReadAllText(path,Encoding.UTF8)) as Dictionary<string,object>;
            if(result==null)throw new IOException("Invalid removal metadata.");return result;
        }
        internal static object Value(Dictionary<string,object> data,string key) { object value;return data.TryGetValue(key,out value)?value:null; }
        public static RemovalPlan Load(string root) {
            var result=new RemovalPlan { Root=Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) };
            var data=Json(SafePath(result.Root,Receipt),4*1024*1024);
            if(!Object.Equals(Value(data,"schema"),1)||!Object.Equals(Value(data,"original_exe_sha256"),Original))throw new IOException("This is not a supported Enhanced installation receipt.");
            var files=Value(data,"files") as Dictionary<string,object>;
            if(files==null||files.Count<5||files.Count>4096)throw new IOException("Invalid installed-file list.");
            var seen=new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            foreach(var item in files) {
                var entry=item.Value as Dictionary<string,object>;
                string hash=entry==null?null:Value(entry,"sha256") as string;
                object count=entry==null?null:Value(entry,"bytes");
                if(!Allowed(item.Key,false)||!seen.Add(item.Key)||!HashValid(hash)||!(count is int||count is long))throw new IOException("Unrecognized installation entry: "+item.Key);
                long length=Convert.ToInt64(count);
                if(length<0||length>256L*1024*1024)throw new IOException("Invalid installed-file size.");
                SafePath(result.Root,item.Key);result.Files.Add(new RemovalFile { Name=item.Key,Hash=hash,Bytes=length });
            }
            if(!Roots.All(seen.Contains))throw new IOException("The receipt does not include this uninstaller and the expected mod components.");
            object raw=Value(data,"created_directories");
            if(raw!=null) {
                var directories=raw as IEnumerable;
                if(directories==null||raw is string)throw new IOException("Invalid created-directory list.");
                foreach(object item in directories) {
                    string name=item as string;
                    if(result.Directories.Count>=8192||!Allowed(name,true))throw new IOException("Invalid mod directory in receipt.");
                    SafePath(result.Root,name);result.Directories.Add(name);
                }
            }
            return result;
        }
        public static void RequireClosedGame() {
            foreach(string name in new[]{"Alone","AITDRuntime","AITD Enhanced Launcher","Spustit hru","Setup"}) {
                var processes=Process.GetProcessesByName(name);
                try { if(processes.Length!=0)throw new IOException("Close the game, launcher and Setup before uninstalling."); }
                finally { foreach(var process in processes)process.Dispose(); }
            }
            var uninstallers=Process.GetProcessesByName("uninstall mod");
            try { if(uninstallers.Length>1)throw new IOException("Close the other uninstaller window before continuing."); }
            finally { foreach(var process in uninstallers)process.Dispose(); }
        }
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
        static extern SafeFileHandle CreateFile(string path,uint access,uint share,IntPtr security,uint disposition,uint flags,IntPtr template);
        [DllImport("kernel32.dll",SetLastError=true)]
        static extern bool SetFileInformationByHandle(SafeFileHandle file,int type,ref byte information,uint size);
        [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)]
        static extern uint GetFinalPathNameByHandle(SafeFileHandle file,StringBuilder path,uint size,uint flags);
        public static void DeleteOwned(string root,string relative,string expectedHash) {
            string path=SafePath(root,relative);
            if(!File.Exists(path))return;
            // Lock out writes/replacement, hash and delete the SAME opened file.
            using(var handle=CreateFile(path,0x80010000,1,IntPtr.Zero,3,0x00200000,IntPtr.Zero)) {
                if(handle.IsInvalid)throw new IOException("Could not lock mod file for removal: "+relative,new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()));
                var openedPath=new StringBuilder(32768);uint length=GetFinalPathNameByHandle(handle,openedPath,(uint)openedPath.Capacity,0);
                if(length==0||length>=openedPath.Capacity)throw new IOException("Could not verify the opened removal target.");
                string final=openedPath.ToString();if(final.StartsWith(@"\\?\",StringComparison.Ordinal))final=final.Substring(4);
                if(!String.Equals(final,path,StringComparison.OrdinalIgnoreCase))throw new IOException("Removal target was redirected; file preserved.");
                using(var file=new FileStream(handle,FileAccess.Read)) {
                    if(expectedHash!=null)using(var hash=SHA256.Create()) {
                        string actual=BitConverter.ToString(hash.ComputeHash(file)).Replace("-","").ToLowerInvariant();
                        if(actual!=expectedHash)throw new IOException("File changed during removal; it was preserved: "+relative);
                    }
                    byte delete=1;
                    if(!SetFileInformationByHandle(handle,4,ref delete,1))throw new IOException("Could not remove mod file: "+relative,new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error()));
                }
            }
        }
        void EmptyDirectory(string name) {
            string path=SafePath(Root,name);
            if(Directory.Exists(path)&&!Directory.EnumerateFileSystemEntries(path).Any())Directory.Delete(path,false);
        }
        public void RemoveCache(Action<string> log) {
            // A pre-existing/shared mods folder is never claimed by this install.
            if(!Directories.Contains("mods",StringComparer.OrdinalIgnoreCase))return;
            string cache=SafePath(Root,"mods/.cache");if(!Directory.Exists(cache))return;
            var ownPacks=new HashSet<string>(Files.Select(f=>f.Name.Split('/')).Where(p=>p.Length==3&&p[0]=="mods"&&p[2]=="mod.json").Select(p=>p[1]),StringComparer.Ordinal);
            var removed=new HashSet<string>(StringComparer.Ordinal);
            foreach(string folder in Directory.GetDirectories(cache)) {
                string id=Path.GetFileName(folder),relative="mods/.cache/"+id;
                if(!Regex.IsMatch(id,"\\A[0-9a-f]{32}(?:\\.pending-[A-Za-z0-9_-]+)?\\z"))continue;
                SafePath(Root,relative);
                if(Directory.GetDirectories(folder).Length!=0)continue;
                var paths=Directory.GetFiles(folder);
                if(paths.Any(p=>!Regex.IsMatch(Path.GetFileName(p),"\\A(?:alone_compressed_[abc]\\.(?:big|fat)|compile-report\\.json)\\z")))continue;
                foreach(string path in paths)SafePath(Root,relative+"/"+Path.GetFileName(path));
                string report=Path.Combine(folder,"compile-report.json");
                if(File.Exists(report)) {
                    var data=Json(report,4*1024*1024);
                    var packs=Value(data,"packs") as object[];
                    if(!Object.Equals(Value(data,"version"),1)||!Object.Equals(Value(data,"compiler_revision"),2)||!Object.Equals(Value(data,"generation"),id.Substring(0,32))||packs==null||packs.Length==0)continue;
                    if(packs.Any(p=>!(p is Dictionary<string,object>)||!ownPacks.Contains(Value((Dictionary<string,object>)p,"id") as string)))continue;
                }else if(id.Length==32)continue;
                foreach(string path in paths)DeleteOwned(Root,relative+"/"+Path.GetFileName(path),null);
                EmptyDirectory(relative);if(id.Length==32)removed.Add(id);log("Removed generated archive cache: "+id);
            }
            string selector=SafePath(Root,"mods/.cache/active.ini");
            if(File.Exists(selector)&&new FileInfo(selector).Length<65536) {
                string text=File.ReadAllText(selector);
                var match=Regex.Match(text,@"(?m)^Generation\s*=\s*([0-9a-f]{32})\s*$");
                if(match.Success&&removed.Contains(match.Groups[1].Value)&&text.Contains(Original))DeleteOwned(Root,"mods/.cache/active.ini",HashFile(selector));
            }
            EmptyDirectory("mods/.cache");
        }
        public int Remove(bool removeChanged,Action<string> log) {
            RequireClosedGame();int kept=0;
            RemoveCache(log);
            foreach(var item in Files.Where(f=>!String.Equals(f.Name,Self,StringComparison.OrdinalIgnoreCase))) {
                string path=SafePath(Root,item.Name);
                if(!File.Exists(path))continue;
                string actual=HashFile(path);
                if(actual!=item.Hash&&!removeChanged) { kept++;log("Kept modified mod file: "+item.Name);continue; }
                try { DeleteOwned(Root,item.Name,actual);log("Removed: "+item.Name); }
                catch(IOException error) { kept++;log(error.Message); }
            }
            foreach(string directory in Directories.OrderByDescending(x=>x.Length)) {
                try { EmptyDirectory(directory); }catch(IOException error) { log("Kept directory: "+error.Message); }
            }
            return kept;
        }
        static string Ps(string text) { return "'"+text.Replace("'","''")+"'"; }
        public void FinishAfterExit() {
            string self=SafePath(Root,Self),receipt=SafePath(Root,Receipt);
            string selfHash=HashFile(self),receiptHash=HashFile(receipt);
            string folder=Path.Combine(Path.GetTempPath(),"aitd-uninstall-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(folder);
            string script=Path.Combine(folder,"finish.ps1");
            var code=new StringBuilder();
            code.AppendLine("$ErrorActionPreference = 'Stop'");
            code.AppendLine("$game = "+Ps(Root));code.AppendLine("$selfHash = "+Ps(selfHash));code.AppendLine("$receiptHash = "+Ps(receiptHash));
            code.AppendLine("$ownedScript = "+Ps(script));code.AppendLine("$ownedFolder = "+Ps(folder));
            using(var current=Process.GetCurrentProcess()) {
                code.AppendLine("$parentId = "+current.Id);code.AppendLine("$parentTicks = "+current.StartTime.ToUniversalTime().Ticks);
            }
            code.AppendLine(@"function Check-Plain([string]$target) {
    $checked = [IO.Path]::GetFullPath($target)
    while ($checked) {
        try { $attributes = [IO.File]::GetAttributes($checked) }
        catch [IO.FileNotFoundException] { $attributes = 0 }
        catch [IO.DirectoryNotFoundException] { $attributes = 0 }
        if ($attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Link target rejected' }
        $checked = [IO.Path]::GetDirectoryName($checked)
    }
}
function Hash-File([string]$target) {
    $stream = [IO.File]::Open($target, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
    $hash = [Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($hash.ComputeHash($stream)).Replace('-', '').ToLowerInvariant() }
    finally { $hash.Dispose(); $stream.Dispose() }
}
try {
    Check-Plain $game; Check-Plain $ownedFolder
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd([char]92)
    if ([IO.Path]::GetDirectoryName($ownedFolder) -ne $tempRoot -or [IO.Path]::GetFileName($ownedFolder) -notmatch '^aitd-uninstall-[0-9a-f]{32}$') { throw 'Unexpected helper folder' }
    $parent = $null
    try { $parent = [Diagnostics.Process]::GetProcessById($parentId) } catch [ArgumentException] { }
    if ($parent) {
        try {
            if ($parent.StartTime.ToUniversalTime().Ticks -eq $parentTicks -and -not $parent.WaitForExit(60000)) { throw 'Uninstaller is still running' }
        } finally { $parent.Dispose() }
    }
    $selfPath = [IO.Path]::GetFullPath([IO.Path]::Combine($game, 'uninstall mod.exe'))
    $receiptPath = [IO.Path]::GetFullPath([IO.Path]::Combine($game, '.enhanced-install.json'))
    if ([IO.Path]::GetDirectoryName($selfPath) -ne $game -or [IO.Path]::GetDirectoryName($receiptPath) -ne $game) { throw 'Unexpected removal targets' }
    Check-Plain $selfPath; Check-Plain $receiptPath
    if ((Hash-File $selfPath) -ne $selfHash) { throw 'Uninstaller changed' }
    if ((Hash-File $receiptPath) -ne $receiptHash) { throw 'Receipt changed' }
    [IO.File]::Delete($selfPath)
    [IO.File]::Delete($receiptPath)
} catch {
    [IO.File]::WriteAllText([IO.Path]::Combine($ownedFolder, 'cleanup-error.txt'), $_.Exception.Message)
    exit 1
}
Check-Plain $ownedScript
[IO.File]::Delete($ownedScript)
if ([IO.Directory]::GetFileSystemEntries($ownedFolder).Length -eq 0) { [IO.Directory]::Delete($ownedFolder, $false) }
");
            using(var file=new FileStream(script,FileMode.CreateNew,FileAccess.Write,FileShare.Read))
            using(var writer=new StreamWriter(file,new UTF8Encoding(true)))writer.Write(code.ToString());
            string powershell=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),@"WindowsPowerShell\v1.0\powershell.exe");
            var info=new ProcessStartInfo { FileName=powershell,Arguments="-NoProfile -NonInteractive -ExecutionPolicy Bypass -File "+WindowsArguments.Quote(script),UseShellExecute=false,CreateNoWindow=true,WindowStyle=ProcessWindowStyle.Hidden };
            using(var child=Process.Start(info)) { if(child==null)throw new IOException("Could not start final cleanup. The receipt and uninstaller remain available."); }
        }
    }

    internal sealed class UninstallWindow : Form {
        readonly Button remove=new Button { Text="Uninstall mod",AutoSize=true,Padding=new Padding(14,7,14,7) };
        readonly Button close=new Button { Text="Close",AutoSize=true,Padding=new Padding(14,7,14,7) };
        readonly CheckBox changed=new CheckBox { Text="Also remove modified mod files (including my mod settings)",AutoSize=true };
        readonly TextBox output=new TextBox { Multiline=true,ReadOnly=true,ScrollBars=ScrollBars.Vertical,Dock=DockStyle.Fill };
        bool busy;
        public UninstallWindow() {
            Text="Alone in the Dark - Uninstall mod";Font=new Font("Segoe UI",10F);AutoScaleDimensions=new SizeF(96,96);AutoScaleMode=AutoScaleMode.Dpi;
            ClientSize=new Size(760,480);MinimumSize=new Size(650,410);StartPosition=FormStartPosition.CenterScreen;
            BackColor=Color.FromArgb(18,21,23);ForeColor=Color.FromArgb(235,230,219);
            var root=new TableLayoutPanel { Dock=DockStyle.Fill,Padding=new Padding(24),ColumnCount=1,RowCount=5,AutoScroll=true };
            root.RowStyles.Add(new RowStyle(SizeType.AutoSize));root.RowStyles.Add(new RowStyle(SizeType.AutoSize));root.RowStyles.Add(new RowStyle(SizeType.AutoSize));root.RowStyles.Add(new RowStyle(SizeType.Percent,100));root.RowStyles.Add(new RowStyle(SizeType.AutoSize));
            root.Controls.Add(new Label { Text="UNINSTALL ENHANCED PC EDITION",AutoSize=true,Font=new Font("Segoe UI",15F,FontStyle.Bold),ForeColor=Color.FromArgb(218,174,101),Margin=new Padding(0,0,0,14) },0,0);
            root.Controls.Add(new Label { Text="Remove the recorded mod files and its own generated archive cache.\r\nOriginal game files, activation and saved games are preserved.\r\nPython and downloaded requirements remain installed.",AutoSize=true,Margin=new Padding(0,0,0,14) },0,1);
            changed.Margin=new Padding(0,0,0,14);root.Controls.Add(changed,0,2);
            output.BackColor=Color.FromArgb(29,33,36);output.ForeColor=ForeColor;root.Controls.Add(output,0,3);
            var buttons=new FlowLayoutPanel { Dock=DockStyle.Fill,AutoSize=true,FlowDirection=FlowDirection.RightToLeft,Padding=new Padding(0,14,0,0) };buttons.Controls.Add(close);buttons.Controls.Add(remove);root.Controls.Add(buttons,0,4);Controls.Add(root);
            close.Click+=delegate { Close(); };remove.Click+=RemoveClicked;
            FormClosing+=delegate(object sender,FormClosingEventArgs e) { if(busy)e.Cancel=true; };
            output.Text="Folder: "+Path.GetFullPath(AppDomain.CurrentDomain.BaseDirectory)+Environment.NewLine+"Modified files are kept unless the option above is selected. Unknown files and other mods are kept. The uninstaller removes itself after successful cleanup.";
        }
        void Log(string message) {
            if(InvokeRequired) { BeginInvoke(new Action<string>(Log),message);return; }
            if(output.TextLength>200000)output.Text=output.Text.Substring(output.TextLength-100000);
            output.AppendText(Environment.NewLine+message);
        }
        async void RemoveClicked(object sender,EventArgs e) {
            if(busy)return;
            if(MessageBox.Show(this,"Remove Enhanced PC Edition from this game folder?\r\n\r\nYour original game and saved games will be kept.","Uninstall mod",MessageBoxButtons.YesNo,MessageBoxIcon.Question,MessageBoxDefaultButton.Button2)!=DialogResult.Yes)return;
            busy=true;remove.Enabled=false;close.Enabled=false;changed.Enabled=false;
            try {
                var plan=RemovalPlan.Load(AppDomain.CurrentDomain.BaseDirectory);bool removeChanges=changed.Checked;
                int kept=await Task.Run(()=>plan.Remove(removeChanges,Log));
                if(kept!=0)Log("Some modified or locked files were kept. The receipt and uninstaller remain so you can review and retry.");
                else {
                    MessageBox.Show(this,"Recorded mod files were removed.\r\n\r\nAfter this window closes, Setup will remove the uninstaller and its receipt. Unknown/custom files, Python and saved games are kept.","Uninstall complete",MessageBoxButtons.OK,MessageBoxIcon.Information);
                    plan.FinishAfterExit();busy=false;Close();return;
                }
            }catch(Exception error) { Log(error.Message);MessageBox.Show(this,error.Message,"Uninstall could not complete",MessageBoxButtons.OK,MessageBoxIcon.Warning); }
            busy=false;remove.Enabled=true;close.Enabled=true;changed.Enabled=true;
        }
    }
}
