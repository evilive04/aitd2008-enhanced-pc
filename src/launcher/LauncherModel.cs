using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;

namespace EnhancedLauncher {
    public sealed class Option {
        public string Section, Key, Label, Help, Group, Kind;
        public double Minimum, Maximum, Step;
        public string Default;
        public string Id { get { return Section + "/" + Key; } }
        public Option(string group,string section,string key,string label,string help,string kind,string def,double min=0,double max=1,double step=1) {
            Group=group;Section=section;Key=key;Label=label;Help=help;Kind=kind;Default=def;Minimum=min;Maximum=max;Step=step;
        }
    }
    public static class Catalog {
        public static readonly Option[] All = {
            new Option("Graphics","Graphics","SubsurfaceScattering","Subsurface scattering","Softer skin lighting for people and humanoid monsters.","bool","true"),
            new Option("Graphics","Graphics","GTAO","Ambient occlusion · GTAO","More natural depth in corners and where objects meet.","bool","true"),
            new Option("Graphics","Graphics","ContactShadows","Contact shadows","Short shadows near illuminated surfaces and objects.","bool","true"),
            new Option("Graphics","Graphics","HighResolutionShadows","High-resolution shadows","More detailed native shadow maps.","bool","true"),
            new Option("Graphics","Graphics","LodQualityScale","Distant model detail","1.0× matches the original game. Higher values retain detailed models farther away.","range","1.25",1,1.5,.05),
            new Option("Graphics","Graphics","ObjectDrawDistanceScale","Object draw distance","Extends the visibility of loaded models; scripted character appearances are unaffected.","range","1.25",1,1.5,.05),
            new Option("Graphics","Timing","MaxFPS","Frame rate limit","The mod's frame rate limit. The game or graphics driver may also apply a limit.","fps","0"),
            new Option("Controls","Controls","ModernCombat","Modern combat controls","Free look and updated aiming, melee and carried-object controls.","bool","true"),
            new Option("Controls","Controls","MeleeMouseFacing","Mouse-directed melee facing","Turn the hero with the camera and side-step with the movement controls while holding a melee object. Requires modern combat and the free third-person camera. Hold E to position the object; enemy lock keeps priority.","bool","true"),
            new Option("Controls","Controls","CorrectPistolConvergence","Precise pistol aiming","Aligns the weapon's direction with the aiming point.","bool","true"),
            new Option("Controls","Camera","FreeThirdPerson","Free third-person camera","Look around with the mouse independently of the character's facing direction.","bool","true"),
            new Option("Controls","Camera","DisableFixedAngles","Ignore fixed gameplay cameras","Keep the camera behind the hero in fixed-angle gameplay areas. Press Home to toggle in game. Scripted scenes retain their cameras.","bool","false"),
            new Option("Controls","Camera","SensitivityX","Camera sensitivity · horizontal","Sensitivity multiplier for the enhanced first- and third-person camera.","range","1.0",.1,4,.1),
            new Option("Controls","Camera","SensitivityY","Camera sensitivity · vertical","Sensitivity multiplier for the enhanced first- and third-person camera.","range","1.0",.1,4,.1),
            new Option("Controls","Camera","InvertY","Invert vertical look","Reverses vertical movement of the free camera.","bool","false"),
            new Option("Controls","Display","WindowsVolumeKeys","Media keys","Allows volume controls to work while playing.","bool","true"),
            new Option("Gameplay & UI","Gameplay","SafeInventory","Safe inventory and healing","Pauses enemy combat behavior while using the inventory or healing.","bool","true"),
            new Option("Gameplay & UI","Gameplay","ContinuousEpisodes","Continuous episode playback","Skip episode credits and Previously recap videos. Keep saving, Continue and Main Menu choices. Main-menu credits and episode previews remain available.","bool","false"),
            new Option("Gameplay & UI","Cutscenes","FastForward","Fast-forward cutscenes","Hold Space to speed up cinematic scenes.","bool","true"),
            new Option("Gameplay & UI","Cutscenes","FastForwardEvents","Fast-forward scripted events","Hold Space to speed up supported scripted camera sequences while control is locked. Normal gameplay and interactive events keep their original speed.","bool","true"),
            new Option("Gameplay & UI","Cutscenes","MaxSpeed","Fast-forward speed","Maximum playback multiplier while holding Space.","range","20",2,20,1),
            new Option("Gameplay & UI","HUD","Ammunition","Ammunition counter","Shows the enhanced ammunition counter while holding a pistol.","bool","true"),
            new Option("Gameplay & UI","UI","RemakeInterface","Enhanced game interface","Enables the enhanced inventory, phone and in-game menus.","bool","true"),
            new Option("Gameplay & UI","UI","RemakeMenu","Enhanced main menu","Enables the enhanced background, logo and main menu.","bool","true"),
            new Option("Gameplay & UI","UI","ModernSounds","Interface sounds","Audio feedback for the enhanced menus.","bool","true"),
            new Option("Audio","Audio","OutputChannels","Audio output","Choose Stereo / 2.1 for two speakers or headphones, or Automatic for a surround system.","audio","2"),
            new Option("Launcher","Startup","SkipIntro","Skip startup videos","Skip the Atari, Eden and opening credit videos. Keep the Press any key title screen, profile setup and story scenes.","bool","false"),
            new Option("Launcher","Launcher","CloseOnLaunch","Close launcher when the game starts","Mod preparation and crash logging continue in the background.","bool","true")
        };
        public static string Normalize(Option o,string value) {
            if(o.Kind=="bool")return Regex.IsMatch(value.Trim(),@"^(true|1|on)$",RegexOptions.IgnoreCase)?"true":"false";
            double d;if(!double.TryParse(value,NumberStyles.Float,CultureInfo.InvariantCulture,out d)||double.IsNaN(d)||double.IsInfinity(d))d=double.Parse(o.Default,CultureInfo.InvariantCulture);
            if(o.Kind=="range")d=Math.Max(o.Minimum,Math.Min(o.Maximum,d));
            if(o.Kind=="audio"&&d!=0&&d!=2)d=0;
            if(o.Kind=="fps"&&d!=0&&d!=30&&d!=60&&d!=120)d=0;
            return d.ToString("0.###",CultureInfo.InvariantCulture);
        }
    }
    public sealed class IniDocument {
        readonly List<string> lines;
        readonly Encoding encoding;
        readonly bool bom;
        readonly string newline;
        public IniDocument(byte[] data) {
            bom=false;encoding=new UTF8Encoding(false,true);
            int skip=0;
            if(data.Length>=2&&data[0]==255&&data[1]==254){encoding=new UnicodeEncoding(false,true,true);bom=true;skip=2;}
            else if(data.Length>=2&&data[0]==254&&data[1]==255){encoding=new UnicodeEncoding(true,true,true);bom=true;skip=2;}
            else if(data.Length>=3&&data[0]==239&&data[1]==187&&data[2]==191){bom=true;skip=3;}
            var text=encoding.GetString(data,skip,data.Length-skip);
            newline=text.Contains("\r\n")?"\r\n":"\n";
            lines=new List<string>(Regex.Split(text,@"\r\n|\r|\n"));
        }
        static string Section(string line) {var m=Regex.Match(line,@"^\s*\[([^\]]+)\]\s*$");return m.Success?m.Groups[1].Value.Trim():null;}
        static Match Pair(string line) {return Regex.Match(line,@"^\s*([^;#=\s][^=]*)\s*=(.*)$");}
        public string Get(string section,string key,string fallback) {
            string current="";
            foreach(var line in lines){var sec=Section(line);if(sec!=null){current=sec;continue;}
                var m=Pair(line);if(current.Equals(section,StringComparison.OrdinalIgnoreCase)&&m.Success&&m.Groups[1].Value.Trim().Equals(key,StringComparison.OrdinalIgnoreCase))return m.Groups[2].Value.Trim();}
            return fallback;
        }
        public void Set(string section,string key,string value) {
            string current="";int end=-1;bool found=false;
            for(int i=0;i<lines.Count;i++){var sec=Section(lines[i]);if(sec!=null){if(found){end=i;break;}current=sec;if(sec.Equals(section,StringComparison.OrdinalIgnoreCase))found=true;continue;}
                if(!current.Equals(section,StringComparison.OrdinalIgnoreCase))continue;
                var m=Pair(lines[i]);if(m.Success&&m.Groups[1].Value.Trim().Equals(key,StringComparison.OrdinalIgnoreCase)){
                    int equals=lines[i].IndexOf('=');lines[i]=lines[i].Substring(0,equals+1)+value;return;}}
            if(found){if(end<0)end=lines.Count;if(end>0&&lines[end-1]=="")end--;lines.Insert(end,key+"="+value);}
            else {if(lines.Count>0&&lines[lines.Count-1]!="")lines.Add("");lines.Add("["+section+"]");lines.Add(key+"="+value);lines.Add("");}
        }
        public byte[] Bytes() {var body=encoding.GetBytes(string.Join(newline,lines));if(!bom)return body;var pre=encoding is UnicodeEncoding?encoding.GetPreamble():new byte[]{239,187,191};return pre.Concat(body).ToArray();}
    }
    public sealed class Resolution : IEquatable<Resolution> {
        public int Width,Height;
        public Resolution(int width,int height){Width=width;Height=height;}
        public override string ToString(){return Width+" × "+Height;}
        public bool Equals(Resolution other){return other!=null&&Width==other.Width&&Height==other.Height;}
        public override bool Equals(object obj){return Equals(obj as Resolution);}
        public override int GetHashCode(){return Width*397^Height;}
    }
    public static class Native {
        [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)]
        public struct Mode {
            [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string Device;
            public short Spec,Driver,Size,Extra;public int Fields;
            public int X,Y,Orientation,FixedOutput;
            public short Color,Duplex,YResolution,TT,Collate;
            [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string Form;
            public short LogPixels;public int Bpp,Width,Height,Flags,Frequency,ICMMethod,ICMIntent,Media,Dither,Reserved1,Reserved2,PanW,PanH;
        }
        [DllImport("user32.dll",CharSet=CharSet.Unicode)]static extern bool EnumDisplaySettings(string device,int mode,ref Mode data);
        public static List<Resolution> Modes() {
            if(Marshal.SizeOf(typeof(Mode))!=220)throw new InvalidOperationException("Unsupported display mode format.");
            var list=new List<Resolution>();
            for(int i=0;i<10000;i++){var m=new Mode();m.Size=(short)Marshal.SizeOf(typeof(Mode));if(!EnumDisplaySettings(null,i,ref m))break;
                var r=new Resolution(m.Width,m.Height);if(m.Bpp>=24&&m.Width>=640&&m.Height>=360&&Math.Abs(m.Width*9-m.Height*16)<=16&&!list.Contains(r))list.Add(r);}
            return list.OrderByDescending(r=>r.Width).ThenByDescending(r=>r.Height).ToList();
        }
        public static Resolution Desktop() {var m=new Mode();m.Size=(short)Marshal.SizeOf(typeof(Mode));if(!EnumDisplaySettings(null,-1,ref m))return new Resolution(1920,1080);return new Resolution(m.Width,m.Height);}
        public static bool GameRunning() {
            foreach(var p in Process.GetProcessesByName("Alone")){p.Dispose();return true;}return false;
        }
        public static byte[] ReadOptional(string path){return File.Exists(path)?File.ReadAllBytes(path):null;}
        public static bool Same(byte[] a,byte[] b){return a==null?b==null:b!=null&&a.SequenceEqual(b);}
        public static string Hash(string value){using(var h=SHA256.Create())return BitConverter.ToString(h.ComputeHash(Encoding.UTF8.GetBytes(value.ToLowerInvariant()))).Replace("-","").Substring(0,24);}
        public static void AtomicWrite(string path,byte[] bytes) {
            Directory.CreateDirectory(Path.GetDirectoryName(path));string temp=path+".launcher-"+Guid.NewGuid().ToString("N")+".tmp";
            try {using(var f=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){f.Write(bytes,0,bytes.Length);f.Flush(true);}
                if(File.Exists(path))File.Replace(temp,path,null,true);else File.Move(temp,path);}
            finally {if(File.Exists(temp))File.Delete(temp);}
        }
    }
    public sealed class SettingsStore {
        public readonly string DirectoryPath,IniPath,ProfilePath;
        public byte[] InitialIni,InitialProfile;
        public readonly Dictionary<string,string> Draft=new Dictionary<string,string>();
        readonly Dictionary<string,string> baseline=new Dictionary<string,string>();
        public Resolution SelectedResolution,InitialResolution;
        public bool Borderless,InitialBorderless;
        public SettingsStore(string game,string profile){DirectoryPath=Path.GetFullPath(game);IniPath=Path.Combine(DirectoryPath,"aitd-enhanced.ini");ProfilePath=profile;Reload();}
        public static Resolution ParseResolution(byte[] data) {
            if(data==null)return null;if(data.Length>4096)throw new InvalidDataException("The resolution file has an unknown format.");
            var fields=new Dictionary<string,int>();var allowed=new[]{"ResolutionWidth","ResolutionHeight","NewResolution","PrevResolutionWidth","PrevResolutionHeight"};
            foreach(var l in new UTF8Encoding(false,true).GetString(data).TrimStart('\ufeff').Split('\n')){
                if(l.Trim()=="")continue;var m=Regex.Match(l,@"^\s*([A-Za-z]+)\s*=\s*([0-9]+)\s*$");int n;
                if(!m.Success||!allowed.Contains(m.Groups[1].Value)||fields.ContainsKey(m.Groups[1].Value)||!int.TryParse(m.Groups[2].Value,out n))throw new InvalidDataException("The resolution file has an unknown format.");
                fields.Add(m.Groups[1].Value,n);}
            if(fields.Count!=5||fields["NewResolution"]>1||fields.Any(x=>x.Key!="NewResolution"&&(x.Value<16||x.Value>16384)))throw new InvalidDataException("Incomplete resolution settings.");
            return new Resolution(fields["ResolutionWidth"],fields["ResolutionHeight"]);
        }
        public void Reload() {
            InitialIni=Native.ReadOptional(IniPath);if(InitialIni==null)throw new FileNotFoundException("aitd-enhanced.ini is missing from the launcher folder.");
            var ini=new IniDocument(InitialIni);Draft.Clear();baseline.Clear();
            foreach(var o in Catalog.All){var v=Catalog.Normalize(o,ini.Get(o.Section,o.Key,o.Default));Draft[o.Id]=v;baseline[o.Id]=v;}
            Borderless=ini.Get("Display","BorderlessFullscreen","true").Equals("true",StringComparison.OrdinalIgnoreCase)||ini.Get("Display","BorderlessFullscreen","true")=="1";InitialBorderless=Borderless;
            InitialProfile=Native.ReadOptional(ProfilePath);InitialResolution=ParseResolution(InitialProfile)??Native.Desktop();SelectedResolution=InitialResolution;
        }
        public bool Dirty {get{return Borderless!=InitialBorderless||(!Borderless&&(InitialProfile==null||!SelectedResolution.Equals(InitialResolution)))||Draft.Any(k=>k.Value!=baseline[k.Key]);}}
        public string Save(Func<bool> running,Action<string> beforeCommit=null) {
            if(running())throw new InvalidOperationException("Close the game before saving settings.");
            using(var gate=new System.Threading.Mutex(false,@"Local\AITDSettings-"+Native.Hash(DirectoryPath))) {
                if(!gate.WaitOne(0))throw new InvalidOperationException("Another launcher is currently saving settings.");
                try {
                    if(!Native.Same(Native.ReadOptional(IniPath),InitialIni)||!Native.Same(Native.ReadOptional(ProfilePath),InitialProfile))throw new InvalidOperationException("Settings have changed outside this launcher. Choose Discard changes to reload the current values.");
                    var ini=new IniDocument(InitialIni);
                    foreach(var o in Catalog.All)if(Draft[o.Id]!=baseline[o.Id])ini.Set(o.Section,o.Key,Catalog.Normalize(o,Draft[o.Id]));
                    if(Borderless!=InitialBorderless)ini.Set("Display","BorderlessFullscreen",Borderless?"true":"false");
                    byte[] nextIni=ini.Bytes(),nextProfile=InitialProfile;
                    bool resolutionChanged=!Borderless&&(InitialProfile==null||!SelectedResolution.Equals(InitialResolution));
                    if(resolutionChanged) {
                        if(!Native.Modes().Contains(SelectedResolution))throw new InvalidOperationException("The selected resolution is no longer available on the primary monitor.");
                        nextProfile=Encoding.ASCII.GetBytes(string.Format(CultureInfo.InvariantCulture,"ResolutionWidth = {0}\r\nResolutionHeight = {1}\r\nNewResolution = 0\r\nPrevResolutionWidth = {0}\r\nPrevResolutionHeight = {1}\r\n",SelectedResolution.Width,SelectedResolution.Height));
                    }
                    bool writeIni=!Native.Same(nextIni,InitialIni),writeProfile=!Native.Same(nextProfile,InitialProfile);
                    if(!writeIni&&!writeProfile)return "";
                    var backup=Path.Combine(DirectoryPath,"aitd-enhanced-backups","launcher-"+DateTime.UtcNow.ToString("yyyyMMdd-HHmmss-fffffff",CultureInfo.InvariantCulture));
                    Directory.CreateDirectory(backup);File.WriteAllBytes(Path.Combine(backup,"aitd-enhanced.ini.before"),InitialIni);
                    if(InitialProfile!=null)File.WriteAllBytes(Path.Combine(backup,"res.aitd.before"),InitialProfile);
                    File.WriteAllText(Path.Combine(backup,"location.txt"),ProfilePath,Encoding.UTF8);
                    if(beforeCommit!=null)beforeCommit(backup);
                    if(running()||!Native.Same(Native.ReadOptional(IniPath),InitialIni)||!Native.Same(Native.ReadOptional(ProfilePath),InitialProfile))throw new InvalidOperationException("The game or settings changed while saving. No changes were applied.");
                    bool profileWritten=false;
                    try {
                        if(writeProfile){Native.AtomicWrite(ProfilePath,nextProfile);profileWritten=true;}
                        if(writeIni)Native.AtomicWrite(IniPath,nextIni);
                    } catch {
                        if(profileWritten&&Native.Same(Native.ReadOptional(ProfilePath),nextProfile)){if(InitialProfile!=null)Native.AtomicWrite(ProfilePath,InitialProfile);else File.Delete(ProfilePath);}
                        throw;
                    }
                    Reload();return backup;
                } finally {gate.ReleaseMutex();}
            }
        }
    }
}
